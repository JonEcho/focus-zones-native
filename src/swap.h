#pragma once

#include "layout.h"
#include "window.h"
#include "resize.h"

typedef struct {
    HWND hwnd;
    char source_column_name[LAYOUT_NAME_MAX];
    RECT original_rect;
    bool is_tracking;
    bool shift_held;
} SwapTracker;

void swap_track_start(SwapTracker *tracker, HWND hwnd, Layout *layout);
void swap_track_end(
    SwapTracker *tracker, HWND hwnd,
    Layout *layout, float focus_ratio, int zone_gap,
    const ResizeOps *ops
);
