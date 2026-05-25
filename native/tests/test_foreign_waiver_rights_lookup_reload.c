#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/foreign/rights/query/foreign_waiver_rights_query.h"

static char g_test_rights_path[MAX_PATH] = "save_a.sqlite3";
static int g_test_load_count = 0;
static int g_test_load_ok = 1;

int kbo_get_foreign_waiver_rights_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u || g_test_rights_path[0] == '\0') {
        return 0;
    }
    snprintf(out, out_size, "%s", g_test_rights_path);
    return 1;
}

int kbo_load_foreign_waiver_rights(void)
{
    g_test_load_count++;
    return g_test_load_ok;
}

static void test_lookup_loads_once_for_same_save_path(void)
{
    assert(!kbo_foreign_waiver_rights_lookup_context_ready());
    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();
    assert(g_test_load_count == 1);
    assert(kbo_foreign_waiver_rights_lookup_context_ready());
    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();
    assert(g_test_load_count == 1);
    printf("test_lookup_loads_once_for_same_save_path: PASS\n");
}

static void test_lookup_reloads_when_save_path_changes(void)
{
    snprintf(g_test_rights_path, sizeof(g_test_rights_path), "%s", "save_b.sqlite3");
    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();
    assert(g_test_load_count == 2);
    assert(kbo_foreign_waiver_rights_lookup_context_ready());
    printf("test_lookup_reloads_when_save_path_changes: PASS\n");
}

static void test_lookup_not_ready_when_new_save_load_fails(void)
{
    g_test_load_ok = 0;
    snprintf(g_test_rights_path, sizeof(g_test_rights_path), "%s", "save_c.sqlite3");
    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();
    assert(g_test_load_count == 3);
    assert(!kbo_foreign_waiver_rights_lookup_context_ready());
    printf("test_lookup_not_ready_when_new_save_load_fails: PASS\n");
}

int main(void)
{
    test_lookup_loads_once_for_same_save_path();
    test_lookup_reloads_when_save_path_changes();
    test_lookup_not_ready_when_new_save_load_fails();
    printf("All foreign waiver rights lookup reload tests passed.\n");
    return 0;
}
