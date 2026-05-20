#include "..\amateur_assignment_ortools.h"

uint32_t kbo_amateur_ortools_read_result(const char* result_path)
{
    FILE* file = fopen(result_path, "rb");
    if (file == NULL) {
        return 0u;
    }
    char line[512] = {0};
    if (fgets(line, sizeof(line), file) == NULL || fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0u;
    }
    fclose(file);
    return (uint32_t)strtoul(line, NULL, 10);
}

int kbo_amateur_ortools_read_batch_result(const char* result_path, uint32_t league_id)
{
    FILE* file = fopen(result_path, "rb");
    if (file == NULL) {
        return 0;
    }

    char line[512] = {0};
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0;
    }

    KboAmateurBatchAssignment assignments[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
    int count = 0;
    while (count < KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX && fgets(line, sizeof(line), file) != NULL) {
        char* cursor = line;
        uint32_t player_id = (uint32_t)strtoul(cursor, &cursor, 10);
        if (*cursor == ',') {
            cursor++;
        }
        uint32_t target_team_id = (uint32_t)strtoul(cursor, NULL, 10);
        if (player_id != 0u && target_team_id != 0u) {
            assignments[count].player_id = player_id;
            assignments[count].league_id = league_id;
            assignments[count].target_team_id = target_team_id;
            count++;
        }
    }
    fclose(file);

    kbo_amateur_batch_lock();
    memset(g_kbo_amateur_batch_assignments, 0, sizeof(g_kbo_amateur_batch_assignments));
    memcpy(g_kbo_amateur_batch_assignments, assignments, (size_t)count * sizeof(assignments[0]));
    InterlockedExchange(&g_kbo_amateur_batch_assignment_count, count);
    kbo_amateur_batch_unlock();
    return count;
}
