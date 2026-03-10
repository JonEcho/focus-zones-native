#pragma once

#include <windows.h>
#include <shellapi.h>
#include <stdbool.h>

typedef struct {
    NOTIFYICONDATAW nid;
    HWND message_window;
    bool is_enabled;
    bool is_waiting_for_hotkey;
    HHOOK keyboard_hook;
    char config_path[260];
} TrayState;

void tray_create(TrayState *state, HINSTANCE instance, bool enabled);
void tray_update(TrayState *state, bool enabled);
void tray_remove(TrayState *state);
void tray_show_menu(TrayState *state);
void tray_show_balloon(TrayState *state, const wchar_t *title, const wchar_t *message);
void tray_set_config_path(TrayState *state, const char *path);
