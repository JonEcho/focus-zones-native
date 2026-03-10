#include <windows.h>
#include <stdio.h>

#include "config.h"
#include "layout.h"
#include "resize.h"
#include "swap.h"
#include "window.h"
#include "tray.h"
#include "overlay.h"
#include "hotkey.h"

#define MUTEX_NAME L"FocusZones_SingleInstance"
#define CONFIG_FILENAME "config.json"

typedef struct {
    FocusConfig *config;
    Layout layout;
    TrayState tray;
    OverlayState overlay;
    SwapTracker swap_tracker;
    DWORD last_event_ms;
} AppState;

static AppState app = {0};

static ResizeOps real_ops = {
    .set_pos = window_set_pos,
    .get_rect = window_get_rect,
    .get_min_size = window_get_min_size,
};

static void CALLBACK on_focus(
    HWINEVENTHOOK hook, DWORD event, HWND hwnd,
    LONG id_object, LONG id_child, DWORD event_thread, DWORD event_time
) {
    (void)hook; (void)event; (void)id_object; (void)id_child; (void)event_thread;

    if (!app.tray.is_enabled) return;

    DWORD now = event_time;
    if ((int)(now - app.last_event_ms) < app.config->debounce_ms) return;
    app.last_event_ms = now;

    if (!window_is_app(hwnd)) return;
    if (window_is_dialog(hwnd)) return;

    char exe_name[WINDOW_EXE_MAX];
    window_get_exe(hwnd, exe_name, WINDOW_EXE_MAX);
    if (config_is_ignored(app.config, exe_name)) return;

    Sleep(app.config->title_bar_click_delay_ms);

    if (!IsWindow(hwnd)) {
        resize_single_occupant_columns(
            &app.layout, app.config->focus_ratio,
            app.config->zone_gap_px, NULL, &real_ops
        );
        return;
    }

    RECT rect;
    window_get_rect(hwnd, &rect);
    int x_center = (rect.left + rect.right) / 2;

    Column *focused_column = layout_find_dynamic_column(&app.layout, x_center);
    if (focused_column) {
        WindowInfo siblings[WINDOW_MAX_SIBLINGS];
        int count = window_find_in_column(focused_column, siblings, WINDOW_MAX_SIBLINGS);
        resize_column(
            hwnd, siblings, count,
            app.config->focus_ratio, focused_column,
            app.config->zone_gap_px, &real_ops
        );
    }

    resize_single_occupant_columns(
        &app.layout, app.config->focus_ratio,
        app.config->zone_gap_px, focused_column, &real_ops
    );
}

static void CALLBACK on_move_size(
    HWINEVENTHOOK hook, DWORD event, HWND hwnd,
    LONG id_object, LONG id_child, DWORD event_thread, DWORD event_time
) {
    (void)hook; (void)id_object; (void)id_child; (void)event_thread; (void)event_time;

    if (!app.tray.is_enabled) return;
    if (!window_is_app(hwnd)) return;

    char exe_name[WINDOW_EXE_MAX];
    window_get_exe(hwnd, exe_name, WINDOW_EXE_MAX);
    if (config_is_ignored(app.config, exe_name)) return;

    if (event == EVENT_SYSTEM_MOVESIZESTART) {
        swap_track_start(&app.swap_tracker, hwnd, &app.layout);
    } else if (event == EVENT_SYSTEM_MOVESIZEEND) {
        swap_track_end(
            &app.swap_tracker, hwnd, &app.layout,
            app.config->focus_ratio, app.config->zone_gap_px, &real_ops
        );
    }
}

static char *build_config_path(char *buffer, int buffer_size) {
    GetModuleFileNameA(NULL, buffer, buffer_size);
    char *last_slash = strrchr(buffer, '\\');
    if (last_slash) *(last_slash + 1) = '\0';
    strncat(buffer, CONFIG_FILENAME, buffer_size - strlen(buffer) - 1);
    return buffer;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmd, int show) {
    (void)prev; (void)cmd; (void)show;

    HANDLE mutex = CreateMutexW(NULL, FALSE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    char config_path[MAX_PATH];
    build_config_path(config_path, MAX_PATH);
    app.config = config_load(config_path);

    layout_detect_monitors(&app.layout);
    for (int m = 0; m < app.layout.monitor_count; m++) {
        layout_auto_columns(&app.layout.monitors[m], app.config->layout_template);
    }

    ParsedHotkey hotkey;
    if (hotkey_parse(app.config->toggle_hotkey, &hotkey)) {
        hotkey_register(HOTKEY_TOGGLE_ID, &hotkey);
    }

    tray_create(&app.tray, instance, true);
    overlay_create(&app.overlay, instance);

    HWINEVENTHOOK focus_hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        NULL, on_focus, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );
    HWINEVENTHOOK move_hook = SetWinEventHook(
        EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND,
        NULL, on_move_size, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_HOTKEY && msg.wParam == HOTKEY_TOGGLE_ID) {
            app.tray.is_enabled = !app.tray.is_enabled;
            tray_update(&app.tray, app.tray.is_enabled);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWinEvent(focus_hook);
    UnhookWinEvent(move_hook);
    hotkey_unregister(HOTKEY_TOGGLE_ID);
    tray_remove(&app.tray);
    overlay_destroy(&app.overlay);
    config_free(app.config);
    CloseHandle(mutex);

    return 0;
}
