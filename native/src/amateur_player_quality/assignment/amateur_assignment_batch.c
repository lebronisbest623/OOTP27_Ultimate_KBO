#include "../internal/amateur_assignment_internal.h"
#include "../../bootstrap/profiling/profiler.h"

__declspec(noinline) void ootp_kbo_amateur_assignment_batch_probe(
    uintptr_t player_list_ptr,
    int32_t player_count,
    uintptr_t context_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    static volatile LONG entry_log_count = 0;
    LONG entry_slot = InterlockedIncrement(&entry_log_count);
    if (entry_slot <= 20) {
        kbo_log_runtimef(
            "amateur assignment batch probe entry list=%p count=%d context=%p",
            (void*)player_list_ptr,
            player_count,
            (void*)context_ptr);
    }

    if (player_list_ptr == 0 || player_count <= 0 || player_count > 512) {
        if (entry_slot <= 20) {
            kbo_log_runtimef(
                "amateur assignment batch probe skipped reason=bad_args list=%p count=%d context=%p",
                (void*)player_list_ptr,
                player_count,
                (void*)context_ptr);
        }
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "amateur.assignment_batch");
    }

    if (!memory_range_readable((void*)player_list_ptr, (SIZE_T)player_count * sizeof(uintptr_t))) {
        if (entry_slot <= 20) {
            kbo_log_runtimef(
                "amateur assignment batch probe skipped reason=unreadable list_readable=%d list=%p count=%d context=%p",
                memory_range_readable((void*)player_list_ptr, (SIZE_T)player_count * sizeof(uintptr_t)),
                (void*)player_list_ptr,
                player_count,
                (void*)context_ptr);
        }
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "amateur.assignment_batch");
    }

    static volatile LONG capture_log_count = 0;
    LONG capture_slot = InterlockedIncrement(&capture_log_count);
    if (capture_slot <= 20 || (capture_slot % 100) == 0) {
        uintptr_t* players = (uintptr_t*)player_list_ptr;
        uint32_t first_player_id = 0;
        uint32_t last_player_id = 0;
        uint32_t first_league_id = 0;
        uint32_t first_team_id = 0;
        int readable_players = 0;

        for (int32_t i = 0; i < player_count; i++) {
            uint8_t* player = (uint8_t*)players[i];
            if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
                continue;
            }
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            if (player_id == 0) {
                continue;
            }
            if (first_player_id == 0) {
                first_player_id = player_id;
                first_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
                if (first_league_id == 0u) {
                    first_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
                }
                first_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
                if (first_team_id == 0u) {
                    first_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
                }
            }
            last_player_id = player_id;
            readable_players++;
        }

        kbo_log_runtimef(
            "amateur assignment batch captured first_league=%u first_team=%u count=%d readable=%d first_player=%u last_player=%u list=%p context=%p",
            first_league_id,
            first_team_id,
            player_count,
            readable_players,
            first_player_id,
            last_player_id,
            (void*)player_list_ptr,
            (void*)context_ptr);
    } else if (capture_slot == 21) {
        kbo_log_runtime_line(
            "amateur assignment batch capture log suppressed after 20 calls; logging every 100th call");
    }
    kbo_prepare_amateur_assignment_batch_ortools(player_list_ptr, player_count, context_ptr);
    KBO_HOOK_PROFILE_END(profile_hook, "amateur.assignment_batch");
}
