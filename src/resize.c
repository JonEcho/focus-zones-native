#include "resize.h"
#include <stdlib.h>

static int max_int(int a, int b) { return a > b ? a : b; }

void resize_column(
    HWND focused_hwnd,
    WindowInfo *siblings, int sibling_count,
    float focus_ratio, const Column *column,
    int zone_gap,
    const ResizeOps *ops
) {
    if (sibling_count <= 0) return;

    int column_top = column->bounds.top;
    int column_left = column->bounds.left;
    int column_width = column->bounds.right - column->bounds.left;
    int column_height = column->bounds.bottom - column->bounds.top;

    if (sibling_count == 1) {
        ops->set_pos(siblings[0].hwnd, column_left, column_top, column_width, column_height);
        return;
    }

    int gap_count = sibling_count - 1;
    int usable = column_height - (zone_gap * gap_count);
    int unfocused_count = sibling_count - 1;
    int target_unfocused_height = (int)(usable * (1.0f - focus_ratio)) / max_int(unfocused_count, 1);

    int *actual_heights = calloc(sibling_count, sizeof(int));
    if (!actual_heights) return;

    int total_unfocused = 0;
    for (int i = 0; i < sibling_count; i++) {
        if (siblings[i].hwnd == focused_hwnd) continue;

        int min_width = 0, min_height = 0;
        ops->get_min_size(siblings[i].hwnd, &min_width, &min_height);
        int clamped_height = max_int(target_unfocused_height, min_height);

        actual_heights[i] = clamped_height;
        total_unfocused += clamped_height;
    }

    int focused_height = max_int(usable - total_unfocused, 50);
    for (int i = 0; i < sibling_count; i++) {
        if (siblings[i].hwnd == focused_hwnd) {
            actual_heights[i] = focused_height;
            break;
        }
    }

    int current_y = column_top;
    for (int i = 0; i < sibling_count; i++) {
        ops->set_pos(siblings[i].hwnd, column_left, current_y, column_width, actual_heights[i]);
        current_y += actual_heights[i] + zone_gap;
    }

    free(actual_heights);
}

void resize_single_occupant_columns(
    Layout *layout,
    float focus_ratio,
    int zone_gap,
    const Column *skip_column,
    const ResizeOps *ops
) {
    WindowInfo siblings[WINDOW_MAX_SIBLINGS];
    for (int m = 0; m < layout->monitor_count; m++) {
        Monitor *monitor = &layout->monitors[m];
        for (int c = 0; c < monitor->column_count; c++) {
            Column *column = &monitor->columns[c];
            if (column == skip_column) continue;
            if (!column->is_dynamic) continue;

            int count = window_find_in_column(column, siblings, WINDOW_MAX_SIBLINGS);
            if (count == 1) {
                resize_column(siblings[0].hwnd, siblings, 1, focus_ratio, column, zone_gap, ops);
            }
        }
    }
}
