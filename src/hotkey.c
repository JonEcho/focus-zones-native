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
