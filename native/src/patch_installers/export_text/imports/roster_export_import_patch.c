#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>

#include "../../../runtime_memory/runtime_memory.h"
#include "roster_export_import_patch.h"

int kbo_roster_export_patch_module_imports(
    HMODULE module,
    KboRosterExportHookResolverFn resolver)
{
    if (module == NULL || resolver == NULL) {
        return 0;
    }

    uint8_t* base = (uint8_t*)module;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (!memory_range_readable(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (!memory_range_readable(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }

    IMAGE_DATA_DIRECTORY imports_dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (imports_dir.VirtualAddress == 0u || imports_dir.Size == 0u) {
        return 0;
    }

    IMAGE_IMPORT_DESCRIPTOR* imports = (IMAGE_IMPORT_DESCRIPTOR*)(base + imports_dir.VirtualAddress);
    int patched = 0;
    for (DWORD i = 0u; ; i++) {
        IMAGE_IMPORT_DESCRIPTOR* desc = &imports[i];
        if (!memory_range_readable(desc, sizeof(*desc)) || desc->Name == 0u) {
            break;
        }

        IMAGE_THUNK_DATA* original_thunk = desc->OriginalFirstThunk != 0u
            ? (IMAGE_THUNK_DATA*)(base + desc->OriginalFirstThunk)
            : (IMAGE_THUNK_DATA*)(base + desc->FirstThunk);
        IMAGE_THUNK_DATA* first_thunk = (IMAGE_THUNK_DATA*)(base + desc->FirstThunk);
        for (DWORD t = 0u; ; t++) {
            IMAGE_THUNK_DATA* orig = &original_thunk[t];
            IMAGE_THUNK_DATA* slot = &first_thunk[t];
            if (!memory_range_readable(orig, sizeof(*orig)) || !memory_range_readable(slot, sizeof(*slot))) {
                break;
            }
            if (orig->u1.AddressOfData == 0u) {
                break;
            }
            if ((orig->u1.Ordinal & IMAGE_ORDINAL_FLAG) != 0u) {
                continue;
            }

            IMAGE_IMPORT_BY_NAME* import_name = (IMAGE_IMPORT_BY_NAME*)(base + orig->u1.AddressOfData);
            if (!memory_range_readable(import_name, sizeof(WORD) + 1u)) {
                continue;
            }
            uintptr_t hook_proc = resolver((const char*)import_name->Name);
            if (hook_proc == 0u || (uintptr_t)slot->u1.Function == hook_proc) {
                continue;
            }

            DWORD old_protect = 0u;
            if (!VirtualProtect(&slot->u1.Function, sizeof(slot->u1.Function), PAGE_READWRITE, &old_protect)) {
                continue;
            }
            slot->u1.Function = (ULONGLONG)hook_proc;
            DWORD ignored = 0u;
            VirtualProtect(&slot->u1.Function, sizeof(slot->u1.Function), old_protect, &ignored);
            patched++;
        }
    }
    return patched;
}
