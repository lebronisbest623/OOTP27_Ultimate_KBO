#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "fa_declaration_sql_store.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../core/logging/core_log.h"
#include "../../core/sql/escape/core_sql_escape.h"
#include "../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboFaDeclarationDecisionSqlResult {
    int found;
    KboFaDeclarationDecision decision;
} KboFaDeclarationDecisionSqlResult;

typedef struct KboFaDeclarationReportSqlContext {
    KboFaDeclarationReportRow* rows;
    int capacity;
    int count;
    int overflowed;
} KboFaDeclarationReportSqlContext;

static int kbo_fa_declaration_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS fa_declarations ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "declaration_date INTEGER NOT NULL,"
        "season INTEGER NOT NULL,"
        "player_id INTEGER NOT NULL,"
        "player_name TEXT NOT NULL DEFAULT '',"
        "declared INTEGER NOT NULL DEFAULT 0,"
        "team_id INTEGER NOT NULL DEFAULT 0,"
        "league_id INTEGER NOT NULL DEFAULT 0,"
        "nation_id INTEGER NOT NULL DEFAULT 0,"
        "age INTEGER NOT NULL DEFAULT 0,"
        "contract_level INTEGER NOT NULL DEFAULT 0,"
        "salary INTEGER NOT NULL DEFAULT 0,"
        "fa_demand INTEGER NOT NULL DEFAULT 0,"
        "score INTEGER NOT NULL DEFAULT 0,"
        "threshold_value INTEGER NOT NULL DEFAULT 0,"
        "grade TEXT NOT NULL DEFAULT '',"
        "case_label TEXT NOT NULL DEFAULT '',"
        "overall INTEGER NOT NULL DEFAULT 0,"
        "talent INTEGER NOT NULL DEFAULT 0,"
        "ratings INTEGER NOT NULL DEFAULT 0,"
        "career INTEGER NOT NULL DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "reason TEXT NOT NULL DEFAULT '',"
        "decision_reason TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_fa_declarations_player_season_date "
        "ON fa_declarations(player_id, season, declaration_date);"
        "CREATE INDEX IF NOT EXISTS idx_fa_declarations_season_date "
        "ON fa_declarations(season, declaration_date);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('fa_declarations', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "fa_declarations_schema");
}

int kbo_fa_declaration_sql_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static uint32_t kbo_fa_declaration_sql_u32(char** vals, int index)
{
    unsigned int value = 0u;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%u", &value);
    }
    return (uint32_t)value;
}

static int32_t kbo_fa_declaration_sql_i32(char** vals, int index)
{
    int value = 0;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%d", &value);
    }
    return (int32_t)value;
}

static void kbo_fa_declaration_sql_text(char** vals, int index, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (vals != NULL && vals[index] != NULL) {
        snprintf(out, out_size, "%s", vals[index]);
    }
}

static int kbo_fa_declaration_sql_append(
    char* out,
    size_t out_size,
    size_t* cursor,
    const char* fmt,
    ...)
{
    if (out == NULL || cursor == NULL || fmt == NULL || *cursor >= out_size) {
        return 0;
    }
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(out + *cursor, out_size - *cursor, fmt, args);
    va_end(args);
    if (len < 0 || (size_t)len >= out_size - *cursor) {
        return 0;
    }
    *cursor += (size_t)len;
    return 1;
}

