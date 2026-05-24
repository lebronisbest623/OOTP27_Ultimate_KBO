#ifndef KBO_BUILD_VERIFY_H
#define KBO_BUILD_VERIFY_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stddef.h>
#include <stdint.h>

typedef struct OotpSupportedBuild {
    uint32_t timestamp;
    uint32_t size_of_image;
    const char* label;
    int native_patches_supported;
} OotpSupportedBuild;

typedef struct OotpBuildInfo {
    int      ok;
    uint32_t timestamp;
    uint32_t size_of_image;
} OotpBuildInfo;

typedef struct OotpBuildRva {
    uint32_t timestamp;
    uint32_t size_of_image;
    uint32_t canonical_rva;
    uint32_t build_rva;
    const char* name;
} OotpBuildRva;

typedef struct OotpBuildAbiValue {
    uint32_t timestamp;
    uint32_t size_of_image;
    const char* name;
    uint32_t canonical_value;
    uint32_t build_value;
} OotpBuildAbiValue;

OotpBuildInfo read_ootp_build_info(void);
int verify_ootp_build(void);
size_t kbo_supported_ootp_build_count(void);
const OotpSupportedBuild* kbo_supported_ootp_build_at(size_t index);
int kbo_ootp_build_is_steam_2026_05_04(OotpBuildInfo info);
int kbo_resolve_build_specific_rva(uint32_t canonical_rva, uint32_t* out_rva);
void* kbo_resolve_build_specific_rva_ptr(HMODULE exe, uint32_t canonical_rva);
int kbo_resolve_build_specific_abi_value(const char* name, uint32_t* out_value);
int kbo_current_build_has_abi_profile(void);
int kbo_resolve_build_specific_rva_delta(
    uint32_t from_canonical_rva,
    uint32_t to_canonical_rva,
    intptr_t* out_delta);
void* kbo_resolve_build_specific_rva_related_ptr(
    HMODULE exe,
    void* from_ptr,
    uint32_t from_canonical_rva,
    uint32_t to_canonical_rva);
int kbo_current_build_rva_matches(uintptr_t rva, uint32_t canonical_rva);
int kbo_current_build_caller_rva_matches(uintptr_t caller_rva, uint32_t canonical_rva);

#endif
