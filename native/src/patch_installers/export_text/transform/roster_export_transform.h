#ifndef KBOFIX_SRC_PATCH_INSTALLERS_EXPORT_TEXT_TRANSFORM_ROSTER_EXPORT_TRANSFORM_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_EXPORT_TEXT_TRANSFORM_ROSTER_EXPORT_TRANSFORM_H_

#include <stddef.h>

#define KBO_ROSTER_EXPORT_PENDING_MAX (32u * 1024u * 1024u)

int kbo_roster_export_buffer_append(
    char** buffer,
    size_t* len,
    size_t* cap,
    const char* data,
    size_t data_len);

int kbo_roster_export_build_transformed(
    char** pending,
    size_t* pending_len,
    size_t* pending_cap,
    const char* data,
    size_t data_len,
    int flush_pending,
    char** out,
    size_t* out_len,
    size_t* out_cap);

#endif
