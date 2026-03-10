#pragma once

#include <windows.h>
#include <stdbool.h>

#define WINDOW_MAX_SIBLINGS 16
#define WINDOW_EXE_MAX 260

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