int kbo_fa_declaration_sql_append_candidates(
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    const char* source)
{
    if (candidate_count < 0 || (candidate_count > 0 && candidates == NULL)
            || !kbo_fa_declaration_sql_ensure_schema("fa_declarations_append_schema")) {
        return 0;
    }

    size_t sql_size = 1024u + ((size_t)candidate_count * 2300u);
    char* sql = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sql_size);
    if (sql == NULL) {
        kbo_log_runtime_line("KBO FA declarations sqlite append failed reason=alloc_sql");
        return 0;
    }

    char escaped_source[128] = {0};
    if (!kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "fa_declaration_event")) {
        HeapFree(GetProcessHeap(), 0, sql);
        return 0;
    }

    size_t cursor = 0u;
    int ok = kbo_fa_declaration_sql_append(sql, sql_size, &cursor, "BEGIN IMMEDIATE;");
    for (int i = 0; ok && i < candidate_count; i++) {
        const KboFaDeclarationCandidate* c = &candidates[i];
        if (c->player_id == 0u) {
            continue;
        }

        char player_name[224] = {0};
        char grade[48] = {0};
        char case_label[112] = {0};
        char reason[448] = {0};
        char decision_reason[384] = {0};
        if (!kbo_sql_escape_literal(player_name, sizeof(player_name), c->player_name)
                || !kbo_sql_escape_literal(grade, sizeof(grade), c->grade)
                || !kbo_sql_escape_literal(case_label, sizeof(case_label), c->case_label)
                || !kbo_sql_escape_literal(reason, sizeof(reason), c->reason)
                || !kbo_sql_escape_literal(decision_reason, sizeof(decision_reason), c->decision_reason)) {
            ok = 0;
            break;
        }

        ok = kbo_fa_declaration_sql_append(
            sql,
            sql_size,
            &cursor,
            "INSERT INTO fa_declarations("
            "declaration_date, season, player_id, player_name, declared, team_id, league_id, "
            "nation_id, age, contract_level, salary, fa_demand, score, threshold_value, "
            "grade, case_label, overall, talent, ratings, career, source, reason, decision_reason, created_at"
            ") VALUES(%u, %u, %u, '%s', %u, %u, %u, %u, %u, %u, %d, %d, %d, %d, "
            "'%s', '%s', %d, %d, %d, %d, '%s', '%s', '%s', datetime('now'));",
            c->declaration_date,
            c->season,
            c->player_id,
            player_name,
            (uint32_t)c->declared,
            c->team_id,
            c->league_id,
            c->nation_id,
            (uint32_t)c->age,
            (uint32_t)c->contract_level,
            c->salary,
            c->fa_demand,
            c->score,
            c->threshold,
            grade,
            case_label,
            (int)c->overall,
            (int)c->talent,
            (int)c->ratings,
            (int)c->career,
            escaped_source,
            reason,
            decision_reason);
    }
    if (ok) {
        ok = kbo_fa_declaration_sql_append(sql, sql_size, &cursor, "COMMIT;");
    }
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, sql);
        kbo_log_runtimef(
            "KBO FA declarations sqlite append failed reason=sql_buffer_full rows=%d",
            candidate_count);
        return 0;
    }

    ok = kbo_save_state_exec(sql, "fa_declarations_append");
    HeapFree(GetProcessHeap(), 0, sql);
    return ok;
}

static int kbo_fa_declaration_sql_decision_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboFaDeclarationDecisionSqlResult* result = (KboFaDeclarationDecisionSqlResult*)user_data;
    if (result == NULL || vals == NULL || ncols < 10) {
        return 0;
    }
    result->decision.player_id = kbo_fa_declaration_sql_u32(vals, 0);
    result->decision.declaration_date = kbo_fa_declaration_sql_u32(vals, 1);
    result->decision.season = kbo_fa_declaration_sql_u32(vals, 2);
    result->decision.declared = kbo_fa_declaration_sql_u32(vals, 3);
    result->decision.team_id = kbo_fa_declaration_sql_u32(vals, 4);
    result->decision.league_id = kbo_fa_declaration_sql_u32(vals, 5);
    result->decision.contract_level = (uint8_t)(kbo_fa_declaration_sql_u32(vals, 6) & 0xffu);
    result->decision.salary = kbo_fa_declaration_sql_i32(vals, 7);
    result->decision.fa_demand = kbo_fa_declaration_sql_i32(vals, 8);
    result->decision.score = kbo_fa_declaration_sql_i32(vals, 9);
    result->found = result->decision.player_id != 0u;
    return 0;
}

int kbo_fa_declaration_sql_find_latest_decision(
    uint32_t player_id,
    uint32_t season,
    KboFaDeclarationDecision* out_decision)
{
    if (out_decision != NULL) {
        memset(out_decision, 0, sizeof(*out_decision));
    }
    if (player_id == 0u || out_decision == NULL
            || !kbo_fa_declaration_sql_ensure_schema("fa_declarations_find_schema")) {
        return 0;
    }

    char sql[512] = {0};
    int len = 0;
    if (season != 0u) {
        len = snprintf(
            sql,
            sizeof(sql),
            "SELECT player_id, declaration_date, season, declared, team_id, league_id, "
            "contract_level, salary, fa_demand, score "
            "FROM fa_declarations WHERE player_id=%u AND season=%u "
            "ORDER BY declaration_date DESC, id DESC LIMIT 1;",
            player_id,
            season);
        if (len <= 0 || len >= (int)sizeof(sql)) {
            return 0;
        }
    } else {
        len = snprintf(
            sql,
            sizeof(sql),
            "SELECT player_id, declaration_date, season, declared, team_id, league_id, "
            "contract_level, salary, fa_demand, score "
            "FROM fa_declarations WHERE player_id=%u "
            "ORDER BY declaration_date DESC, id DESC LIMIT 1;",
            player_id);
        if (len <= 0 || len >= (int)sizeof(sql)) {
            return 0;
        }
    }

    KboFaDeclarationDecisionSqlResult result = {0};
    if (!kbo_save_state_query(sql, kbo_fa_declaration_sql_decision_cb, &result, "fa_declarations_find_latest")) {
        return 0;
    }
    if (!result.found) {
        return 0;
    }
    *out_decision = result.decision;
    return 1;
}

