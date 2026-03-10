#include "layout.h"
#include <shellscalingapi.h>
#include <stdio.h>
#include <string.h>

#define ULTRAWIDE_THRESHOLD 3440
#define STANDARD_THRESHOLD 1920

typedef struct {
    Layout *layout;
} EnumMonitorContext;

static BOOL CALLBACK enum_monitor_callback(HMONITOR monitor, HDC hdc, LPRECT rect, LPARAM lparam) {
    (void)hdc;
    (void)rect;
    EnumMonitorContext *context = (EnumMonitorContext *)lparam;
    Layout *layout = context->layout;

    if (layout->monitor_count >= LAYOUT_MAX_MONITORS) return FALSE;

    Monitor *mon = &layout->monitors[layout->monitor_count];
    mon->handle = monitor;

    MONITORINFO info = { .cbSize = sizeof(MONITORINFO) };
    GetMonitorInfoW(monitor, &info);
    mon->work_area = info.rcWork;

    UINT dpi_x = 96, dpi_y = 96;
    GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);
    mon->dpi = dpi_x;

    layout->monitor_count++;
    return TRUE;
}

void layout_detect_monitors(Layout *layout) {
    layout->monitor_count = 0;
    EnumMonitorContext context = { layout };
    EnumDisplayMonitors(NULL, NULL, enum_monitor_callback, (LPARAM)&context);
}

void layout_auto_columns(Monitor *monitor, const char *layout_template) {
    int width = monitor->work_area.right - monitor->work_area.left;
    int top = monitor->work_area.top;
    int bottom = monitor->work_area.bottom;
    int left = monitor->work_area.left;

    const char *effective_template = layout_template;
    if (strcmp(layout_template, "auto") == 0) {
        if (width >= ULTRAWIDE_THRESHOLD) {
            effective_template = "3-col";
        } else if (width >= STANDARD_THRESHOLD) {
            effective_template = "2-col";
        } else {
            effective_template = "single";
        }
    }

    if (strcmp(effective_template, "3-col") == 0) {
        int quarter = width / 4;
        int half = width / 2;
        monitor->column_count = 3;

        strncpy(monitor->columns[0].name, "left", LAYOUT_NAME_MAX);
        monitor->columns[0].bounds = (RECT){ left, top, left + quarter, bottom };
        monitor->columns[0].is_dynamic = true;

        strncpy(monitor->columns[1].name, "center", LAYOUT_NAME_MAX);
        monitor->columns[1].bounds = (RECT){ left + quarter, top, left + quarter + half, bottom };
        monitor->columns[1].is_dynamic = false;

        strncpy(monitor->columns[2].name, "right", LAYOUT_NAME_MAX);
        monitor->columns[2].bounds = (RECT){ left + quarter + half, top, left + width, bottom };
        monitor->columns[2].is_dynamic = true;
    } else if (strcmp(effective_template, "2-col") == 0) {
        int half = width / 2;
        monitor->column_count = 2;

        strncpy(monitor->columns[0].name, "left", LAYOUT_NAME_MAX);
        monitor->columns[0].bounds = (RECT){ left, top, left + half, bottom };
        monitor->columns[0].is_dynamic = true;

        strncpy(monitor->columns[1].name, "right", LAYOUT_NAME_MAX);
        monitor->columns[1].bounds = (RECT){ left + half, top, left + width, bottom };
        monitor->columns[1].is_dynamic = true;
    } else {
        monitor->column_count = 1;
        strncpy(monitor->columns[0].name, "full", LAYOUT_NAME_MAX);
        monitor->columns[0].bounds = (RECT){ left, top, left + width, bottom };
        monitor->columns[0].is_dynamic = true;
    }
}

Column *layout_find_column(Layout *layout, int x_center) {
    for (int m = 0; m < layout->monitor_count; m++) {
        Monitor *monitor = &layout->monitors[m];
        for (int c = 0; c < monitor->column_count; c++) {
            Column *column = &monitor->columns[c];
            if (x_center >= column->bounds.left && x_center < column->bounds.right) {
                return column;
            }
        }
    }
    return NULL;
}

Column *layout_find_dynamic_column(Layout *layout, int x_center) {
    Column *column = layout_find_column(layout, x_center);
    if (column && column->is_dynamic) return column;
    return NULL;
}
