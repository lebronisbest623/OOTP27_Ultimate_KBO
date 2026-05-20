#ifndef KBOFIX_SRC_CORE_POLICY_CORE_POLICY_H_
#define KBOFIX_SRC_CORE_POLICY_CORE_POLICY_H_

#include <stdint.h>

int32_t kbo_read_clamped_policy_int(
    const char* file_name,
    const char* key,
    int32_t fallback,
    int32_t min_value,
    int32_t max_value);
int32_t kbo_read_clamped_global_policy_int(
    const char* file_name,
    const char* key,
    int32_t fallback,
    int32_t min_value,
    int32_t max_value);
int kbo_read_policy_int_value(const char* file_name, const char* key, int* out_value);
int kbo_read_policy_flag_value(const char* file_name, const char* key, int* out_value);
int kbo_read_global_policy_int_value(const char* file_name, const char* key, int* out_value);

#endif
