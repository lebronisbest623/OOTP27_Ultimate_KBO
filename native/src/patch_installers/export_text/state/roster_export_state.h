#ifndef KBOFIX_SRC_PATCH_INSTALLERS_EXPORT_TEXT_STATE_ROSTER_EXPORT_STATE_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_EXPORT_TEXT_STATE_ROSTER_EXPORT_STATE_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef BOOL (WINAPI *KboRosterExportWriteFileFn)(
    HANDLE hFile,
    LPCVOID lpBuffer,
    DWORD nNumberOfBytesToWrite,
    LPDWORD lpNumberOfBytesWritten,
    LPOVERLAPPED lpOverlapped);

void kbo_roster_export_state_set_original_write_file(KboRosterExportWriteFileFn write_file);
void kbo_roster_export_register_handle(HANDLE handle, const char* path);
void kbo_roster_export_unregister_handle(HANDLE handle);

BOOL WINAPI kbo_roster_export_WriteFile(
    HANDLE hFile,
    LPCVOID lpBuffer,
    DWORD nNumberOfBytesToWrite,
    LPDWORD lpNumberOfBytesWritten,
    LPOVERLAPPED lpOverlapped);

#endif
