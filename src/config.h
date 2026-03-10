#pragma once

#include <windows.h>
#include <stdbool.h>

#define CONFIG_MAX_IGNORED 64
#define CONFIG_MAX_HOTKEY 32
#define CONFIG_MAX_PATH 260
#define CONFIG_MAX_LAYOUT 16

typedef struct {
    float focus_ratio;
    int debounce_ms;
    int zone_gap_px;
    char toggle_hotkey[CONFIG_MAX_HOTKEY];
    char layout_template[CONFIG_MAX_LAYOUT];
    char *ignored_executables[CONFIG_MAX_IGNORED];
    int ignored_count;
} FocusConfig;

FocusConfig *config_load(const char *path);
void config_free(FocusConfig *config);
bool config_is_ignored(const FocusConfig *config, const char *exe_name);
