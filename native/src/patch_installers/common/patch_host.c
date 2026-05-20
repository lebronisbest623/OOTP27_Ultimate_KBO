#include "patch_host.h"

#include <string.h>

int kbo_patch_host_matches_product(const char* host_path)
{
    if (host_path == NULL || host_path[0] == '\0') {
        return 0;
    }
    return strstr(host_path, KBO_OOTP_EXECUTABLE_NAME) != NULL
        || strstr(host_path, KBO_OOTP_EXECUTABLE_NAME_UPPER) != NULL;
}
