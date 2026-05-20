#include "../internal/foreign_injury_internal.h"
#include "../../../core/csv/core_csv.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/keys/runtime_flag_keys.generated.h"
#include "../records/sql/foreign_injury_replacements_sql_store.h"

static int kbo_foreign_injury_persisted_date_span_plausible(
    uint32_t opened_on,
    uint32_t expected_end)
{
    if (opened_on == 0u || expected_end == 0u) {
        return 1;
    }

    uint32_t open_serial = kbo_date_serial(
        opened_on / 10000u,
        (opened_on / 100u) % 100u,
        opened_on % 100u);
    uint32_t end_serial = kbo_date_serial(
        expected_end / 10000u,
        (expected_end / 100u) % 100u,
        expected_end % 100u);
    if (open_serial == 0u || end_serial == 0u || end_serial < open_serial) {
        return 0;
    }

    return end_serial - open_serial <= 730u;
}

int kbo_foreign_injury_replacement_enabled(void)
{
    return kbo_fix_enabled() && !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_FOREIGN_INJURY_REPLACEMENT_FILE);
}

int kbo_foreign_injury_status_uses_slot(uint8_t status)
{
    return status == KBO_FOREIGN_INJURY_STATUS_OPEN || status == KBO_FOREIGN_INJURY_STATUS_ACTIVE;
}

uint8_t kbo_foreign_injury_slot_type_for_player(uint8_t* player)
{
    return kbo_player_is_asian_quota_candidate(player)
        ? KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
        : KBO_FOREIGN_INJURY_SLOT_REGULAR;
}

static void kbo_foreign_injury_normalize_record_org_team(KboForeignInjuryReplacement* rec)
{
    if (rec == NULL || rec->team_id == 0u) {
        return;
    }

    uint32_t original_team_id = rec->team_id;
    uint32_t org_team_id = kbo_org_team_id_for_team_id(original_team_id);
    if (org_team_id != 0u) {
        rec->team_id = org_team_id;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(rec->team_id, 1);
    if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (league_id != 0u && (rec->league_id == 0u || rec->team_id != original_team_id)) {
            rec->league_id = league_id;
        }
    }
}

void kbo_lock_foreign_injury_replacements(void)
{
    kbo_lock_enter(&g_kbo_foreign_injury_replacement_lock);
}

void kbo_unlock_foreign_injury_replacements(void)
{
    kbo_lock_leave(&g_kbo_foreign_injury_replacement_lock);
}

int kbo_load_foreign_injury_replacements_locked(const char* path)
{
    g_kbo_foreign_injury_replacement_count = 0;
    if (path == NULL || path[0] == '\0') {
        g_kbo_foreign_injury_replacement_loaded_path[0] = '\0';
        return 0;
    }
    snprintf(g_kbo_foreign_injury_replacement_loaded_path, sizeof(g_kbo_foreign_injury_replacement_loaded_path), "%s", path);

    KboForeignInjuryReplacement loaded_records[KBO_FOREIGN_INJURY_REPLACEMENT_MAX];
    memset(loaded_records, 0, sizeof(loaded_records));
    int loaded_count = 0;
    if (!kbo_foreign_injury_replacements_sql_load(
            loaded_records,
            KBO_FOREIGN_INJURY_REPLACEMENT_MAX,
            &loaded_count)) {
        return 0;
    }

    int loaded = 0;
    int invalid_date_span = 0;
    for (int i = 0; i < loaded_count && g_kbo_foreign_injury_replacement_count < KBO_FOREIGN_INJURY_REPLACEMENT_MAX; i++) {
        KboForeignInjuryReplacement rec = loaded_records[i];
        if (rec.team_id == 0u
                || rec.injured_player_id == 0u
                || rec.slot_type < KBO_FOREIGN_INJURY_SLOT_REGULAR
                || rec.slot_type > KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
                || rec.status < KBO_FOREIGN_INJURY_STATUS_OPEN
                || rec.status > KBO_FOREIGN_INJURY_STATUS_CLOSED) {
            continue;
        }
        if (!kbo_foreign_injury_persisted_date_span_plausible(
                rec.opened_on_yyyymmdd,
                rec.expected_end_yyyymmdd)) {
            invalid_date_span++;
            if (invalid_date_span <= 20) {
                kbo_log_runtimef(
                    "foreign injury replacement: discarded persisted record with invalid date span team=%u injured=%u opened=%u expected_end=%u status=%u path=%s",
                    rec.team_id,
                    rec.injured_player_id,
                    rec.opened_on_yyyymmdd,
                    rec.expected_end_yyyymmdd,
                    (uint32_t)rec.status,
                    path);
            }
            continue;
        }
        if (rec.status != KBO_FOREIGN_INJURY_STATUS_CLOSED) {
            rec.closed_on_yyyymmdd = 0u;
            rec.close_choice = 0u;
        } else if (rec.close_choice == 0u) {
            rec.close_choice = rec.converted
                ? KBO_FOREIGN_INJURY_CLOSE_KEEP_REPLACEMENT
                : KBO_FOREIGN_INJURY_CLOSE_KEEP_INJURED;
        }
        kbo_foreign_injury_normalize_record_org_team(&rec);
        g_kbo_foreign_injury_replacements[g_kbo_foreign_injury_replacement_count++] = rec;
        loaded++;
    }

    kbo_log_runtimef(
        "foreign injury replacement: loaded=%d invalid_date_span=%d path=%s",
        loaded,
        invalid_date_span,
        path);
    return loaded;
}

