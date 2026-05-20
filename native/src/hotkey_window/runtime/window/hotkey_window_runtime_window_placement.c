#include "hotkey_window_runtime_window_internal.h"

#define KBO_HUB_WINDOW_PLACEMENT_FILE "hub_window_placement.txt"

void kbo_hub_get_work_area(HWND hwnd, RECT* out)
{
    if (out == NULL) {
        return;
    }
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    if (monitor != NULL && GetMonitorInfoA(monitor, &info)) {
        *out = info.rcWork;
        return;
    }
    SystemParametersInfoA(SPI_GETWORKAREA, 0, out, 0);
}

RECT kbo_hub_fixed_window_rect(HWND hwnd)
{
    RECT work = {0, 0, 0, 0};
    kbo_hub_get_work_area(hwnd, &work);
    int max_client_w = (work.right > work.left) ? (work.right - work.left - 48) : KBO_HUB_FIXED_CLIENT_WIDTH;
    int max_client_h = (work.bottom > work.top) ? (work.bottom - work.top - 72) : KBO_HUB_FIXED_CLIENT_HEIGHT;
    int client_w = KBO_HUB_FIXED_CLIENT_WIDTH;
    int client_h = KBO_HUB_FIXED_CLIENT_HEIGHT;
    if (max_client_w > 0 && client_w > max_client_w) { client_w = max_client_w; }
    if (max_client_h > 0 && client_h > max_client_h) { client_h = max_client_h; }
    if (client_w < KBO_HUB_MIN_CLIENT_WIDTH) { client_w = KBO_HUB_MIN_CLIENT_WIDTH; }
    if (client_h < KBO_HUB_MIN_CLIENT_HEIGHT) { client_h = KBO_HUB_MIN_CLIENT_HEIGHT; }
    RECT rect = {0, 0, client_w, client_h};
    AdjustWindowRectEx(&rect, KBO_HUB_WINDOW_STYLE, FALSE, KBO_HUB_WINDOW_EX_STYLE);
    return rect;
}

SIZE kbo_hub_min_track_size(void)
{
    RECT rect = {0, 0, KBO_HUB_MIN_CLIENT_WIDTH, KBO_HUB_MIN_CLIENT_HEIGHT};
    AdjustWindowRectEx(&rect, KBO_HUB_WINDOW_STYLE, FALSE, KBO_HUB_WINDOW_EX_STYLE);
    SIZE size = {rect.right - rect.left, rect.bottom - rect.top};
    return size;
}

int kbo_hub_rect_width(const RECT* rect)
{
    return rect != NULL ? (int)(rect->right - rect->left) : 0;
}

int kbo_hub_rect_height(const RECT* rect)
{
    return rect != NULL ? (int)(rect->bottom - rect->top) : 0;
}

static void kbo_hub_clamp_rect_to_work_area(HWND hwnd, RECT* rect)
{
    if (rect == NULL) {
        return;
    }

    RECT work = {0, 0, 0, 0};
    HMONITOR monitor = !IsRectEmpty(rect) ? MonitorFromRect(rect, MONITOR_DEFAULTTONEAREST) : NULL;
    if (monitor != NULL) {
        MONITORINFO info;
        memset(&info, 0, sizeof(info));
        info.cbSize = sizeof(info);
        if (GetMonitorInfoA(monitor, &info)) {
            work = info.rcWork;
        }
    }
    if (work.right <= work.left || work.bottom <= work.top) {
        kbo_hub_get_work_area(hwnd, &work);
    }

    SIZE min_track = kbo_hub_min_track_size();
    int width = kbo_hub_rect_width(rect);
    int height = kbo_hub_rect_height(rect);
    int work_width = work.right - work.left;
    int work_height = work.bottom - work.top;

    if (width < min_track.cx) {
        width = min_track.cx;
    }
    if (height < min_track.cy) {
        height = min_track.cy;
    }
    if (work_width > 0 && width > work_width) {
        width = work_width;
    }
    if (work_height > 0 && height > work_height) {
        height = work_height;
    }

    if (rect->left + width > work.right) {
        rect->left = work.right - width;
    }
    if (rect->top + height > work.bottom) {
        rect->top = work.bottom - height;
    }
    if (rect->left < work.left) {
        rect->left = work.left;
    }
    if (rect->top < work.top) {
        rect->top = work.top;
    }

    rect->right = rect->left + width;
    rect->bottom = rect->top + height;
}

