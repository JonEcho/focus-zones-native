#include "overlay.h"

#define OVERLAY_CLASS L"FocusZonesOverlay"
#define OVERLAY_ALPHA 60
#define OVERLAY_COLOR RGB(80, 140, 255)

static LRESULT CALLBACK overlay_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        HBRUSH brush = CreateSolidBrush(OVERLAY_COLOR);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void overlay_create(OverlayState *state, HINSTANCE instance) {
    WNDCLASSW wc = {
        .lpfnWndProc = overlay_wndproc,
        .hInstance = instance,
        .lpszClassName = OVERLAY_CLASS,
    };
    RegisterClassW(&wc);

    state->overlay_window = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        OVERLAY_CLASS, L"",
        WS_POPUP,
        0, 0, 0, 0,
        NULL, NULL, instance, NULL
    );
    SetLayeredWindowAttributes(state->overlay_window, 0, OVERLAY_ALPHA, LWA_ALPHA);
    state->is_visible = false;
}

void overlay_show(OverlayState *state, const Column *column) {
    int x = column->bounds.left;
    int y = column->bounds.top;
    int width = column->bounds.right - column->bounds.left;
    int height = column->bounds.bottom - column->bounds.top;
    SetWindowPos(state->overlay_window, HWND_TOPMOST, x, y, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    state->is_visible = true;
}

void overlay_hide(OverlayState *state) {
    ShowWindow(state->overlay_window, SW_HIDE);
    state->is_visible = false;
}

void overlay_destroy(OverlayState *state) {
    if (state->overlay_window) {
        DestroyWindow(state->overlay_window);
        state->overlay_window = NULL;
    }
}
