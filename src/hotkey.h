#pragma once

#include <windows.h>
#include <stdbool.h>

#define HOTKEY_TOGGLE_ID 1

typedef struct {
    UINT modifiers;
    UINT virtual_key;
} ParsedHotkey;

bool hotkey_parse(const char *hotkey_string, ParsedHotkey *result);
bool hotkey_register(int id, const ParsedHotkey *hotkey);
void hotkey_unregister(int id);

HHOOK keyboard_hook_install(void);
void keyboard_hook_remove(HHOOK hook);
bool keyboard_is_shift_held(void);
