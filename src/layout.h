#pragma once

#include <windows.h>
#include <stdbool.h>
#include "config.h"

#define LAYOUT_MAX_COLUMNS 8
#define LAYOUT_MAX_MONITORS 4
#define LAYOUT_NAME_MAX 64

typedef struct Column {
    char name[LAYOUT_NAME_MAX];
    RECT bounds;
    bool is_dynamic;
    TrackedWindow tracked_windows[MAXIMUM_TRACKED_WINDOWS];
    int tracked_window_count;
} Column;

typedef struct Monitor {
    HMONITOR handle;
    RECT work_area;
    UINT dpi;
    Column columns[LAYOUT_MAX_COLUMNS];
    int column_count;
} Monitor;

typedef struct Layout {
    Monitor monitors[LAYOUT_MAX_MONITORS];
    int monitor_count;
} Layout;

void layout_detect_monitors(Layout *layout);
void layout_auto_columns(Monitor *monitor, const char *layout_template);
Column *layout_find_column(Layout *layout, int x_center);
Column *layout_find_dynamic_column(Layout *layout, int x_center);
