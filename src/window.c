#include "window.h"
#include "layout.h"
#include <dwmapi.h>
#include <psapi.h>

#define DWMWA_CLOAKED 14
#define SWP_FLAGS (SWP_NOZORDER | SWP_NOACTIVATE)
#define MIN_WINDOW_SIZE 50

bool window_is_app(HWND hwnd) {
    if (!IsWindowVisible(hwnd)) return false;

    LONG extended_style = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if ((extended_style & WS_EX_TOOLWINDOW) && !(extended_style & WS_EX_APPWINDOW)) {
        return false;
    }

    DWORD cloaked = 0;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked) return false;

    int title_length = GetWindowTextLengthW(hwnd);
    return title_length > 0;
}

bool window_is_dialog(HWND hwnd) {
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    LONG extended_style = GetWindowLongW(hwnd, GWL_EXSTYLE);
    return (style & WS_POPUP) != 0 || (extended_style & WS_EX_DLGMODALFRAME) != 0;
}

void window_get_rect(HWND hwnd, RECT *rect) {
    GetWindowRect(hwnd, rect);
}

void window_get_exe(HWND hwnd, char *buffer, int buffer_size) {
    buffer[0] = '\0';
    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (!process) return;

    char full_path[MAX_PATH];
    DWORD path_size = MAX_PATH;
    if (QueryFullProcessImageNameA(process, 0, full_path, &path_size)) {
        const char *last_slash = strrchr(full_path, '\\');
        const char *basename = last_slash ? last_slash + 1 : full_path;
        strncpy(buffer, basename, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
    }
    CloseHandle(process);
}

void window_get_min_size(HWND hwnd, int *min_width, int *min_height) {
    MINMAXINFO mmi = {0};
    SendMessage(hwnd, WM_GETMINMAXINFO, 0, (LPARAM)&mmi);
    *min_width = mmi.ptMinTrackSize.x;
    *min_height = mmi.ptMinTrackSize.y;
}

void window_set_pos(HWND hwnd, int x, int y, int width, int height) {
    SetWindowPos(hwnd, NULL, x, y, width, height, SWP_FLAGS);
}

typedef struct {
    const Column *column;
    WindowInfo *results;
    int max_results;
    int count;
} EnumColumnContext;

static BOOL CALLBACK enum_column_callback(HWND hwnd, LPARAM lparam) {
    EnumColumnContext *context = (EnumColumnContext *)lparam;
    if (context->count >= context->max_results) return FALSE;
    if (!window_is_app(hwnd)) return TRUE;
    if (window_is_dialog(hwnd)) return TRUE;

    RECT rect;
    GetWindowRect(hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width < MIN_WINDOW_SIZE || height < MIN_WINDOW_SIZE) return TRUE;

    int x_center = (rect.left + rect.right) / 2;
    if (x_center >= context->column->bounds.left && x_center < context->column->bounds.right) {
        context->results[context->count].hwnd = hwnd;
        context->results[context->count].rect = rect;
        context->count++;
    }
    return TRUE;
}

static int compare_window_top(const void *a, const void *b) {
    const WindowInfo *wa = (const WindowInfo *)a;
    const WindowInfo *wb = (const WindowInfo *)b;
    return wa->rect.top - wb->rect.top;
}

int window_find_in_column(const Column *column, WindowInfo *results, int max_results) {
    EnumColumnContext context = { column, results, max_results, 0 };
    EnumWindows(enum_column_callback, (LPARAM)&context);
    qsort(results, context.count, sizeof(WindowInfo), compare_window_top);
    return context.count;
}
