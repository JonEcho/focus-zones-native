#include "tray.h"
#include "hotkey.h"
#include "cJSON.h"
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WM_TRAYICON (WM_USER + 1)
#define MENU_TOGGLE 1001
#define MENU_CHANGE_HOTKEY 1002
#define MENU_EXIT   1003

#define ICON_SIZE 16
#define ICON_COL_LEFT_END 5
#define ICON_COL_RIGHT_START 10
#define ICON_FOCUS_BOUNDARY 10

#define VK_CONTROL_CODE 0x11
#define VK_MENU_CODE    0x12
#define VK_SHIFT_CODE   0x10
#define VK_LWIN_CODE    0x5B
#define VK_RWIN_CODE    0x5C

typedef struct {
    DWORD vk_code;
    DWORD scan_code;
    DWORD flags;
    DWORD time;
    ULONG_PTR extra_info;
} KeyboardHookData;

static TrayState *global_tray_state = NULL;

static HICON create_custom_icon(void) {
    BYTE pixels[ICON_SIZE * ICON_SIZE * 4];
    BYTE dynamic_color[] = { 0x00, 0x78, 0xD4, 0xFF };
    BYTE focus_color[]   = { 0x00, 0xB9, 0xFF, 0xFF };
    BYTE static_color[]  = { 0x99, 0x99, 0x99, 0xFF };
    BYTE sep_color[]     = { 0x50, 0x50, 0x50, 0xFF };
    BYTE bg_color[]      = { 0x2D, 0x2D, 0x2D, 0xFF };

    for (int y = 0; y < ICON_SIZE; y++) {
        for (int x = 0; x < ICON_SIZE; x++) {
            int row = ICON_SIZE - 1 - y;
            int offset = (row * ICON_SIZE + x) * 4;
            BYTE *color;

            if (x == ICON_COL_LEFT_END || x == ICON_COL_RIGHT_START) {
                color = sep_color;
            } else if (y == 0 || y == ICON_SIZE - 1) {
                color = bg_color;
            } else if (x < ICON_COL_LEFT_END) {
                color = (y < ICON_FOCUS_BOUNDARY) ? focus_color : dynamic_color;
            } else if (x < ICON_COL_RIGHT_START) {
                color = static_color;
            } else {
                color = (y < ICON_FOCUS_BOUNDARY) ? focus_color : dynamic_color;
            }

            pixels[offset + 0] = color[0];
            pixels[offset + 1] = color[1];
            pixels[offset + 2] = color[2];
            pixels[offset + 3] = color[3];
        }
    }

    BYTE and_mask[ICON_SIZE * ICON_SIZE / 8];
    memset(and_mask, 0, sizeof(and_mask));

    return CreateIcon(
        GetModuleHandle(NULL),
        ICON_SIZE, ICON_SIZE,
        1, 32,
        and_mask, pixels
    );
}

static bool is_modifier_vk(DWORD vk) {
    return vk == VK_CONTROL_CODE || vk == VK_MENU_CODE ||
           vk == VK_SHIFT_CODE || vk == VK_LWIN_CODE || vk == VK_RWIN_CODE;
}

static void save_hotkey_to_config(const char *config_path, const char *new_hotkey) {
    FILE *file = fopen(config_path, "rb");
    if (!file) return;

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_text = malloc(length + 1);
    if (!json_text) { fclose(file); return; }
    fread(json_text, 1, length, file);
    json_text[length] = '\0';
    fclose(file);

    cJSON *root = cJSON_Parse(json_text);
    free(json_text);
    if (!root) return;

    cJSON *hotkey_item = cJSON_GetObjectItem(root, "toggle_hotkey");
    if (hotkey_item) {
        cJSON_SetValuestring(hotkey_item, new_hotkey);
    } else {
        cJSON_AddStringToObject(root, "toggle_hotkey", new_hotkey);
    }

    char *updated_json = cJSON_Print(root);
    cJSON_Delete(root);
    if (!updated_json) return;

    file = fopen(config_path, "w");
    if (file) {
        fputs(updated_json, file);
        fclose(file);
    }
    free(updated_json);
}

static void apply_new_hotkey(TrayState *state, const char *new_hotkey) {
    state->is_waiting_for_hotkey = false;

    if (state->keyboard_hook) {
        UnhookWindowsHookEx(state->keyboard_hook);
        state->keyboard_hook = NULL;
    }

    hotkey_unregister(HOTKEY_TOGGLE_ID);

    ParsedHotkey parsed;
    if (hotkey_parse(new_hotkey, &parsed) && hotkey_register(HOTKEY_TOGGLE_ID, &parsed)) {
        wchar_t balloon_message[128];
        size_t converted = 0;
        wchar_t wide_hotkey[64];
        mbstowcs_s(&converted, wide_hotkey, 64, new_hotkey, _TRUNCATE);

        for (wchar_t *ch = wide_hotkey; *ch; ch++) {
            *ch = (wchar_t)towupper(*ch);
        }
        _snwprintf_s(balloon_message, 128, _TRUNCATE, L"Hotkey changed to %s", wide_hotkey);
        tray_show_balloon(state, L"Focus Zones", balloon_message);

        if (state->config_path[0] != '\0') {
            save_hotkey_to_config(state->config_path, new_hotkey);
        }
    } else {
        tray_show_balloon(state, L"Error", L"Failed to register new hotkey");
    }

    tray_update(state, state->is_enabled);
}

