#pragma once

#include <windows.h>
#include <stdbool.h>

#define CONFIG_MAX_IGNORED 64
#define CONFIG_MAX_HOTKEY 32
#define CONFIG_MAX_PATH 260
#define CONFIG_MAX_LAYOUT 16
#define CONFIG_MAX_COLUMNS 8
#define CONFIG_MAX_COLUMN_NAME 32
#define CONFIG_SWAP_POLL_INTERVAL_MS 50
#define CONFIG_SWAP_POLL_LIMIT 20

typedef struct {
    char name[CONFIG_MAX_COLUMN_NAME];
    int x_min;
    int x_max;
    bool dynamic_resize;
} ConfigColumn;

typedef struct {
    float focus_ratio;
    int debounce_ms;
    int zone_gap_px;
    int title_bar_click_delay_ms;
    char toggle_hotkey[CONFIG_MAX_HOTKEY];
    char layout_template[CONFIG_MAX_LAYOUT];
    char *ignored_executables[CONFIG_MAX_IGNORED];
    int ignored_count;
    ConfigColumn columns[CONFIG_MAX_COLUMNS];
    int column_count;
} FocusConfig;

FocusConfig *config_load(const char *path);
void config_free(FocusConfig *config);
bool config_is_ignored(const FocusConfig *config, const char *exe_name);
