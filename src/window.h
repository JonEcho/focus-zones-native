#pragma once

#include <windows.h>
#include <stdbool.h>

#define WINDOW_MAX_SIBLINGS 16
#define WINDOW_EXE_MAX 260

typedef struct Column Column;
typedef struct Layout Layout;

typedef struct {
    HWND hwnd;
    RECT rect;
} WindowInfo;

bool window_is_app(HWND hwnd);
bool window_is_dialog(HWND hwnd);
void window_get_rect(HWND hwnd, RECT *rect);
void window_get_exe(HWND hwnd, char *buffer, int buffer_size);
void window_get_min_size(HWND hwnd, int *min_width, int *min_height);
void window_set_pos(HWND hwnd, int x, int y, int width, int height);
int window_find_in_column(const Column *column, WindowInfo *results, int max_results);

bool tracking_add_window(Column *column, HWND window_handle);
void tracking_remove_window(Column *column, HWND window_handle);
void tracking_remove_window_from_all(Layout *layout, HWND window_handle);
bool tracking_is_window_tracked(const Layout *layout, HWND window_handle);
Column *tracking_find_column_for_window(Layout *layout, HWND window_handle);
int window_get_tracked_in_column(Column *column, WindowInfo *results, int maximum_results);