static int kbo_fa_declaration_sql_report_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboFaDeclarationReportSqlContext* ctx = (KboFaDeclarationReportSqlContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 23) {
        return 0;
    }
    if (ctx->count >= ctx->capacity) {
        ctx->overflowed++;
        return 0;
    }

    KboFaDeclarationReportRow row = {0};
    row.declaration_date = kbo_fa_declaration_sql_u32(vals, 0);
    row.season = kbo_fa_declaration_sql_u32(vals, 1);
    row.player_id = kbo_fa_declaration_sql_u32(vals, 2);
    kbo_fa_declaration_sql_text(vals, 3, row.player_name, sizeof(row.player_name));
    row.declared = kbo_fa_declaration_sql_u32(vals, 4);
    row.team_id = kbo_fa_declaration_sql_u32(vals, 5);
    row.league_id = kbo_fa_declaration_sql_u32(vals, 6);
    row.nation_id = kbo_fa_declaration_sql_u32(vals, 7);
    row.age = (uint16_t)(kbo_fa_declaration_sql_u32(vals, 8) & 0xffffu);
    row.contract_level = (uint8_t)(kbo_fa_declaration_sql_u32(vals, 9) & 0xffu);
    row.salary = kbo_fa_declaration_sql_i32(vals, 10);
    row.fa_demand = kbo_fa_declaration_sql_i32(vals, 11);
    row.score = kbo_fa_declaration_sql_i32(vals, 12);
    row.threshold = kbo_fa_declaration_sql_i32(vals, 13);
    kbo_fa_declaration_sql_text(vals, 14, row.grade, sizeof(row.grade));
    kbo_fa_declaration_sql_text(vals, 15, row.case_label, sizeof(row.case_label));
    row.overall = (int16_t)kbo_fa_declaration_sql_i32(vals, 16);
    row.talent = (int16_t)kbo_fa_declaration_sql_i32(vals, 17);
    row.ratings = (int16_t)kbo_fa_declaration_sql_i32(vals, 18);
    row.career = (int16_t)kbo_fa_declaration_sql_i32(vals, 19);
    kbo_fa_declaration_sql_text(vals, 20, row.source, sizeof(row.source));
    kbo_fa_declaration_sql_text(vals, 21, row.reason, sizeof(row.reason));
    kbo_fa_declaration_sql_text(vals, 22, row.decision_reason, sizeof(row.decision_reason));
    if (row.player_id == 0u || row.season == 0u) {
        return 0;
    }
    ctx->rows[ctx->count++] = row;
    return 0;
}

int kbo_fa_declaration_sql_load_report_rows(
    KboFaDeclarationReportRow* rows,
    int max_rows,
    int* out_count)
{
    if (rows == NULL || max_rows <= 0 || out_count == NULL) {
        return 0;
    }
    *out_count = 0;
    if (!kbo_fa_declaration_sql_ensure_schema("fa_declarations_report_schema")) {
        return 0;
    }

    KboFaDeclarationReportSqlContext ctx = {rows, max_rows, 0, 0};
    static const char* sql =
        "SELECT declaration_date, season, player_id, player_name, declared, team_id, league_id, "
        "nation_id, age, contract_level, salary, fa_demand, score, threshold_value, "
        "grade, case_label, overall, talent, ratings, career, source, reason, decision_reason "
        "FROM fa_declarations "
        "WHERE player_id != 0 AND season != 0 "
        "ORDER BY declaration_date DESC, id DESC;";
    if (!kbo_save_state_query(sql, kbo_fa_declaration_sql_report_cb, &ctx, "fa_declarations_report_load")) {
        return 0;
    }
    if (ctx.overflowed > 0) {
        kbo_log_runtimef(
            "KBO FA declarations sqlite report load truncated rows=%d capacity=%d",
            ctx.overflowed,
            max_rows);
    }
    *out_count = ctx.count;
    return 1;
}
