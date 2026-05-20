

#include "nation_id_scan.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"

void kbo_scan_nation_id_offset_once(void)
{
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        kbo_log_runtime_line("nation_id_scan: no player vector");
        return;
    }

    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    if (kbo_league_id == 0) {
        kbo_log_runtime_line("nation_id_scan: kbo league id unknown");
        return;
    }

    /* Scan 0x00-0x400 for uint32 == 177 across KBO players */
    const uint32_t SCAN_END  = 0x400u;
    const uint32_t SCAN_STEP = 4u;
    const uint32_t SLOTS     = SCAN_END / SCAN_STEP;   /* 256 slots */
    uint32_t hits32[256] = {0};
    uint32_t hits8[256]  = {0};   /* also check uint8 at same positions */

    int kbo_players_checked = 0;
    for (int32_t i = 0; i < player_count && kbo_players_checked < 300; i++) {
        uintptr_t ptr = *(uintptr_t*)(player_vector + (uintptr_t)i * sizeof(uintptr_t));
        if (!kbo_player_pointer_plausible(ptr)) {
            continue;
        }
        uint8_t* p = (uint8_t*)ptr;
        if (!memory_range_readable(p, SCAN_END + 4)) {
            continue;
        }
        uint32_t league_id = *(uint32_t*)(p + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (league_id != kbo_league_id) {
            continue;
        }
        kbo_players_checked++;
        for (uint32_t off = 0; off < SCAN_END; off += SCAN_STEP) {
            uint32_t val32 = *(uint32_t*)(p + off);
            if (val32 == 177u) {
                hits32[off / SCAN_STEP]++;
            }
            if (p[off] == 177u) {
                hits8[off / SCAN_STEP]++;
            }
        }
    }

    kbo_log_runtimef("nation_id_scan: checked %d KBO players (range 0x000-0x%03x)",
        kbo_players_checked, SCAN_END);
    for (uint32_t s = 0; s < SLOTS; s++) {
        uint32_t off = s * SCAN_STEP;
        if (hits32[s] > 0) {
            kbo_log_runtimef("nation_id_scan: uint32 offset=0x%03x hits=%u/%d",
                off, hits32[s], kbo_players_checked);
        }
        if (hits8[s] > 0) {
            kbo_log_runtimef("nation_id_scan: uint8  offset=0x%03x hits=%u/%d",
                off, hits8[s], kbo_players_checked);
        }
    }
    if (kbo_players_checked > 0) {
        /* dump first KBO player's raw bytes at 0x00-0x100 for manual inspection */
        for (int32_t i = 0; i < player_count; i++) {
            uintptr_t ptr = *(uintptr_t*)(player_vector + (uintptr_t)i * sizeof(uintptr_t));
            if (!kbo_player_pointer_plausible(ptr)) continue;
            uint8_t* p = (uint8_t*)ptr;
            if (!memory_range_readable(p, 0x110)) continue;
            if (*(uint32_t*)(p + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) != kbo_league_id) continue;
            uint32_t pid = *(uint32_t*)(p + OOTP27_PLAYER_ID_OFFSET);
            kbo_log_runtimef("nation_id_scan: first KBO player ptr=%p pid=%u", (void*)ptr, pid);
            kbo_log_runtimef("nation_id_scan: +000 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],p[12],p[13],p[14],p[15]);
            kbo_log_runtimef("nation_id_scan: +010 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[16],p[17],p[18],p[19],p[20],p[21],p[22],p[23],p[24],p[25],p[26],p[27],p[28],p[29],p[30],p[31]);
            kbo_log_runtimef("nation_id_scan: +020 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[32],p[33],p[34],p[35],p[36],p[37],p[38],p[39],p[40],p[41],p[42],p[43],p[44],p[45],p[46],p[47]);
            kbo_log_runtimef("nation_id_scan: +030 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[48],p[49],p[50],p[51],p[52],p[53],p[54],p[55],p[56],p[57],p[58],p[59],p[60],p[61],p[62],p[63]);
            kbo_log_runtimef("nation_id_scan: +040 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[64],p[65],p[66],p[67],p[68],p[69],p[70],p[71],p[72],p[73],p[74],p[75],p[76],p[77],p[78],p[79]);
            kbo_log_runtimef("nation_id_scan: +050 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[80],p[81],p[82],p[83],p[84],p[85],p[86],p[87],p[88],p[89],p[90],p[91],p[92],p[93],p[94],p[95]);
            kbo_log_runtimef("nation_id_scan: +060 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[96],p[97],p[98],p[99],p[100],p[101],p[102],p[103],p[104],p[105],p[106],p[107],p[108],p[109],p[110],p[111]);
            kbo_log_runtimef("nation_id_scan: +070 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[112],p[113],p[114],p[115],p[116],p[117],p[118],p[119],p[120],p[121],p[122],p[123],p[124],p[125],p[126],p[127]);
            kbo_log_runtimef("nation_id_scan: +080 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[128],p[129],p[130],p[131],p[132],p[133],p[134],p[135],p[136],p[137],p[138],p[139],p[140],p[141],p[142],p[143]);
            kbo_log_runtimef("nation_id_scan: +090 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[144],p[145],p[146],p[147],p[148],p[149],p[150],p[151],p[152],p[153],p[154],p[155],p[156],p[157],p[158],p[159]);
            kbo_log_runtimef("nation_id_scan: +0a0 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[160],p[161],p[162],p[163],p[164],p[165],p[166],p[167],p[168],p[169],p[170],p[171],p[172],p[173],p[174],p[175]);
            kbo_log_runtimef("nation_id_scan: +0b0 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[176],p[177],p[178],p[179],p[180],p[181],p[182],p[183],p[184],p[185],p[186],p[187],p[188],p[189],p[190],p[191]);
            kbo_log_runtimef("nation_id_scan: +0c0 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[192],p[193],p[194],p[195],p[196],p[197],p[198],p[199],p[200],p[201],p[202],p[203],p[204],p[205],p[206],p[207]);
            kbo_log_runtimef("nation_id_scan: +0d0 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[208],p[209],p[210],p[211],p[212],p[213],p[214],p[215],p[216],p[217],p[218],p[219],p[220],p[221],p[222],p[223]);
            kbo_log_runtimef("nation_id_scan: +0e0 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[224],p[225],p[226],p[227],p[228],p[229],p[230],p[231],p[232],p[233],p[234],p[235],p[236],p[237],p[238],p[239]);
            kbo_log_runtimef("nation_id_scan: +0f0 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[240],p[241],p[242],p[243],p[244],p[245],p[246],p[247],p[248],p[249],p[250],p[251],p[252],p[253],p[254],p[255]);
            break;
        }
    }
}
