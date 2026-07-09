#ifndef KBOFIX_SRC_PATCH_INSTALLERS_EXPORT_TEXT_IMPORTS_ROSTER_EXPORT_IMPORT_PATCH_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_EXPORT_TEXT_IMPORTS_ROSTER_EXPORT_IMPORT_PATCH_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

typedef uintptr_t (*KboRosterExportHookResolverFn)(const char* proc_name);

int kbo_roster_export_patch_module_imports(
    HMODULE module,
    KboRosterExportHookResolverFn resolver);

#endif
