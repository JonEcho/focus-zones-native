#include "hotkey.h"
#include "config.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

bool hotkey_parse(const char *hotkey_string, ParsedHotkey *result) {
    result->modifiers = 0;
    result->virtual_key = 0;

    char buffer[CONFIG_MAX_HOTKEY];
    strncpy(buffer, hotkey_string, CONFIG_MAX_HOTKEY - 1);
    buffer[CONFIG_MAX_HOTKEY - 1] = '\0';

    for (char *c = buffer; *c; c++) *c = (char)tolower(*c);

    char *token = strtok(buffer, "+");
    while (token) {
        if (strcmp(token, "ctrl") == 0) {
            result->modifiers |= MOD_CONTROL;
        } else if (strcmp(token, "alt") == 0) {
            result->modifiers |= MOD_ALT;
        } else if (strcmp(token, "shift") == 0) {
            result->modifiers |= MOD_SHIFT;
        } else if (strcmp(token, "win") == 0) {
            result->modifiers |= MOD_WIN;
        } else if (strlen(token) == 1 && isalnum(token[0])) {
            result->virtual_key = (UINT)toupper(token[0]);
        }
        token = strtok(NULL, "+");
    }

    return result->virtual_key != 0 || result->modifiers != 0;
}

bool hotkey_register(int id, const ParsedHotkey *hotkey) {
    return RegisterHotKey(NULL, id, hotkey->modifiers, hotkey->virtual_key) != 0;
}

void hotkey_unregister(int id) {
    UnregisterHotKey(NULL, id);
}

static volatile bool g_shift_pressed = false;

static LRESULT CALLBACK ll_keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT *)lParam;
        if (kb->vkCode == VK_LSHIFT || kb->vkCode == VK_RSHIFT) {
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                g_shift_pressed = true;
            } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                g_shift_pressed = false;
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

HHOOK keyboard_hook_install(void) {
    return SetWindowsHookExW(WH_KEYBOARD_LL, ll_keyboard_proc, NULL, 0);
}

void keyboard_hook_remove(HHOOK hook) {
    if (hook) UnhookWindowsHookEx(hook);
}

bool keyboard_is_shift_held(void) {
    return g_shift_pressed;
}
