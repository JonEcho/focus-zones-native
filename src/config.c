#include "config.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void config_set_defaults(FocusConfig *config) {
    config->focus_ratio = 0.75f;
    config->debounce_ms = 150;
    config->zone_gap_px = 8;
    config->title_bar_click_delay_ms = 150;
    strncpy(config->toggle_hotkey, "ctrl+alt+z", CONFIG_MAX_HOTKEY - 1);
    strncpy(config->layout_template, "auto", CONFIG_MAX_LAYOUT - 1);
    config->ignored_count = 0;
    config->column_count = 0;
}

FocusConfig *config_load(const char *path) {
    FocusConfig *config = calloc(1, sizeof(FocusConfig));
    if (!config) return NULL;
    config_set_defaults(config);

    FILE *file = fopen(path, "rb");
    if (!file) return config;

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_text = malloc(length + 1);
    if (!json_text) { fclose(file); return config; }
    fread(json_text, 1, length, file);
    json_text[length] = '\0';
    fclose(file);

    cJSON *root = cJSON_Parse(json_text);
    free(json_text);
    if (!root) return config;

    cJSON *ratio = cJSON_GetObjectItem(root, "focus_ratio");
    if (cJSON_IsNumber(ratio)) config->focus_ratio = (float)ratio->valuedouble;

    cJSON *debounce = cJSON_GetObjectItem(root, "debounce_ms");
    if (cJSON_IsNumber(debounce)) config->debounce_ms = debounce->valueint;

    cJSON *gap = cJSON_GetObjectItem(root, "zone_gap_px");
    if (cJSON_IsNumber(gap)) config->zone_gap_px = gap->valueint;

    cJSON *click_delay = cJSON_GetObjectItem(root, "title_bar_click_delay_ms");
    if (cJSON_IsNumber(click_delay)) config->title_bar_click_delay_ms = click_delay->valueint;

    cJSON *hotkey = cJSON_GetObjectItem(root, "toggle_hotkey");
    if (cJSON_IsString(hotkey)) {
        strncpy(config->toggle_hotkey, hotkey->valuestring, CONFIG_MAX_HOTKEY - 1);
    }

    cJSON *layout = cJSON_GetObjectItem(root, "layout");
    if (cJSON_IsString(layout)) {
        strncpy(config->layout_template, layout->valuestring, CONFIG_MAX_LAYOUT - 1);
    }

    cJSON *ignored = cJSON_GetObjectItem(root, "ignore_exe");
    if (cJSON_IsArray(ignored)) {
        int count = cJSON_GetArraySize(ignored);
        if (count > CONFIG_MAX_IGNORED) count = CONFIG_MAX_IGNORED;
        for (int i = 0; i < count; i++) {
            cJSON *item = cJSON_GetArrayItem(ignored, i);
            if (cJSON_IsString(item)) {
                config->ignored_executables[config->ignored_count] = _strdup(item->valuestring);
                config->ignored_count++;
            }
        }
    }

    cJSON *columns = cJSON_GetObjectItem(root, "columns");
    if (cJSON_IsArray(columns)) {
        int col_count = cJSON_GetArraySize(columns);
        if (col_count > CONFIG_MAX_COLUMNS) col_count = CONFIG_MAX_COLUMNS;
        for (int i = 0; i < col_count; i++) {
            cJSON *col = cJSON_GetArrayItem(columns, i);
            if (!cJSON_IsObject(col)) continue;

            ConfigColumn *cc = &config->columns[config->column_count];

            cJSON *name = cJSON_GetObjectItem(col, "name");
            if (cJSON_IsString(name)) {
                strncpy(cc->name, name->valuestring, CONFIG_MAX_COLUMN_NAME - 1);
            }

            cJSON *xmin = cJSON_GetObjectItem(col, "x_min");
            if (cJSON_IsNumber(xmin)) cc->x_min = xmin->valueint;

            cJSON *xmax = cJSON_GetObjectItem(col, "x_max");
            if (cJSON_IsNumber(xmax)) cc->x_max = xmax->valueint;

            cJSON *dynamic = cJSON_GetObjectItem(col, "dynamic_resize");
            cc->dynamic_resize = cJSON_IsTrue(dynamic);

            config->column_count++;
        }
    }

    cJSON_Delete(root);
    return config;
}

void config_free(FocusConfig *config) {
    if (!config) return;
    for (int i = 0; i < config->ignored_count; i++) {
        free(config->ignored_executables[i]);
    }
    free(config);
}

bool config_is_ignored(const FocusConfig *config, const char *exe_name) {
    for (int i = 0; i < config->ignored_count; i++) {
        if (_stricmp(config->ignored_executables[i], exe_name) == 0) {
            return true;
        }
    }
    return false;
}
