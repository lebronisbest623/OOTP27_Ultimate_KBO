#include "../patch_helpers_internal.h"

void write_u64(uint8_t* dst, uint64_t value)
{
    for (int i = 0; i < 8; i++) {
        dst[i] = (uint8_t)((value >> (i * 8)) & 0xffu);
    }
}

void write_u32(uint8_t* dst, uint32_t value)
{
    for (int i = 0; i < 4; i++) {
        dst[i] = (uint8_t)((value >> (i * 8)) & 0xffu);
    }
}

int is_r11_absolute_jump_patch(const uint8_t* target)
{
    return target[0] == 0x49
        && target[1] == 0xBB
        && target[10] == 0x41
        && target[11] == 0xFF
        && target[12] == 0xE3;
}

int is_rax_absolute_jump_patch(const uint8_t* target)
{
    return target[0] == 0x48
        && target[1] == 0xB8
        && target[10] == 0xFF
        && target[11] == 0xE0;
}

int is_rip_absolute_jump_patch(const uint8_t* target)
{
    return target[0] == 0xFF
        && target[1] == 0x25
        && target[2] == 0x00
        && target[3] == 0x00
        && target[4] == 0x00
        && target[5] == 0x00;
}

void log_patch_bytes_mismatch(const char* label, const uint8_t* target, size_t size)
{
    char bytes[128] = {0};
    size_t offset = 0;
    for (size_t i = 0; i < size && offset + 4 < sizeof(bytes); i++) {
        offset += (size_t)snprintf(bytes + offset, sizeof(bytes) - offset, "%s%02X", i == 0 ? "" : " ", target[i]);
    }
    kbo_log_runtimef("%s bytes mismatch at %p: %s", label, target, bytes);
}

void log_extended_context(const char* label, const uint8_t* target, int pre_bytes, int total_bytes)
{
    const uint8_t* start = (pre_bytes > 0) ? (target - pre_bytes) : target;
    if ((uintptr_t)start < KBO_RUNTIME_MIN_USER_POINTER) {
        start = (const uint8_t*)KBO_RUNTIME_MIN_USER_POINTER;
    }
    if (!memory_range_readable(start, (SIZE_T)total_bytes)) {
        kbo_log_runtimef("%s extended ctx unreadable start=%p pre=%d total=%d", label, start, pre_bytes, total_bytes);
        return;
    }
    for (int row = 0; row < total_bytes; row += 16) {
        char hex[80] = {0};
        size_t hex_off = 0;
        for (int col = 0; col < 16 && row + col < total_bytes; col++) {
            hex_off += (size_t)snprintf(hex + hex_off, sizeof(hex) - hex_off,
                "%s%02X", col == 0 ? "" : " ", start[row + col]);
        }
        int rel = row - pre_bytes;
        kbo_log_runtimef("%s ctx%+d [%p]: %s", label, rel, start + row, hex);
    }
}

int kbo_memory_matches_masked_pattern(const uint8_t* data, const uint8_t* pattern, const uint8_t* mask, size_t size)
{
    if (data == NULL || pattern == NULL || mask == NULL || size == 0) {
        return 0;
    }
    for (size_t i = 0; i < size; i++) {
        if (mask[i] != 0 && data[i] != pattern[i]) {
            return 0;
        }
    }
    return 1;
}

int patch_static_bytes(const char* label, uint8_t* target, const uint8_t* expected, const uint8_t* patch, size_t size)
{
    if (memcmp(target, patch, size) == 0) {
        kbo_log_runtimef("%s already installed target=%p", label, target);
        return 1;
    }

    if (memcmp(target, expected, size) != 0) {
        log_patch_bytes_mismatch(label, target, size);
        return 0;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        kbo_log_runtimef("VirtualProtect failed for %s error=%lu", label, GetLastError());
        return 0;
    }

    memcpy(target, patch, size);
    FlushInstructionCache(GetCurrentProcess(), target, size);

    DWORD ignored = 0;
    VirtualProtect(target, size, old_protect, &ignored);

    kbo_log_runtimef("installed %s target=%p", label, target);
    return 1;
}

