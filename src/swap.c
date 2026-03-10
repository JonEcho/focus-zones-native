#include "swap.h"
#include <string.h>

#define SWAP_TARGET_ZONE_RATIO 0.5f

void swap_track_start(SwapTracker *tracker, HWND hwnd, Layout *layout) {
    RECT rect;
    window_get_rect(hwnd, &rect);
    int x_center = (rect.left + rect.right) / 2;

    Column *source_column = layout_find_column(layout, x_center);
    if (!source_column) {
        tracker->is_tracking = false;
        return;
    }

    tracker->hwnd = hwnd;
    strncpy(tracker->source_column_name, source_column->name, LAYOUT_NAME_MAX - 1);
    tracker->original_rect = rect;
    tracker->is_tracking = true;
}

static WindowInfo *find_most_overlapping(
    int target_top, int target_bottom,
    WindowInfo *windows, int count,
    HWND exclude_hwnd
) {
    WindowInfo *best = NULL;
    int best_overlap = 0;

    for (int i = 0; i < count; i++) {
        if (windows[i].hwnd == exclude_hwnd) continue;
        int overlap_top = target_top > windows[i].rect.top ? target_top : windows[i].rect.top;
        int overlap_bottom = target_bottom < windows[i].rect.bottom ? target_bottom : windows[i].rect.bottom;
        int overlap = overlap_bottom - overlap_top;
        if (overlap > best_overlap) {
            best_overlap = overlap;
            best = &windows[i];
        }
    }
    return best;
}

void swap_track_end(
    SwapTracker *tracker, HWND hwnd,
    Layout *layout, float focus_ratio, int zone_gap,
    const ResizeOps *ops
) {
    if (!tracker->is_tracking || tracker->hwnd != hwnd) return;
    tracker->is_tracking = false;

    RECT drop_rect;
    window_get_rect(hwnd, &drop_rect);
    int x_center = (drop_rect.left + drop_rect.right) / 2;

    Column *destination = layout_find_column(layout, x_center);
    if (!destination) return;
    if (strcmp(destination->name, tracker->source_column_name) == 0) return;

    WindowInfo windows[WINDOW_MAX_SIBLINGS];
    int count = window_find_in_column(destination, windows, WINDOW_MAX_SIBLINGS);

    WindowInfo *overlapping = find_most_overlapping(
        drop_rect.top, drop_rect.bottom, windows, count, hwnd
    );

    if (overlapping) {
        int target_height = overlapping->rect.bottom - overlapping->rect.top;
        float dead_zone_margin = target_height * (1.0f - SWAP_TARGET_ZONE_RATIO) / 2.0f;
        int zone_top = overlapping->rect.top + (int)dead_zone_margin;
        int zone_bottom = overlapping->rect.bottom - (int)dead_zone_margin;
        int drop_center = (drop_rect.top + drop_rect.bottom) / 2;

        if (drop_center >= zone_top && drop_center <= zone_bottom) {
            int orig_width = tracker->original_rect.right - tracker->original_rect.left;
            int orig_height = tracker->original_rect.bottom - tracker->original_rect.top;
            ops->set_pos(
                overlapping->hwnd,
                tracker->original_rect.left, tracker->original_rect.top,
                orig_width, orig_height
            );
        }
    }

    resize_single_occupant_columns(layout, focus_ratio, zone_gap, NULL, ops);
}
