#include "tray.h"
#include <shellapi.h>

#define WM_TRAYICON (WM_USER + 1)
#define MENU_TOGGLE 1001
#define MENU_RELOAD 1002
#define MENU_EXIT   1003

static LRESULT CALLBACK tray_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    TrayState *state = (TrayState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (msg == WM_TRAYICON) {
        if (LOWORD(lparam) == WM_RBUTTONUP) {
            tray_show_menu(state);
        }
        return 0;
    }
    if (msg == WM_COMMAND) {
        switch (LOWORD(wparam)) {
            case MENU_TOGGLE:
                state->is_enabled = !state->is_enabled;
                tray_update(state, state->is_enabled);
                break;
            case MENU_EXIT:
                PostQuitMessage(0);
                break;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void tray_create(TrayState *state, HINSTANCE instance, bool enabled) {
    WNDCLASSW wc = {
        .lpfnWndProc = tray_wndproc,
        .hInstance = instance,
        .lpszClassName = L"FocusZonesTray",
    };
    RegisterClassW(&wc);

    state->message_window = CreateWindowExW(
        0, L"FocusZonesTray", L"FocusZones",
        0, 0, 0, 0, 0, HWND_MESSAGE, NULL, instance, NULL
    );
    SetWindowLongPtrW(state->message_window, GWLP_USERDATA, (LONG_PTR)state);

    state->is_enabled = enabled;
    ZeroMemory(&state->nid, sizeof(state->nid));
    state->nid.cbSize = sizeof(NOTIFYICONDATAW);
    state->nid.hWnd = state->message_window;
    state->nid.uID = 1;
    state->nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    state->nid.uCallbackMessage = WM_TRAYICON;
    state->nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(state->nid.szTip, 128, enabled ? L"Focus Zones (Active)" : L"Focus Zones (Paused)");
    Shell_NotifyIconW(NIM_ADD, &state->nid);
}

void tray_update(TrayState *state, bool enabled) {
    state->is_enabled = enabled;
    wcscpy_s(state->nid.szTip, 128, enabled ? L"Focus Zones (Active)" : L"Focus Zones (Paused)");
    Shell_NotifyIconW(NIM_MODIFY, &state->nid);
}

void tray_remove(TrayState *state) {
    Shell_NotifyIconW(NIM_DELETE, &state->nid);
    if (state->message_window) {
        DestroyWindow(state->message_window);
        state->message_window = NULL;
    }
}

void tray_show_menu(TrayState *state) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (state->is_enabled ? MF_CHECKED : 0), MENU_TOGGLE, L"Enabled");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, MENU_EXIT, L"Exit");

    POINT cursor;
    GetCursorPos(&cursor);
    SetForegroundWindow(state->message_window);
    TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, cursor.x, cursor.y, 0, state->message_window, NULL);
    DestroyMenu(menu);
}
