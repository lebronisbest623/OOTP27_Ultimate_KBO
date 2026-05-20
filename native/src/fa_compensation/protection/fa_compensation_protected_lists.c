#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "../../core/logging/core_log.h"
#include "../state/fa_compensation_paths_parse.h"
#include "fa_compensation_protected_lists.h"
#include "sql/fa_compensation_protected_lists_sql_store.h"

static void kbo_fa_compensation_write_csv_text(HANDLE file, const char* text)
{
    DWORD written = 0;
    WriteFile(file, "\"", 1, &written, NULL);
    if (text != NULL) {
        const char* p = text;
        while (*p != '\0') {
            if (*p == '"') {
                WriteFile(file, "\"\"", 2, &written, NULL);
            } else {
                WriteFile(file, p, 1, &written, NULL);
            }
            p++;
        }
    }
    WriteFile(file, "\"", 1, &written, NULL);
}

int kbo_persist_fa_compensation_protected_list(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t generated_yyyymmdd,
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    const char* source)
{
    if (rec == NULL || candidates == NULL || candidate_count <= 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_fa_compensation_protected_lists_sql_path(path, sizeof(path))) {
        return 0;
    }

    int protected_count = candidate_count;
    if (protected_count > (int)rec->protect_count) {
        protected_count = (int)rec->protect_count;
    }
    if (!kbo_fa_compensation_protected_lists_sql_append(
            rec,
            due_yyyymmdd,
            generated_yyyymmdd,
            candidates,
            candidate_count,
            source)) {
        kbo_log_runtimef(
            "KBO FA protected list persist failed reason=sqlite player=%u path=%s",
            rec->player_id,
            path);
        return 0;
    }

    kbo_log_runtimef(
        "KBO FA protected list generated fa_player=%u signing_team=%u original_team=%u grade=%s protected=%d/%u due=%u path=%s",
        rec->player_id,
        rec->signing_team_id,
        rec->original_team_id,
        rec->grade,
        protected_count,
        rec->protect_count,
        due_yyyymmdd,
        path);
    return 1;
}

int kbo_persist_fa_compensation_protection_debug(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t generated_yyyymmdd,
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    const KboFaProtectedCandidate* auto_protected,
    int auto_protected_count,
    const char* source)
{
    if (rec == NULL || candidates == NULL || candidate_count < 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_protection_debug_path(path, sizeof(path))) {
        return 0;
    }
    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    int exists = GetFileAttributesExA(path, GetFileExInfoStandard, &attrs);
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef("KBO FA protection debug persist failed reason=open gle=%lu path=%s", GetLastError(), path);
        return 0;
    }

    DWORD written = 0;
    if (!exists || (attrs.nFileSizeHigh == 0u && attrs.nFileSizeLow == 0u)) {
        const char* header =
            "fa_player_id,season,grade,original_team_id,signing_team_id,due_on,generated_on,"
            "list_type,rank,player_id,player_name,score,age,role,reason,source\r\n";
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    int protected_count = candidate_count > (int)rec->protect_count ? (int)rec->protect_count : candidate_count;
    for (int i = 0; i < candidate_count; i++) {
        const KboFaProtectedCandidate* candidate = &candidates[i];
        const char* list_type = i < protected_count ? "protected" : "unprotected";
        char prefix[256] = {0};
        int len = snprintf(
            prefix,
            sizeof(prefix),
            "%u,%u,%s,%u,%u,%u,%u,%s,%d,%u,",
            rec->player_id,
            rec->season,
            rec->grade,
            rec->original_team_id,
            rec->signing_team_id,
            due_yyyymmdd,
            generated_yyyymmdd,
            list_type,
            i + 1,
            candidate->player_id);
        if (len > 0 && len < (int)sizeof(prefix)) {
            WriteFile(file, prefix, (DWORD)len, &written, NULL);
        }
        kbo_fa_compensation_write_csv_text(file, candidate->player_name);
        char suffix[128] = {0};
        len = snprintf(
            suffix,
            sizeof(suffix),
            ",%d,%u,%u,%s,",
            candidate->score,
            (uint32_t)candidate->age,
            (uint32_t)candidate->role,
            candidate->reason);
        if (len > 0 && len < (int)sizeof(suffix)) {
            WriteFile(file, suffix, (DWORD)len, &written, NULL);
        }
        kbo_fa_compensation_write_csv_text(file, source != NULL ? source : "fa_compensation_protection_debug");
        WriteFile(file, "\r\n", 2, &written, NULL);
    }

    if (auto_protected != NULL && auto_protected_count > 0) {
        for (int i = 0; i < auto_protected_count; i++) {
            const KboFaProtectedCandidate* automatic = &auto_protected[i];
            char prefix[256] = {0};
            int len = snprintf(
                prefix,
                sizeof(prefix),
                "%u,%u,%s,%u,%u,%u,%u,auto_protected,%d,%u,",
                rec->player_id,
                rec->season,
                rec->grade,
                rec->original_team_id,
                rec->signing_team_id,
                due_yyyymmdd,
                generated_yyyymmdd,
                i + 1,
                automatic->player_id);
            if (len > 0 && len < (int)sizeof(prefix)) {
                WriteFile(file, prefix, (DWORD)len, &written, NULL);
            }
            kbo_fa_compensation_write_csv_text(file, automatic->player_name);
            char suffix[128] = {0};
            len = snprintf(
                suffix,
                sizeof(suffix),
                ",%d,%u,%u,%s,",
                automatic->score,
                (uint32_t)automatic->age,
                (uint32_t)automatic->role,
                automatic->reason);
            if (len > 0 && len < (int)sizeof(suffix)) {
                WriteFile(file, suffix, (DWORD)len, &written, NULL);
            }
            kbo_fa_compensation_write_csv_text(file, source != NULL ? source : "fa_compensation_protection_debug");
            WriteFile(file, "\r\n", 2, &written, NULL);
        }
    }

    CloseHandle(file);
    kbo_log_runtimef(
        "KBO FA protection debug written fa_player=%u protected=%d unprotected=%d auto=%d path=%s",
        rec->player_id,
        protected_count,
        candidate_count - protected_count,
        auto_protected_count,
        path);
    return 1;
}
