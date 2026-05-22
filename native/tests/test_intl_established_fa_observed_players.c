#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/foreign/intl_established_fa_postscan/internal/intl_established_fa_postscan_internal.h"

void kbo_log_runtime_line_at(const char* file, int line, const char* message)
{
    (void)file;
    (void)line;
    (void)message;
}

void kbo_log_runtimef_at(const char* file, int line, const char* fmt, ...)
{
    (void)file;
    (void)line;
    (void)fmt;
}

int kbo_intl_established_fa_pitcher_role_is_starter(uint8_t position_role)
{
    return position_role == 11u;
}

int kbo_intl_established_fa_pitcher_role_is_bullpen(uint8_t position_role)
{
    return position_role == 12u || position_role == 13u;
}

int kbo_intl_established_fa_position_is_catcher(uint8_t position_group, uint8_t position_role)
{
    return position_group == 2u || (position_group != 1u && position_role == 2u);
}

static void write_u32(uint8_t* player, uint32_t offset, uint32_t value)
{
    *(uint32_t*)(player + offset) = value;
}

static void test_observed_pointer_overrides_new_id_match(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    memset(player, 0, sizeof(player));
    write_u32(player, OOTP27_PLAYER_ID_OFFSET, 400u);

    KboIntlEstablishedFaPostscanState batch;
    memset(&batch, 0, sizeof(batch));
    batch.before_count = 100;
    batch.before_max_player_id = 500u;
    batch.expected_count = 20;

    kbo_intl_established_fa_postscan_reset_observed_players();
    assert(!kbo_intl_established_fa_postscan_candidate_matches(&batch, 99, 100, player));

    kbo_intl_established_fa_postscan_note_observed_player((uintptr_t)player);
    assert(kbo_intl_established_fa_postscan_observed_player_count() == 1);
    assert(kbo_intl_established_fa_postscan_candidate_matches(&batch, 99, 100, player));

    kbo_intl_established_fa_postscan_reset_observed_players();
    assert(kbo_intl_established_fa_postscan_observed_player_count() == 0);
    assert(!kbo_intl_established_fa_postscan_candidate_matches(&batch, 99, 100, player));

    write_u32(player, OOTP27_PLAYER_ID_OFFSET, 501u);
    assert(kbo_intl_established_fa_postscan_candidate_matches(&batch, 99, 100, player));

    printf("test_observed_pointer_overrides_new_id_match: PASS\n");
}

int main(void)
{
    test_observed_pointer_overrides_new_id_match();
    printf("All international established FA observed player tests passed.\n");
    return 0;
}
