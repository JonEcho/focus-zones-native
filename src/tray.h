#pragma once

#include <windows.h>
#include <shellapi.h>
#include <stdbool.h>

typedef struct {
    NOTIFYICONDATAW nid;
    HWND message_window;
    bool is_enabled;
} TrayState;

void tray_create(TrayState *state, HINSTANCE instance, bool enabled);
void tray_update(TrayState *state, bool enabled);
void tray_remove(TrayState *state);
void tray_show_menu(TrayState *state);
