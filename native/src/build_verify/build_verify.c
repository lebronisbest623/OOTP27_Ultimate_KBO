#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

#include "build_verify.h"
#include "../core/logging/core_log.h"

#include "build_abi.generated.h"
#include "build_rvas.generated.h"
#include "supported_builds.generated.h"

OotpBuildInfo read_ootp_build_info(void)
{
    OotpBuildInfo info = {0};

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("build verify: GetModuleHandleA(NULL) returned NULL");
        return info;
    }

    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)exe;
    if (IsBadReadPtr(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE) {
        kbo_log_runtime_line("build verify: invalid DOS header");
        return info;
    }

    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((uint8_t*)exe + dos->e_lfanew);
    if (IsBadReadPtr(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) {
        kbo_log_runtime_line("build verify: invalid NT header");
        return info;
    }

    info.ok = 1;
    info.timestamp = nt->FileHeader.TimeDateStamp;
    info.size_of_image = nt->OptionalHeader.SizeOfImage;
    return info;
}

size_t kbo_supported_ootp_build_count(void)
{
    return sizeof(KBO_SUPPORTED_OOTP_BUILDS) / sizeof(KBO_SUPPORTED_OOTP_BUILDS[0]);
}

const OotpSupportedBuild* kbo_supported_ootp_build_at(size_t index)
{
    if (index >= kbo_supported_ootp_build_count()) {
        return NULL;
    }
    return &KBO_SUPPORTED_OOTP_BUILDS[index];
}

static OotpBuildInfo kbo_cached_ootp_build_info(void)
{
    static volatile LONG state = 0;
    static OotpBuildInfo cached_info = {0};

    if (InterlockedCompareExchange(&state, 2, 2) == 2) {
        return cached_info;
    }

    if (InterlockedCompareExchange(&state, 1, 0) == 0) {
        cached_info = read_ootp_build_info();
        InterlockedExchange(&state, 2);
        return cached_info;
    }

    while (InterlockedCompareExchange(&state, 2, 2) != 2) {
        Sleep(0);
    }
    return cached_info;
}

int kbo_ootp_build_is_steam_2026_05_04(OotpBuildInfo info)
{
    return info.ok
        && info.timestamp == KBO_SUPPORTED_OOTP_BUILD_STEAM_2026_05_04_TIMESTAMP
        && info.size_of_image == KBO_SUPPORTED_OOTP_BUILD_STEAM_2026_05_04_SIZE_OF_IMAGE;
}

static const OotpBuildRva* kbo_find_build_rva(OotpBuildInfo info, uint32_t canonical_rva)
{
    if (!info.ok) {
        return NULL;
    }

    for (size_t i = 0; i < KBO_BUILD_RVA_COUNT; ++i) {
        const OotpBuildRva* rva = &KBO_BUILD_RVAS[i];
        if (rva->timestamp == info.timestamp
                && rva->size_of_image == info.size_of_image
                && rva->canonical_rva == canonical_rva) {
            return rva;
        }
    }

    return NULL;
}

static int kbo_build_has_abi_profile(OotpBuildInfo info)
{
    if (!info.ok) {
        return 0;
    }

    for (size_t i = 0; i < KBO_BUILD_ABI_VALUE_COUNT; ++i) {
        const OotpBuildAbiValue* value = &KBO_BUILD_ABI_VALUES[i];
        if (value->timestamp == info.timestamp && value->size_of_image == info.size_of_image) {
            return 1;
        }
    }

    return 0;
}

static const OotpBuildAbiValue* kbo_find_build_abi_value(OotpBuildInfo info, const char* name)
{
    if (!info.ok || name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < KBO_BUILD_ABI_VALUE_COUNT; ++i) {
        const OotpBuildAbiValue* value = &KBO_BUILD_ABI_VALUES[i];
        if (value->timestamp == info.timestamp
                && value->size_of_image == info.size_of_image
                && strcmp(value->name, name) == 0) {
            return value;
        }
    }

    return NULL;
}

int kbo_resolve_build_specific_rva(uint32_t canonical_rva, uint32_t* out_rva)
{
    if (out_rva == NULL) {
        return 0;
    }
    *out_rva = 0u;

    OotpBuildInfo info = kbo_cached_ootp_build_info();
    const OotpBuildRva* rva = kbo_find_build_rva(info, canonical_rva);
    if (rva == NULL || rva->build_rva == 0u) {
        return 0;
    }

    *out_rva = rva->build_rva;
    return 1;
}

void* kbo_resolve_build_specific_rva_ptr(HMODULE exe, uint32_t canonical_rva)
{
    if (exe == NULL) {
        return NULL;
    }

    uint32_t rva = 0u;
    if (!kbo_resolve_build_specific_rva(canonical_rva, &rva)) {
        return NULL;
    }

    return (void*)((uint8_t*)exe + rva);
}

int kbo_resolve_build_specific_abi_value(const char* name, uint32_t* out_value)
{
    if (out_value == NULL) {
        return 0;
    }
    *out_value = 0u;

    OotpBuildInfo info = kbo_cached_ootp_build_info();
    const OotpBuildAbiValue* value = kbo_find_build_abi_value(info, name);
    if (value == NULL) {
        return 0;
    }

    *out_value = value->build_value;
    return 1;
}

int kbo_current_build_has_abi_profile(void)
{
    return kbo_build_has_abi_profile(kbo_cached_ootp_build_info());
}

int kbo_resolve_build_specific_rva_delta(
    uint32_t from_canonical_rva,
    uint32_t to_canonical_rva,
    intptr_t* out_delta)
{
    if (out_delta == NULL) {
        return 0;
    }
    *out_delta = 0;

    uint32_t from_rva = 0u;
    uint32_t to_rva = 0u;
    if (!kbo_resolve_build_specific_rva(from_canonical_rva, &from_rva)
            || !kbo_resolve_build_specific_rva(to_canonical_rva, &to_rva)) {
        return 0;
    }

    *out_delta = (intptr_t)((int64_t)to_rva - (int64_t)from_rva);
    return 1;
}

void* kbo_resolve_build_specific_rva_related_ptr(
    HMODULE exe,
    void* from_ptr,
    uint32_t from_canonical_rva,
    uint32_t to_canonical_rva)
{
    if (exe == NULL || from_ptr == NULL) {
        return NULL;
    }

    intptr_t delta = 0;
    if (!kbo_resolve_build_specific_rva_delta(from_canonical_rva, to_canonical_rva, &delta)) {
        return NULL;
    }

    return (void*)((uint8_t*)from_ptr + delta);
}

int kbo_current_build_rva_matches(uintptr_t rva, uint32_t canonical_rva)
{
    uint32_t build_rva = 0u;
    return kbo_resolve_build_specific_rva(canonical_rva, &build_rva)
        && rva == (uintptr_t)build_rva;
}

int kbo_current_build_caller_rva_matches(uintptr_t caller_rva, uint32_t canonical_rva)
{
    return kbo_current_build_rva_matches(caller_rva, canonical_rva);
}

int verify_ootp_build(void)
{
    OotpBuildInfo info = read_ootp_build_info();
    if (!info.ok) {
        kbo_log_runtime_line("build verify: failed to read PE headers; skipping all KBO patches");
        return 0;
    }

    if (KBO_SUPPORTED_OOTP_BUILD_COUNT == 0u) {
        kbo_log_runtimef(
            "build verify: discovery mode; detected timestamp=0x%08X size_of_image=0x%08X",
            info.timestamp, info.size_of_image);
        return 1;
    }

    for (size_t i = 0; i < kbo_supported_ootp_build_count(); ++i) {
        const OotpSupportedBuild* build = kbo_supported_ootp_build_at(i);
        if (build != NULL && info.timestamp == build->timestamp && info.size_of_image == build->size_of_image) {
            if (!build->native_patches_supported) {
                kbo_log_runtimef(
                    "build verify: metadata-only label=%s timestamp=0x%08X size_of_image=0x%08X. "
                    "Native patches DISABLED because this build does not have a complete verified RVA table.",
                    build->label, info.timestamp, info.size_of_image);
                return 0;
            }

            if (!kbo_build_has_abi_profile(info)) {
                kbo_log_runtimef(
                    "build verify: metadata-only label=%s timestamp=0x%08X size_of_image=0x%08X. "
                    "Native patches DISABLED because this build does not have a complete verified ABI profile.",
                    build->label, info.timestamp, info.size_of_image);
                return 0;
            }

            kbo_log_runtimef(
                "build verify: ok label=%s timestamp=0x%08X size_of_image=0x%08X",
                build->label, info.timestamp, info.size_of_image);
            return 1;
        }
    }

    kbo_log_runtimef(
        "build verify: MISMATCH expected one of %u supported builds, "
        "detected timestamp=0x%08X size_of_image=0x%08X. "
        "All KBO patches DISABLED to prevent crashes or save corruption.",
        (unsigned)kbo_supported_ootp_build_count(),
        info.timestamp,
        info.size_of_image);

    return 0;
}
