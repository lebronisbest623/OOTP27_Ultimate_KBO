#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <assert.h>
#include <stdio.h>

#include "../src/core/optimizer/kbo_optimizer.h"

static void test_exe_wins_over_python_script(void)
{
    assert(!kbo_optimizer_should_use_python_script(FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_ARCHIVE));
}

static void test_python_script_is_fallback_only(void)
{
    assert(kbo_optimizer_should_use_python_script(INVALID_FILE_ATTRIBUTES, FILE_ATTRIBUTE_ARCHIVE));
}

static void test_missing_tools_do_not_select_python(void)
{
    assert(!kbo_optimizer_should_use_python_script(INVALID_FILE_ATTRIBUTES, INVALID_FILE_ATTRIBUTES));
}

int main(void)
{
    test_exe_wins_over_python_script();
    test_python_script_is_fallback_only();
    test_missing_tools_do_not_select_python();
    printf("All optimizer tool-selection tests passed.\n");
    return 0;
}