static LRESULT CALLBACK keyboard_hook_proc(int code, WPARAM wparam, LPARAM lparam) {
    if (code >= 0 && wparam == WM_KEYDOWN && global_tray_state &&
        global_tray_state->is_waiting_for_hotkey) {
        KeyboardHookData *kb = (KeyboardHookData *)lparam;
        DWORD vk = kb->vk_code;

        if (!is_modifier_vk(vk)) {
            char parts[128] = "";
            if (GetAsyncKeyState(VK_CONTROL_CODE) & 0x8000) {
                strcat_s(parts, sizeof(parts), "ctrl+");
            }
            if (GetAsyncKeyState(VK_MENU_CODE) & 0x8000) {
                strcat_s(parts, sizeof(parts), "alt+");
            }
            if (GetAsyncKeyState(VK_SHIFT_CODE) & 0x8000) {
                strcat_s(parts, sizeof(parts), "shift+");
            }

            size_t parts_length = strlen(parts);
            if (parts_length > 0 && isalnum((int)vk)) {
                char key_char = (char)tolower((int)vk);
                char key_str[2] = { key_char, '\0' };
                strcat_s(parts, sizeof(parts), key_str);
                apply_new_hotkey(global_tray_state, parts);
                return 1;
            }
        }
    }
    return CallNextHookEx(NULL, code, wparam, lparam);
}

static void start_hotkey_capture(TrayState *state) {
    state->is_waiting_for_hotkey = true;
    global_tray_state = state;

    wcscpy_s(state->nid.szTip, 128, L"Press new hotkey...");
    state->nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &state->nid);

    state->keyboard_hook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        keyboard_hook_proc,
        GetModuleHandle(NULL),
        0
    );

    if (state->keyboard_hook) {
        tray_show_balloon(state, L"Change Hotkey",
            L"Press a new hotkey combo (e.g. Ctrl+Alt+X)");
    } else {
        state->is_waiting_for_hotkey = false;
        tray_show_balloon(state, L"Error", L"Failed to install keyboard hook");
        tray_update(state, state->is_enabled);
    }
}

static LRESULT CALLBACK tray_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    TrayState *state = (TrayState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (msg == WM_TRAYICON) {
        if (LOWORD(lparam) == WM_LBUTTONUP) {
            state->is_enabled = !state->is_enabled;
            tray_update(state, state->is_enabled);
        } else if (LOWORD(lparam) == WM_RBUTTONUP) {
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
            case MENU_CHANGE_HOTKEY:
                start_hotkey_capture(state);
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
    state->is_waiting_for_hotkey = false;
    state->keyboard_hook = NULL;
    global_tray_state = state;

    ZeroMemory(&state->nid, sizeof(state->nid));
    state->nid.cbSize = sizeof(NOTIFYICONDATAW);
    state->nid.hWnd = state->message_window;
    state->nid.uID = 1;
    state->nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    state->nid.uCallbackMessage = WM_TRAYICON;

    HICON custom_icon = create_custom_icon();
    state->nid.hIcon = custom_icon ? custom_icon : LoadIcon(NULL, IDI_APPLICATION);

    wcscpy_s(state->nid.szTip, 128, enabled ? L"Focus Zones (Active)" : L"Focus Zones (Paused)");
    Shell_NotifyIconW(NIM_ADD, &state->nid);
}

void tray_update(TrayState *state, bool enabled) {
    state->is_enabled = enabled;
    wcscpy_s(state->nid.szTip, 128, enabled ? L"Focus Zones (Active)" : L"Focus Zones (Paused)");
    Shell_NotifyIconW(NIM_MODIFY, &state->nid);
}

void tray_remove(TrayState *state) {
    if (state->keyboard_hook) {
        UnhookWindowsHookEx(state->keyboard_hook);
        state->keyboard_hook = NULL;
    }
    Shell_NotifyIconW(NIM_DELETE, &state->nid);
    if (state->nid.hIcon) {
        DestroyIcon(state->nid.hIcon);
    }
    if (state->message_window) {
        DestroyWindow(state->message_window);
        state->message_window = NULL;
    }
}

void tray_show_menu(TrayState *state) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (state->is_enabled ? MF_CHECKED : 0), MENU_TOGGLE, L"Enabled");
    AppendMenuW(menu, MF_STRING, MENU_CHANGE_HOTKEY, L"Change Hotkey...");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, MENU_EXIT, L"Exit");

    POINT cursor;
    GetCursorPos(&cursor);
    SetForegroundWindow(state->message_window);
    TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, cursor.x, cursor.y, 0, state->message_window, NULL);
    DestroyMenu(menu);
}

void tray_show_balloon(TrayState *state, const wchar_t *title, const wchar_t *message) {
    wcscpy_s(state->nid.szInfoTitle, 64, title);
    wcscpy_s(state->nid.szInfo, 256, message);
    state->nid.dwInfoFlags = NIIF_INFO;
    state->nid.uFlags = NIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &state->nid);
}

void tray_set_config_path(TrayState *state, const char *path) {
    strncpy(state->config_path, path, sizeof(state->config_path) - 1);
    state->config_path[sizeof(state->config_path) - 1] = '\0';
}