static int kbo_finalize_foreign_injury_replacement_seed(
    uint32_t today,
    KboForeignInjuryReplacement* out)
{
    uint32_t injured_team_id = 0u;
    uint32_t injured_league_id = 0u;
    uint32_t replacement_team_id = 0u;
    uint32_t replacement_league_id = 0u;
    uint8_t* injured = kbo_find_player_by_id(out->injured_player_id, &injured_team_id, &injured_league_id);
    uint8_t* replacement = kbo_find_player_by_id(out->replacement_player_id, &replacement_team_id, &replacement_league_id);

    if (out->team_id == 0u) {
        out->team_id = injured_team_id != 0u ? injured_team_id : replacement_team_id;
    }
    if (out->league_id == 0u) {
        out->league_id = injured_league_id != 0u ? injured_league_id : replacement_league_id;
    }
    if (out->league_id == 0u && out->team_id != 0u) {
        uint8_t* team = find_kbo_team_by_numeric_id_any_league(out->team_id, 1);
        if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            out->league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        }
    }
    kbo_foreign_injury_normalize_record_org_team(out);

    if (out->opened_on_yyyymmdd == 0u) {
        out->opened_on_yyyymmdd = today;
    }
    if ((out->slot_type < KBO_FOREIGN_INJURY_SLOT_REGULAR || out->slot_type > KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA)) {
        if (injured != NULL && memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
            out->slot_type = kbo_foreign_injury_slot_type_for_player(injured);
        } else if (replacement != NULL && memory_range_readable(replacement, OOTP27_PLAYER_SCAN_BYTES)) {
            out->slot_type = kbo_foreign_injury_slot_type_for_player(replacement);
        } else {
            out->slot_type = KBO_FOREIGN_INJURY_SLOT_REGULAR;
        }
    }
    if (out->status < KBO_FOREIGN_INJURY_STATUS_OPEN || out->status > KBO_FOREIGN_INJURY_STATUS_CLOSED) {
        out->status = out->replacement_player_id != 0u ? KBO_FOREIGN_INJURY_STATUS_ACTIVE : KBO_FOREIGN_INJURY_STATUS_OPEN;
    }
    if (out->replacement_player_id == 0u && out->status == KBO_FOREIGN_INJURY_STATUS_ACTIVE) {
        out->status = KBO_FOREIGN_INJURY_STATUS_OPEN;
    }
    if (out->status != KBO_FOREIGN_INJURY_STATUS_CLOSED) {
        out->closed_on_yyyymmdd = 0u;
        out->close_choice = 0u;
    } else if (out->close_choice == 0u) {
        out->close_choice = out->converted
            ? KBO_FOREIGN_INJURY_CLOSE_KEEP_REPLACEMENT
            : KBO_FOREIGN_INJURY_CLOSE_KEEP_INJURED;
    }
    if (injured != NULL && memory_range_readable(injured, OOTP27_PLAYER_SCAN_BYTES)) {
        KboForeignInjuryLiveMemory live_injury;
        memset(&live_injury, 0, sizeof(live_injury));
        kbo_foreign_injury_read_live_memory(injured, &live_injury);
        if (out->injury_id == 0u && live_injury.injury_id != 0u) {
            out->injury_id = live_injury.injury_id;
        }
        if (out->expected_end_yyyymmdd == 0u
                && live_injury.active != 0u
                && live_injury.days_left > 0
                && today != 0u) {
            out->expected_end_yyyymmdd = kbo_add_days_yyyymmdd(today, (uint32_t)live_injury.days_left);
        }
    }
    if (out->expected_end_yyyymmdd == 0u
            && today != 0u
            && kbo_foreign_injury_status_uses_slot(out->status)) {
        int min_days = kbo_foreign_player_policy()->injury_replacement_min_days;
        if (min_days > 0) {
            out->expected_end_yyyymmdd = kbo_add_days_yyyymmdd(today, (uint32_t)min_days);
        }
    }

    return out->team_id != 0u;
}

