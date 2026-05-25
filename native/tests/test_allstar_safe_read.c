#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../src/allstar/allstar_league_context/allstar_league_context.h"

KboAllstarLayout kbo_get_allstar_layout(void)
{
    KboAllstarLayout layout;
    memset(&layout, 0, sizeof(layout));
    return layout;
}

static void test_allstar_safe_read_valid_memory(void)
{
    uint8_t bytes[32];
    memset(bytes, 0, sizeof(bytes));
    *(uint32_t*)(bytes + 4u) = 0x12345678u;

    uint32_t value = 0u;
    assert(kbo_allstar_try_read_u32(bytes, 4u, &value));
    assert(value == 0x12345678u);
    assert(kbo_allstar_read_u32(bytes, 4u) == 0x12345678u);

    uintptr_t expected = (uintptr_t)bytes;
    uintptr_t slot = expected;
    uintptr_t actual = 0u;
    assert(kbo_allstar_try_read_ptr((uintptr_t)&slot, &actual));
    assert(actual == expected);
}

static void test_allstar_safe_read_uncommitted_memory_fails(void)
{
    void* reserved = VirtualAlloc(NULL, 4096u, MEM_RESERVE, PAGE_NOACCESS);
    assert(reserved != NULL);

    uint32_t value = 0xfeedbeefu;
    assert(!kbo_allstar_try_read_u32((uint8_t*)reserved, 0u, &value));
    assert(value == 0u);

    uintptr_t ptr = 0x1234u;
    assert(!kbo_allstar_try_read_ptr((uintptr_t)reserved, &ptr));
    assert(ptr == 0u);

    assert(VirtualFree(reserved, 0u, MEM_RELEASE));
}

int main(void)
{
    test_allstar_safe_read_valid_memory();
    test_allstar_safe_read_uncommitted_memory_fails();
    printf("All all-star safe read tests passed.\n");
    return 0;
}
