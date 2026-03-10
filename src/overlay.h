#pragma once

#include <windows.h>
#include "layout.h"

typedef struct {
    HWND overlay_window;
    bool is_visible;
} OverlayState;

void overlay_create(OverlayState *state, HINSTANCE instance);
void overlay_show(OverlayState *state, const Column *column);
void overlay_hide(OverlayState *state);
void overlay_destroy(OverlayState *state);
