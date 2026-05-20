#include "../internal/state_league_lookup_internal.h"

uintptr_t kbo_scan_named_league_ptr(uint32_t league_id, SIZE_T max_region_size, int* out_score, char* out_name, size_t out_name_size)
{
    uintptr_t best_ptr = 0;
    int best_score = -1000;
    char best_name[96] = {0};
    char best_logo[128] = {0};

    uintptr_t address = KBO_RUNTIME_MIN_USER_POINTER;
    MEMORY_BASIC_INFORMATION mbi;
    while (VirtualQuery((void*)address, &mbi, sizeof(mbi)) != 0) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        uintptr_t end = base + mbi.RegionSize;
        if (end <= base) {
            break;
        }

        if (mbi.State == MEM_COMMIT
                && mbi.Type == MEM_PRIVATE
                && kbo_league_scan_protect_allows_read(mbi.Protect)
                && mbi.RegionSize >= (SIZE_T)KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN
                && mbi.RegionSize <= max_region_size) {
            uintptr_t scan_end = end >= sizeof(uint32_t) ? end - sizeof(uint32_t) : base;
            for (uintptr_t p = base; p <= scan_end; p += sizeof(uint32_t)) {
                if (*(uint32_t*)p != league_id) {
                    continue;
                }

                static const uint32_t id_offsets[] = {
                    OOTP27_KBO_LEAGUE_ID_OFFSET,
                    OOTP27_KBO_LEAGUE_ID_OFFSET + 8u
                };
                for (size_t i = 0; i < sizeof(id_offsets) / sizeof(id_offsets[0]); i++) {
                    uint32_t id_offset = id_offsets[i];
                    if (p < base + id_offset) {
                        continue;
                    }

                    uintptr_t candidate = p - id_offset;
                    if ((candidate & 7u) != 0
                            || candidate < base
                            || candidate + KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN > end) {
                        continue;
                    }

                    char candidate_name[96] = {0};
                    char candidate_logo[128] = {0};
                    int score = kbo_hub_named_league_candidate_score(
                        candidate,
                        league_id,
                        candidate_name,
                        sizeof(candidate_name),
                        candidate_logo,
                        sizeof(candidate_logo));
                    if (score > best_score) {
                        best_score = score;
                        best_ptr = candidate;
                        snprintf(best_name, sizeof(best_name), "%s", candidate_name);
                        snprintf(best_logo, sizeof(best_logo), "%s", candidate_logo);
                        if (score >= KBO_NAMED_LEAGUE_SCAN_EARLY_SCORE) {
                            if (out_score != NULL) {
                                *out_score = best_score;
                            }
                            if (out_name != NULL && out_name_size > 0) {
                                snprintf(out_name, out_name_size, "%s", best_name);
                            }
                            kbo_hub_store_league_display_cache(league_id, best_ptr, best_score, best_name, best_logo);
                            return best_ptr;
                        }
                    }
                }
            }
        }

        address = end;
        if (address >= KBO_RUNTIME_USER_SCAN_END) {
            break;
        }
    }

    if (out_score != NULL) {
        *out_score = best_score;
    }
    if (out_name != NULL && out_name_size > 0) {
        snprintf(out_name, out_name_size, "%s", best_name);
    }
    if (best_ptr != 0 && best_score >= KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        kbo_hub_store_league_display_cache(league_id, best_ptr, best_score, best_name, best_logo);
    }
    return best_ptr;
}

int kbo_hub_league_id_list_index(const uint32_t* league_ids, int league_count, uint32_t league_id)
{
    if (league_ids == NULL || league_id == 0) {
        return -1;
    }
    for (int i = 0; i < league_count; i++) {
        if (league_ids[i] == league_id) {
            return i;
        }
    }
    return -1;
}

int kbo_hub_collect_visible_league_ids(uint32_t* league_ids, int max_leagues)
{
    if (league_ids == NULL || max_leagues <= 0) {
        return 0;
    }

    uintptr_t team_vector = 0;
    int32_t team_count = 0;
    if (!kbo_hub_get_team_vector(&team_vector, &team_count)) {
        return 0;
    }

    int league_count = 0;
    for (int32_t i = 0; i < team_count && league_count < max_leagues; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }

        uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (league_id == 0 || kbo_hub_league_id_list_index(league_ids, league_count, league_id) >= 0) {
            continue;
        }

        league_ids[league_count++] = league_id;
    }

    return league_count;
}

int kbo_hub_count_cached_league_ids(const uint32_t* league_ids, int league_count)
{
    int count = 0;
    for (int i = 0; i < league_count; i++) {
        if (kbo_hub_find_league_display_cache_slot(league_ids[i]) >= 0) {
            count++;
        }
    }
    return count;
}

int kbo_scan_named_league_ptrs_for_ids(const uint32_t* league_ids, int league_count, SIZE_T max_region_size)
{
    if (league_ids == NULL || league_count <= 0) {
        return 0;
    }

    int found = kbo_hub_count_cached_league_ids(league_ids, league_count);
    uintptr_t address = KBO_RUNTIME_MIN_USER_POINTER;
    MEMORY_BASIC_INFORMATION mbi;
    while (VirtualQuery((void*)address, &mbi, sizeof(mbi)) != 0) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        uintptr_t end = base + mbi.RegionSize;
        if (end <= base) {
            break;
        }

        if (mbi.State == MEM_COMMIT
                && mbi.Type == MEM_PRIVATE
                && kbo_league_scan_protect_allows_read(mbi.Protect)
                && mbi.RegionSize >= (SIZE_T)KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN
                && mbi.RegionSize <= max_region_size) {
            uintptr_t scan_end = end >= sizeof(uint32_t) ? end - sizeof(uint32_t) : base;
            for (uintptr_t p = base; p <= scan_end; p += sizeof(uint32_t)) {
                uint32_t probe_id = *(uint32_t*)p;
                if (kbo_hub_league_id_list_index(league_ids, league_count, probe_id) < 0) {
                    continue;
                }

                static const uint32_t id_offsets[] = {
                    OOTP27_KBO_LEAGUE_ID_OFFSET,
                    OOTP27_KBO_LEAGUE_ID_OFFSET + 8u
                };
                for (size_t i = 0; i < sizeof(id_offsets) / sizeof(id_offsets[0]); i++) {
                    uint32_t id_offset = id_offsets[i];
                    if (p < base + id_offset) {
                        continue;
                    }

                    uintptr_t candidate = p - id_offset;
                    if ((candidate & 7u) != 0
                            || candidate < base
                            || candidate + KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN > end) {
                        continue;
                    }

                    char candidate_name[96] = {0};
                    char candidate_logo[128] = {0};
                    int score = kbo_hub_named_league_candidate_score(
                        candidate,
                        probe_id,
                        candidate_name,
                        sizeof(candidate_name),
                        candidate_logo,
                        sizeof(candidate_logo));
                    if (score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
                        continue;
                    }

                    int before = kbo_hub_find_league_display_cache_slot(probe_id) >= 0;
                    kbo_hub_store_league_display_cache(probe_id, candidate, score, candidate_name, candidate_logo);
                    if (!before && kbo_hub_find_league_display_cache_slot(probe_id) >= 0) {
                        found++;
                        if (found >= league_count) {
                            return found;
                        }
                    }
                }
            }
        }

        address = end;
        if (address >= KBO_RUNTIME_USER_SCAN_END) {
            break;
        }
    }

    return found;
}

