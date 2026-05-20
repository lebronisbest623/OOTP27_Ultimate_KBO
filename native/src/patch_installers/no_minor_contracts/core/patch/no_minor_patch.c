#include "../internal/no_minor_patch_internal.h"
#include "../../../common/patch_host.h"

int install_kbo_no_minor_contract_patch(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO no-minor-contract patch");
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO no-minor-contract patch host=%s", host);
        return 0;
    }

    kbo_enable_no_minor_contract_demand_floor();

    int ok = 0;
    ok |= install_kbo_no_minor_contract_base_patches(exe);
    ok |= install_kbo_no_minor_contract_offer_ui_patches(exe);

    kbo_log_runtimef("KBO no-minor-contract patch complete installed_any=%d", ok);
    return ok;
}
