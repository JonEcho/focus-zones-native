#pragma once

#include "layout.h"
#include "window.h"

typedef void (*SetPosFn)(HWND hwnd, int x, int y, int w, int h);
typedef void (*GetRectFn)(HWND hwnd, RECT *rect);
typedef void (*GetMinSizeFn)(HWND hwnd, int *min_w, int *min_h);

typedef struct {
    SetPosFn set_pos;
    GetRectFn get_rect;
    GetMinSizeFn get_min_size;
} ResizeOps;

void resize_column(
    HWND focused_hwnd,
    WindowInfo *siblings, int sibling_count,
    float focus_ratio, const Column *column,
    int zone_gap,
    const ResizeOps *ops
);

void resize_single_occupant_columns(
    Layout *layout,
    float focus_ratio,
    int zone_gap,
    const Column *skip_column,
    const ResizeOps *ops
);
