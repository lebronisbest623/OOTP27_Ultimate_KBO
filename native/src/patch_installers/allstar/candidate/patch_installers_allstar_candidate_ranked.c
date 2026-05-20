#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../build_verify/build_verify.h"
#include "../../../core/logging/core_log.h"
#include "../../../hook_stubs/allstar/candidate/hook_stubs_allstar_candidate.h"
#include "../../../patch_helpers/patch_helpers.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../common/patch_installers_allstar_common.h"
#include "patch_installers_allstar_candidate.h"

typedef struct AllstarRankedCandidatePushSite {
    const char* label;
    uint32_t site_rva;
    uint32_t skip_rva;
    int vector_base_in_rax;
} AllstarRankedCandidatePushSite;

static int ranked_player_push_bytes_match(uint8_t* target, int vector_base_in_rax)
{
    const uint8_t r8_prefix[14] = {
        0x8B, 0x4C, 0x24, 0x48,
        0x48, 0xC1, 0xE1, 0x05,
        0x49, 0x03, 0xC8,
        0x48, 0x8B, 0xD3
    };
    const uint8_t rax_prefix[14] = {
        0x8B, 0x4C, 0x24, 0x48,
        0x48, 0xC1, 0xE1, 0x05,
        0x48, 0x03, 0xC8,
        0x48, 0x8B, 0xD3
    };
    const uint8_t suffix[7] = {
        0x44, 0x89, 0xAB, 0xE8, 0x0F, 0x00, 0x00
    };
    const uint8_t* prefix = vector_base_in_rax ? rax_prefix : r8_prefix;

    return memcmp(target, prefix, sizeof(r8_prefix)) == 0
        && target[OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_CALL_OFFSET] == 0xE8
        && memcmp(
            target + OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_CALL_OFFSET + 5u,
            suffix,
            sizeof(suffix)) == 0;
}

static int install_ranked_player_push_filter_site(HMODULE exe, const AllstarRankedCandidatePushSite* site)
{
    uint8_t* target = (uint8_t*)kbo_resolve_build_specific_rva_ptr(exe, site->site_rva);
    if (target == NULL) {
        kbo_log_runtimef(
            "KBO all-star ranked candidate player push filter hook target unresolved label=%s rva=0x%08X",
            site->label,
            site->site_rva);
        return 0;
    }

    if (memory_range_readable(target, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_STOLEN_LEN)
            && (is_rip_absolute_jump_patch(target) || is_rax_absolute_jump_patch(target))) {
        kbo_log_runtimef(
            "KBO all-star ranked candidate player push filter hook already installed label=%s target=%p",
            site->label,
            target);
        return 1;
    }

    if (!memory_range_readable(target, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_STOLEN_LEN)) {
        kbo_log_runtimef(
            "KBO all-star ranked candidate player push filter hook target unreadable label=%s target=%p",
            site->label,
            target);
        return 0;
    }

    if (!ranked_player_push_bytes_match(target, site->vector_base_in_rax)) {
        kbo_log_runtimef(
            "KBO all-star ranked candidate player push filter hook byte mismatch label=%s target=%p",
            site->label,
            target);
        log_patch_bytes_mismatch(
            "KBO all-star ranked candidate player push filter hook",
            target,
            OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_STOLEN_LEN);
        log_extended_context("KBO all-star ranked candidate player push filter hook", target, 16, 80);
        return 0;
    }

    void* vector_push_back_address = resolve_relative_call_target(
        target + OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_CALL_OFFSET);
    if (vector_push_back_address == NULL) {
        kbo_log_runtimef(
            "KBO all-star ranked candidate player push filter hook skipped: vector push helper target unresolved label=%s",
            site->label);
        return 0;
    }

    void* return_address = target + OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_STOLEN_LEN;
    void* skip_address = kbo_resolve_build_specific_rva_ptr(exe, site->skip_rva);
    if (skip_address == NULL) {
        kbo_log_runtimef(
            "KBO all-star ranked candidate player push filter hook skip target unresolved label=%s rva=0x%08X",
            site->label,
            site->skip_rva);
        return 0;
    }

    uint8_t* stub = build_allstar_candidate_ranked_player_push_filter_stub(
        return_address,
        skip_address,
        vector_push_back_address,
        site->vector_base_in_rax);
    if (stub == NULL) {
        kbo_log_runtimef(
            "failed to allocate KBO all-star ranked candidate player push filter hook stub label=%s",
            site->label);
        return 0;
    }

    uint8_t patch[OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_STOLEN_LEN] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0,0,0,0,0,0,0,0,
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    write_u64(&patch[6], (uint64_t)(uintptr_t)stub);

    DWORD old_protect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef(
            "VirtualProtect failed for KBO all-star ranked candidate player push filter hook label=%s error=%lu",
            site->label,
            GetLastError());
        return 0;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));
    DWORD ignored = 0;
    VirtualProtect(target, sizeof(patch), old_protect, &ignored);
    kbo_log_runtimef(
        "installed KBO all-star ranked candidate player push filter hook label=%s target=%p stub=%p return=%p skip=%p vector_push=%p helper=%p base=%s",
        site->label,
        target,
        stub,
        return_address,
        skip_address,
        vector_push_back_address,
        &ootp_kbo_allstar_candidate_push_filter,
        site->vector_base_in_rax ? "rax" : "r8");
    return 1;
}

int install_allstar_candidate_ranked_player_push_filter_patch(void)
{
    HMODULE exe = kbo_allstar_get_host_exe("KBO all-star ranked candidate player push filter patch");
    if (exe == NULL) {
        return 0;
    }

    const AllstarRankedCandidatePushSite sites[] = {
        { "quota-catcher", OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_SITE_1_RVA, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_COMMON_SKIP_RVA, 0 },
        { "quota-infield", OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_SITE_2_RVA, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_COMMON_SKIP_RVA, 0 },
        { "quota-dh", OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_SITE_3_RVA, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_COMMON_SKIP_RVA, 0 },
        { "quota-outfield", OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_SITE_4_RVA, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_COMMON_SKIP_RVA, 0 },
        { "quota-pitcher", OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_SITE_5_RVA, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_COMMON_SKIP_RVA, 0 },
        { "fallback-fill", OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_SITE_6_RVA, OOTP27_ALLSTAR_CANDIDATE_RANKED_PLAYER_PUSH_FALLBACK_SKIP_RVA, 1 }
    };

    int ok = 1;
    int installed = 0;
    for (int i = 0; i < (int)(sizeof(sites) / sizeof(sites[0])); i++) {
        int site_ok = install_ranked_player_push_filter_site(exe, &sites[i]);
        if (!site_ok) {
            ok = 0;
        } else {
            installed++;
        }
    }

    kbo_log_runtimef(
        "KBO all-star ranked candidate player push filter hooks finished ok=%d installed_or_present=%d total=%d",
        ok,
        installed,
        (int)(sizeof(sites) / sizeof(sites[0])));
    return ok;
}