static int kbo_parse_foreign_injury_replacement_seed_fields(
    char fields[][96],
    int field_count,
    uint32_t today,
    KboForeignInjuryReplacement* out)
{
    if (fields == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    if (field_count <= 0
            || fields[0][0] == '\0'
            || fields[0][0] == '#'
            || fields[0][0] == ';') {
        return 0;
    }

    if (kbo_parse_foreign_injury_replacement_key_seed_fields(fields, field_count, today, out)) {
        return kbo_finalize_foreign_injury_replacement_seed(today, out);
    }

    uint32_t values[12] = {0};
    int count = 0;
    while (count < 12 && count < field_count) {
        if (fields[count][0] < '0' || fields[count][0] > '9') {
            break;
        }
        values[count] = kbo_csv_parse_u32_text(fields[count], 10);
        count++;
    }

    if (count < 3) {
        return 0;
    }

    if (count >= 9) {
        out->team_id = values[0];
        out->league_id = values[1];
        out->injured_player_id = values[2];
        out->replacement_player_id = values[3];
        out->opened_on_yyyymmdd = values[4];
        out->expected_end_yyyymmdd = values[5];
        out->slot_type = (uint8_t)values[6];
        out->status = (uint8_t)values[7];
        out->converted = values[8] ? 1u : 0u;
        out->injury_id = count >= 10 ? values[9] : 0u;
        out->closed_on_yyyymmdd = count >= 11 ? values[10] : 0u;
        out->close_choice = count >= 12 ? (uint8_t)values[11] : 0u;
    } else {
        out->team_id = values[0];
        out->injured_player_id = values[1];
        out->replacement_player_id = values[2];
        out->slot_type = (count >= 4) ? (uint8_t)values[3] : 0u;
        out->status = (count >= 5) ? (uint8_t)values[4] : 0u;
        out->opened_on_yyyymmdd = today;
    }

    if (out->injured_player_id == 0u) {
        return 0;
    }

    return kbo_finalize_foreign_injury_replacement_seed(today, out);
}

int kbo_parse_foreign_injury_replacement_seed_line(
    const char* line,
    uint32_t today,
    KboForeignInjuryReplacement* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    const char* p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0' || *p == '\r' || *p == '\n' || *p == '#' || *p == ';') {
        return 0;
    }

    char fields[9][96];
    int field_count = kbo_csv_read_trimmed_line_fields(p, (char*)fields, sizeof(fields[0]), 9);
    return kbo_parse_foreign_injury_replacement_seed_fields(fields, field_count, today, out);
}

int kbo_import_foreign_injury_replacement_seed_file_locked(
    const char* path,
    uint32_t today,
    const char* source)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int imported = 0;
    int skipped = 0;
    int parse_failed = 0;
    while (kbo_csv_reader_next_row(reader)) {
        char fields[12][96];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 12);

        KboForeignInjuryReplacement rec;
        if (kbo_parse_foreign_injury_replacement_seed_fields(fields, field_count, today, &rec)) {
            int existing = kbo_find_foreign_injury_replacement_locked(rec.injured_player_id, 1);
            if (existing >= 0) {
                KboForeignInjuryReplacement* existing_rec = &g_kbo_foreign_injury_replacements[existing];
                uint32_t repaired_expected_end = 0u;
                if (existing_rec->opened_on_yyyymmdd != 0u) {
                    int min_days = kbo_foreign_player_policy()->injury_replacement_min_days;
                    if (min_days > 0) {
                        repaired_expected_end = kbo_add_days_yyyymmdd(
                            existing_rec->opened_on_yyyymmdd,
                            (uint32_t)min_days);
                    }
                }
                if (existing_rec->status == KBO_FOREIGN_INJURY_STATUS_CLOSED
                        && existing_rec->expected_end_yyyymmdd == 0u
                        && existing_rec->converted == 0u
                        && repaired_expected_end != 0u
                        && (today == 0u || today < repaired_expected_end)
                        && kbo_foreign_injury_status_uses_slot(rec.status)) {
                    rec.opened_on_yyyymmdd = existing_rec->opened_on_yyyymmdd;
                    rec.expected_end_yyyymmdd = repaired_expected_end;
                    *existing_rec = rec;
                    imported++;
                } else {
                    skipped++;
                }
            } else if (g_kbo_foreign_injury_replacement_count < KBO_FOREIGN_INJURY_REPLACEMENT_MAX) {
                g_kbo_foreign_injury_replacements[g_kbo_foreign_injury_replacement_count++] = rec;
                imported++;
            } else {
                skipped++;
            }
        } else if (field_count > 0 && fields[0][0] != '\0' && fields[0][0] != '#') {
            parse_failed++;
        }
    }

    kbo_csv_reader_close(reader);
    if (imported > 0 || skipped > 0 || parse_failed > 0) {
        kbo_log_runtimef(
            "foreign injury replacement: seed import source=%s imported=%d skipped=%d parse_failed=%d path=%s",
            source != NULL ? source : "",
            imported,
            skipped,
            parse_failed,
            path);
    }
    return imported;
}

