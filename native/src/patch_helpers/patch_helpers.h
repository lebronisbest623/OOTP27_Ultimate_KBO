#ifndef KBOFIX_SRC_PATCH_HELPERS_PATCH_HELPERS_H_
#define KBOFIX_SRC_PATCH_HELPERS_PATCH_HELPERS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

void write_u64(uint8_t* dst, uint64_t value);
void write_u32(uint8_t* dst, uint32_t value);
int is_r11_absolute_jump_patch(const uint8_t* target);
int is_rax_absolute_jump_patch(const uint8_t* target);
int is_rip_absolute_jump_patch(const uint8_t* target);
void log_patch_bytes_mismatch(const char* label, const uint8_t* target, size_t size);
void log_extended_context(const char* label, const uint8_t* target, int pre_bytes, int total_bytes);
uint8_t* resolve_patch_target_by_rva_or_pattern(HMODULE exe, uint32_t rva, const uint8_t* expected, size_t expected_size, const char* label);
uint8_t* resolve_patch_target_by_rva_existing_rax_or_pattern(HMODULE exe, uint32_t rva, const uint8_t* expected, size_t expected_size, const char* label);
uint8_t* find_ootp_executable_pattern_nth(const uint8_t* pattern, size_t pattern_size, int desired_index, int expected_hits);
uint8_t* resolve_patch_target_by_rva_or_context_pattern(HMODULE exe, uint32_t rva, const uint8_t* expected, size_t expected_size, const uint8_t* context, size_t context_size, size_t target_offset, const char* label);
int kbo_memory_matches_masked_pattern(const uint8_t* data, const uint8_t* pattern, const uint8_t* mask, size_t size);
uint8_t* resolve_patch_target_by_rva_or_masked_context_pattern(HMODULE exe, uint32_t rva, const uint8_t* expected, size_t expected_size, const uint8_t* context, const uint8_t* context_mask, size_t context_size, size_t target_offset, const char* label);
uint8_t* resolve_patch_target_by_rva_or_masked_context_and_expected_pattern(HMODULE exe, uint32_t rva, const uint8_t* expected, const uint8_t* expected_mask, size_t expected_size, const uint8_t* context, const uint8_t* context_mask, size_t context_size, size_t target_offset, const char* label);
int patch_static_bytes(const char* label, uint8_t* target, const uint8_t* expected, const uint8_t* patch, size_t size);
void* resolve_relative_call_target(uint8_t* call_site);
void* resolve_rip_relative_lea_target(uint8_t* instruction);

#endif
