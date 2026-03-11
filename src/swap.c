#include "swap.h"
#include "config.h"
#include "hotkey.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define SWAP_TARGET_ZONE_RATIO 0.5f
#define DEBUG_LOG_PATH "C:\\Users\\jon\\focus_zones_debug.log"
#define DEBUG_ENABLE_PATH "C:\\Users\\jon\\focus_zones_debug.enable"

static void debug_log(const char *fmt, ...) {
    static int enabled = -1;
    if (enabled == -1) {
        FILE *flag = fopen(DEBUG_ENABLE_PATH, "r");
        enabled = (flag != NULL);
        if (flag) fclose(flag);
    }
    if (!enabled) return;

    FILE *f = fopen(DEBUG_LOG_PATH, "a");
    if (!f) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%02d:%02d:%02d.%03d] ",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);

    fprintf(f, "\n");
    fclose(f);
}

void swap_track_start(SwapTracker *tracker, HWND hwnd, Layout *layout) {
    RECT rect;
    window_get_rect(hwnd, &rect);
    int x_center = (rect.left + rect.right) / 2;

    Column *source_column = layout_find_column(layout, x_center);

    tracker->hwnd = hwnd;
    if (source_column) {
        strncpy(tracker->source_column_name, source_column->name, LAYOUT_NAME_MAX - 1);
    } else {
        tracker->source_column_name[0] = '\0';
    }
    tracker->original_rect = rect;
    tracker->is_tracking = true;

    SHORT raw_shift = GetAsyncKeyState(VK_SHIFT);
    bool async_shift = (raw_shift & 0x8000) != 0;
    bool ll_shift = keyboard_is_shift_held();
    tracker->shift_held = async_shift || ll_shift;

    debug_log("TRACK_START: hwnd=%p source_col='%s' x_center=%d async=0x%04X ll=%d shift_held=%d",
              (void *)hwnd,
              source_column ? source_column->name : "(none)",
              x_center, (unsigned)raw_shift, ll_shift, tracker->shift_held);
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

static void send_behind_siblings(HWND dragged, WindowInfo *windows, int count) {
    for (int i = 0; i < count; i++) {
        if (windows[i].hwnd != dragged) {
            SetWindowPos(windows[i].hwnd, HWND_TOP, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
}

void swap_track_end(
    SwapTracker *tracker, HWND hwnd,
    Layout *layout, float focus_ratio, int zone_gap,
    const ResizeOps *ops
) {
    if (!tracker->is_tracking || tracker->hwnd != hwnd) {
        debug_log("TRACK_END: BAIL — is_tracking=%d hwnd_match=%d (tracker=%p event=%p)",
                  tracker->is_tracking, tracker->hwnd == hwnd,
                  (void *)tracker->hwnd, (void *)hwnd);
        return;
    }
    tracker->is_tracking = false;

    RECT drop_rect;
    window_get_rect(hwnd, &drop_rect);
    int x_center = (drop_rect.left + drop_rect.right) / 2;

    Column *destination = layout_find_column(layout, x_center);
    if (!destination) {
        debug_log("TRACK_END: NO DESTINATION column for x_center=%d", x_center);
        return;
    }

    bool same_column = strcmp(destination->name, tracker->source_column_name) == 0;

    debug_log("TRACK_END: hwnd=%p drop=[%d,%d,%d,%d] x_center=%d dest='%s' src='%s' same_col=%d",
              (void *)hwnd, drop_rect.left, drop_rect.top, drop_rect.right, drop_rect.bottom,
              x_center, destination->name, tracker->source_column_name, same_column);

    if (same_column) {
        for (int poll = 0; poll < CONFIG_SWAP_POLL_LIMIT; poll++) {
            Sleep(CONFIG_SWAP_POLL_INTERVAL_MS);
            window_get_rect(hwnd, &drop_rect);
            x_center = (drop_rect.left + drop_rect.right) / 2;
            destination = layout_find_column(layout, x_center);
            if (!destination) {
                debug_log("TRACK_END: POLL %d — destination lost at x_center=%d", poll, x_center);
                return;
            }
            if (strcmp(destination->name, tracker->source_column_name) != 0) {
                same_column = false;
                debug_log("TRACK_END: POLL %d — FancyZones moved window to '%s'", poll, destination->name);
                break;
            }
        }
        if (same_column) {
            debug_log("TRACK_END: POLL exhausted — still same column '%s'", destination->name);
        }
    }

    SHORT raw_shift_end = GetAsyncKeyState(VK_SHIFT);
    bool async_end = (raw_shift_end & 0x8000) != 0;
    bool ll_end = keyboard_is_shift_held();
    bool shift_held = tracker->shift_held || async_end || ll_end;
    bool was_tracked = tracking_is_window_tracked(layout, hwnd);

    debug_log("TRACK_END: DECISION shift_start=%d async_end=%d ll_end=%d => shift_held=%d was_tracked=%d same_col=%d dest='%s'",
              tracker->shift_held, async_end, ll_end,
              shift_held, was_tracked, same_column, destination->name);

    if (same_column) {
        if (shift_held) {
            if (!was_tracked) {
                tracking_add_window(destination, hwnd);
                debug_log("TRACK_END: BRANCH same_col+shift → REGISTERED hwnd=%p in '%s'",
                          (void *)hwnd, destination->name);
            } else {
                debug_log("TRACK_END: BRANCH same_col+shift → already tracked, resizing");
            }
            WindowInfo windows[WINDOW_MAX_SIBLINGS];
            int count = window_get_tracked_in_column(destination, windows, WINDOW_MAX_SIBLINGS);
            debug_log("TRACK_END: resize_column count=%d in '%s'", count, destination->name);
            resize_column(hwnd, windows, count, focus_ratio, destination, zone_gap, ops);
            send_behind_siblings(hwnd, windows, count);
            resize_single_occupant_columns(layout, focus_ratio, zone_gap, destination, ops);
        } else {
            debug_log("TRACK_END: BRANCH same_col+NO_shift → IGNORED (no registration)");
        }
        return;
    }

    if (!shift_held) {
        if (was_tracked) {
            tracking_remove_window_from_all(layout, hwnd);
            resize_single_occupant_columns(layout, focus_ratio, zone_gap, NULL, ops);
            debug_log("TRACK_END: BRANCH diff_col+no_shift+was_tracked → UNTRACKED");
        } else {
            debug_log("TRACK_END: BRANCH diff_col+no_shift+not_tracked → IGNORED");
        }
        return;
    }

    debug_log("TRACK_END: BRANCH diff_col+shift → REGISTER in '%s' + attempt swap", destination->name);
    tracking_remove_window_from_all(layout, hwnd);
    tracking_add_window(destination, hwnd);

    WindowInfo windows[WINDOW_MAX_SIBLINGS];
    int count = window_get_tracked_in_column(destination, windows, WINDOW_MAX_SIBLINGS);
    debug_log("TRACK_END: tracked_in_dest=%d", count);

    WindowInfo *overlapping = find_most_overlapping(
        drop_rect.top, drop_rect.bottom, windows, count, hwnd
    );

    if (overlapping) {
        debug_log("TRACK_END: SWAP candidate hwnd=%p", (void *)overlapping->hwnd);
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

            int source_x_center = (tracker->original_rect.left + tracker->original_rect.right) / 2;
            Column *source_column = layout_find_column(layout, source_x_center);
            if (source_column) {
                tracking_remove_window_from_all(layout, overlapping->hwnd);
                tracking_add_window(source_column, overlapping->hwnd);
            }
        }
    }

    /* Re-query after swap may have changed tracking, then resize destination */
    count = window_get_tracked_in_column(destination, windows, WINDOW_MAX_SIBLINGS);
    resize_column(hwnd, windows, count, focus_ratio, destination, zone_gap, ops);
    send_behind_siblings(hwnd, windows, count);
    resize_single_occupant_columns(layout, focus_ratio, zone_gap, destination, ops);
}