static int kbo_hub_read_window_placement_file(RECT* out_rect)
{
    if (out_rect == NULL) {
        return 0;
    }

    char path[MAX_PATH];
    if (!kbo_get_global_data_file(KBO_HUB_WINDOW_PLACEMENT_FILE, path, sizeof(path))) {
        kbo_log_runtimef("KBO F2 hub placement load skipped reason=path");
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
            kbo_log_runtimef("KBO F2 hub placement load failed open error=%lu path=%s", error, path);
        }
        return 0;
    }

    char buffer[128];
    DWORD bytes_read = 0;
    memset(buffer, 0, sizeof(buffer));
    if (!ReadFile(file, buffer, (DWORD)(sizeof(buffer) - 1), &bytes_read, NULL)) {
        DWORD error = GetLastError();
        CloseHandle(file);
        kbo_log_runtimef("KBO F2 hub placement load failed read error=%lu path=%s", error, path);
        return 0;
    }
    CloseHandle(file);
    buffer[bytes_read] = '\0';

    long left = 0;
    long top = 0;
    long right = 0;
    long bottom = 0;
    if (sscanf(buffer, "left=%ld top=%ld right=%ld bottom=%ld", &left, &top, &right, &bottom) != 4
            && sscanf(buffer, "%ld %ld %ld %ld", &left, &top, &right, &bottom) != 4) {
        kbo_log_runtimef("KBO F2 hub placement load ignored reason=parse path=%s", path);
        return 0;
    }

    out_rect->left = (LONG)left;
    out_rect->top = (LONG)top;
    out_rect->right = (LONG)right;
    out_rect->bottom = (LONG)bottom;
    if (kbo_hub_rect_width(out_rect) <= 0 || kbo_hub_rect_height(out_rect) <= 0) {
        kbo_log_runtimef("KBO F2 hub placement load ignored reason=invalid_rect path=%s", path);
        return 0;
    }

    kbo_log_runtimef("KBO F2 hub placement loaded rect=%ld,%ld,%ld,%ld path=%s",
        (long)out_rect->left, (long)out_rect->top, (long)out_rect->right, (long)out_rect->bottom, path);
    return 1;
}

int kbo_hub_try_load_window_placement(HWND hwnd, RECT* out_rect)
{
    if (!kbo_hub_read_window_placement_file(out_rect)) {
        return 0;
    }
    kbo_hub_clamp_rect_to_work_area(hwnd, out_rect);
    return 1;
}

void kbo_hub_save_window_placement(HWND hwnd)
{
    if (hwnd == NULL || IsIconic(hwnd)) {
        return;
    }

    char path[MAX_PATH];
    if (!kbo_get_global_data_file(KBO_HUB_WINDOW_PLACEMENT_FILE, path, sizeof(path))) {
        kbo_log_runtimef("KBO F2 hub placement save skipped reason=path");
        return;
    }

    WINDOWPLACEMENT placement;
    memset(&placement, 0, sizeof(placement));
    placement.length = sizeof(placement);

    RECT rect = {0, 0, 0, 0};
    if (GetWindowPlacement(hwnd, &placement)) {
        rect = placement.rcNormalPosition;
    } else if (!GetWindowRect(hwnd, &rect)) {
        kbo_log_runtimef("KBO F2 hub placement save failed rect error=%lu path=%s", GetLastError(), path);
        return;
    }

    if (kbo_hub_rect_width(&rect) <= 0 || kbo_hub_rect_height(&rect) <= 0) {
        kbo_log_runtimef("KBO F2 hub placement save skipped reason=invalid_rect");
        return;
    }

    kbo_hub_clamp_rect_to_work_area(hwnd, &rect);

    char buffer[128];
    int length = snprintf(buffer, sizeof(buffer), "left=%ld top=%ld right=%ld bottom=%ld\r\n",
        (long)rect.left, (long)rect.top, (long)rect.right, (long)rect.bottom);
    if (length <= 0 || length >= (int)sizeof(buffer)) {
        kbo_log_runtimef("KBO F2 hub placement save skipped reason=overflow");
        return;
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef("KBO F2 hub placement save failed open error=%lu path=%s", GetLastError(), path);
        return;
    }

    DWORD bytes_written = 0;
    if (!WriteFile(file, buffer, (DWORD)length, &bytes_written, NULL) || bytes_written != (DWORD)length) {
        DWORD error = GetLastError();
        CloseHandle(file);
        kbo_log_runtimef("KBO F2 hub placement save failed write error=%lu path=%s", error, path);
        return;
    }
    CloseHandle(file);

    kbo_log_runtimef("KBO F2 hub placement saved rect=%ld,%ld,%ld,%ld path=%s",
        (long)rect.left, (long)rect.top, (long)rect.right, (long)rect.bottom, path);
}

void kbo_hub_apply_fixed_window_placement(HWND hwnd, int preserve_position)
{
    if (hwnd == NULL) {
        return;
    }

    RECT rect = {0, 0, 0, 0};
    if (!kbo_hub_try_load_window_placement(hwnd, &rect)) {
        rect = kbo_hub_fixed_window_rect(hwnd);
        if (preserve_position) {
            RECT current = {0, 0, 0, 0};
            if (GetWindowRect(hwnd, &current)) {
                int width = kbo_hub_rect_width(&rect);
                int height = kbo_hub_rect_height(&rect);
                rect.left = current.left;
                rect.top = current.top;
                rect.right = rect.left + width;
                rect.bottom = rect.top + height;
            }
        } else {
            RECT work = {0, 0, 0, 0};
            kbo_hub_get_work_area(hwnd, &work);
            int width = kbo_hub_rect_width(&rect);
            int height = kbo_hub_rect_height(&rect);
            if (work.right > work.left && work.bottom > work.top) {
                rect.left = work.left + ((work.right - work.left) - width) / 2;
                rect.top = work.top + ((work.bottom - work.top) - height) / 2;
                rect.right = rect.left + width;
                rect.bottom = rect.top + height;
            }
        }
        kbo_hub_clamp_rect_to_work_area(hwnd, &rect);
    }

    SetWindowPos(
        hwnd,
        NULL,
        rect.left,
        rect.top,
        kbo_hub_rect_width(&rect),
        kbo_hub_rect_height(&rect),
        SWP_NOZORDER | SWP_NOACTIVATE);
}
