#include <stdio.h>
#include <string.h>
#include "layout.h"
#include "window.h"
#include "resize.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("  PASS: %s\n", message); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s (line %d)\n", message, __LINE__); \
    } \
} while(0)

#define ASSERT_INT_EQ(actual, expected, message) do { \
    tests_run++; \
    if ((actual) == (expected)) { \
        tests_passed++; \
        printf("  PASS: %s\n", message); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s — expected %d, got %d (line %d)\n", \
               message, (expected), (actual), __LINE__); \
    } \
} while(0)

/* ── Off-screen window factory ──────────────────────────────────── */

static const char OFFSCREEN_CLASS[] = "FocusZonesTestWindow";
static BOOL class_registered = FALSE;

static void register_test_window_class(HINSTANCE instance) {
    if (class_registered) return;
    WNDCLASSA window_class = {0};
    window_class.lpfnWndProc = DefWindowProcA;
    window_class.hInstance = instance;
    window_class.lpszClassName = OFFSCREEN_CLASS;
    RegisterClassA(&window_class);
    class_registered = TRUE;
}

static HWND create_offscreen_window(HINSTANCE instance) {
    register_test_window_class(instance);
    HWND hwnd = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        OFFSCREEN_CLASS,
        "TestWindow",
        WS_POPUP,
        -10000, -10000, 400, 300,
        NULL, NULL, instance, NULL
    );
    ShowWindow(hwnd, SW_SHOWNA);
    return hwnd;
}

/* ── Real ResizeOps (actual Win32 calls) ─────────────────────────── */

static ResizeOps real_ops = {
    .set_pos = window_set_pos,
    .get_rect = window_get_rect,
    .get_min_size = window_get_min_size,
};

/* ── Helpers ─────────────────────────────────────────────────────── */

static Column build_column(const char *name, int left, int top, int right, int bottom) {
    Column column = {0};
    strncpy(column.name, name, LAYOUT_NAME_MAX - 1);
    column.bounds = (RECT){left, top, right, bottom};
    column.is_dynamic = true;
    column.tracked_window_count = 0;
    return column;
}

/* ── E2E: tracking with real HWNDs ──────────────────────────────── */

static void test_real_window_can_be_tracked(HINSTANCE instance) {
    printf("test_real_window_can_be_tracked\n");
    HWND hwnd = create_offscreen_window(instance);

    Column column = build_column("left", -10000, -10000, -9000, -9000);
    bool added = tracking_add_window(&column, hwnd);

    ASSERT(added == true, "real HWND tracked successfully");
    ASSERT(IsWindow(hwnd), "real HWND passes IsWindow");
    ASSERT(column.tracked_windows[0].window_handle == hwnd, "stored handle matches");

    DestroyWindow(hwnd);
}

static void test_real_window_appears_in_tracked_query(HINSTANCE instance) {
    printf("test_real_window_appears_in_tracked_query\n");
    HWND hwnd = create_offscreen_window(instance);

    Column column = build_column("left", -10000, -10000, -9000, -9000);
    tracking_add_window(&column, hwnd);

    WindowInfo results[4];
    int count = window_get_tracked_in_column(&column, results, 4);

    ASSERT_INT_EQ(count, 1, "tracked query returns 1 result");
    ASSERT(results[0].hwnd == hwnd, "result HWND matches tracked window");

    DestroyWindow(hwnd);
}

static void test_destroyed_window_pruned_from_tracked(HINSTANCE instance) {
    printf("test_destroyed_window_pruned_from_tracked\n");
    HWND hwnd = create_offscreen_window(instance);

    Column column = build_column("left", -10000, -10000, -9000, -9000);
    tracking_add_window(&column, hwnd);

    DestroyWindow(hwnd);

    WindowInfo results[4];
    int count = window_get_tracked_in_column(&column, results, 4);

    ASSERT_INT_EQ(count, 0, "destroyed window pruned from results");
    ASSERT(column.tracked_windows[0].is_tracked == false, "is_tracked set to false after prune");
}

static void test_untracked_window_excluded_from_query(HINSTANCE instance) {
    printf("test_untracked_window_excluded_from_query\n");
    HWND tracked = create_offscreen_window(instance);
    HWND untracked = create_offscreen_window(instance);

    Column column = build_column("left", -10000, -10000, -9000, -9000);
    tracking_add_window(&column, tracked);
    tracking_add_window(&column, untracked);
    tracking_remove_window(&column, untracked);

    WindowInfo results[4];
    int count = window_get_tracked_in_column(&column, results, 4);

    ASSERT_INT_EQ(count, 1, "only tracked window returned");
    ASSERT(results[0].hwnd == tracked, "returned HWND is the tracked one");

    DestroyWindow(tracked);
    DestroyWindow(untracked);
}

/* ── E2E: focus guard with real HWNDs ───────────────────────────── */

static void test_focus_guard_with_real_windows(HINSTANCE instance) {
    printf("test_focus_guard_with_real_windows\n");
    HWND tracked = create_offscreen_window(instance);
    HWND untracked = create_offscreen_window(instance);

    Column columns[1];
    columns[0] = build_column("left", -10000, -10000, -9000, -9000);
    tracking_add_window(&columns[0], tracked);

    Layout layout = {0};
    layout.monitor_count = 1;
    layout.monitors[0].column_count = 1;
    layout.monitors[0].columns[0] = columns[0];

    ASSERT(tracking_is_window_tracked(&layout, tracked) == true,
           "tracked real window passes guard");
    ASSERT(tracking_is_window_tracked(&layout, untracked) == false,
           "untracked real window blocked by guard");

    DestroyWindow(tracked);
    DestroyWindow(untracked);
}

/* ── E2E: resize with real windows at actual pixel dimensions ──── */

static void test_single_real_window_fills_column(HINSTANCE instance) {
    printf("test_single_real_window_fills_column\n");
    HWND hwnd = create_offscreen_window(instance);

    Column column = build_column("left", -10000, -10000, -9040, -8920);
    int expected_width = 960;
    int expected_height = 1080;

    WindowInfo siblings[1];
    siblings[0].hwnd = hwnd;
    window_get_rect(hwnd, &siblings[0].rect);

    resize_column(hwnd, siblings, 1, 0.75f, &column, 8, &real_ops);

    RECT actual;
    window_get_rect(hwnd, &actual);
    int actual_width = actual.right - actual.left;
    int actual_height = actual.bottom - actual.top;

    ASSERT_INT_EQ(actual.left, -10000, "window placed at column left");
    ASSERT_INT_EQ(actual.top, -10000, "window placed at column top");
    ASSERT_INT_EQ(actual_width, expected_width, "window width fills column");
    ASSERT_INT_EQ(actual_height, expected_height, "window height fills column");

    DestroyWindow(hwnd);
}

static void test_two_real_windows_ratio_split(HINSTANCE instance) {
    printf("test_two_real_windows_ratio_split\n");
    HWND focused = create_offscreen_window(instance);
    HWND unfocused = create_offscreen_window(instance);

    int col_left = -10000;
    int col_top = -10000;
    int col_right = -9040;  /* width: 960 */
    int col_bottom = -8920; /* height: 1080 */
    int zone_gap = 8;
    float focus_ratio = 0.75f;
    int column_height = col_bottom - col_top;

    Column column = build_column("left", col_left, col_top, col_right, col_bottom);

    WindowInfo siblings[2];
    siblings[0].hwnd = focused;
    window_get_rect(focused, &siblings[0].rect);
    siblings[1].hwnd = unfocused;
    window_get_rect(unfocused, &siblings[1].rect);

    resize_column(focused, siblings, 2, focus_ratio, &column, zone_gap, &real_ops);

    RECT focused_rect, unfocused_rect;
    window_get_rect(focused, &focused_rect);
    window_get_rect(unfocused, &unfocused_rect);

    int focused_height = focused_rect.bottom - focused_rect.top;
    int unfocused_height = unfocused_rect.bottom - unfocused_rect.top;
    int gap_between = unfocused_rect.top - focused_rect.bottom;
    int total_covered = focused_height + gap_between + unfocused_height;

    ASSERT_INT_EQ(focused_rect.left, col_left, "focused at column left");
    ASSERT_INT_EQ(unfocused_rect.left, col_left, "unfocused at column left");
    ASSERT_INT_EQ(gap_between, zone_gap, "gap between windows equals zone_gap");
    ASSERT_INT_EQ(total_covered, column_height, "windows + gap cover full column");
    ASSERT(focused_height > unfocused_height, "focused window taller than unfocused");

    int usable = column_height - zone_gap;
    int expected_unfocused = (int)(usable * (1.0f - focus_ratio));
    int expected_focused = usable - expected_unfocused;
    ASSERT_INT_EQ(focused_height, expected_focused, "focused height matches ratio calc");
    ASSERT_INT_EQ(unfocused_height, expected_unfocused, "unfocused height matches ratio calc");

    DestroyWindow(focused);
    DestroyWindow(unfocused);
}

static void test_three_real_windows_edges_meet(HINSTANCE instance) {
    printf("test_three_real_windows_edges_meet\n");
    HWND window_a = create_offscreen_window(instance);
    HWND window_b = create_offscreen_window(instance);
    HWND window_c = create_offscreen_window(instance);

    int col_left = -10000;
    int col_top = -10000;
    int col_right = -9040;
    int col_bottom = -8920;
    int zone_gap = 8;
    int column_height = col_bottom - col_top;

    Column column = build_column("center", col_left, col_top, col_right, col_bottom);

    WindowInfo siblings[3];
    siblings[0].hwnd = window_a;
    window_get_rect(window_a, &siblings[0].rect);
    siblings[1].hwnd = window_b;
    window_get_rect(window_b, &siblings[1].rect);
    siblings[2].hwnd = window_c;
    window_get_rect(window_c, &siblings[2].rect);

    resize_column(window_b, siblings, 3, 0.75f, &column, zone_gap, &real_ops);

    RECT rect_a, rect_b, rect_c;
    window_get_rect(window_a, &rect_a);
    window_get_rect(window_b, &rect_b);
    window_get_rect(window_c, &rect_c);

    int height_a = rect_a.bottom - rect_a.top;
    int height_b = rect_b.bottom - rect_b.top;
    int height_c = rect_c.bottom - rect_c.top;

    int gap_ab = rect_b.top - rect_a.bottom;
    int gap_bc = rect_c.top - rect_b.bottom;
    int total = height_a + gap_ab + height_b + gap_bc + height_c;

    ASSERT_INT_EQ(rect_a.top, col_top, "first window starts at column top");
    ASSERT_INT_EQ(gap_ab, zone_gap, "gap between window A and B");
    ASSERT_INT_EQ(gap_bc, zone_gap, "gap between window B and C");
    ASSERT_INT_EQ(total, column_height, "three windows + gaps fill column");
    ASSERT(height_b > height_a, "focused (B) is taller than unfocused (A)");
    ASSERT_INT_EQ(height_a, height_c, "unfocused windows have equal height");

    DestroyWindow(window_a);
    DestroyWindow(window_b);
    DestroyWindow(window_c);
}

/* ── E2E: on_focus flow (tracking-based column lookup) ────────── */

static void test_on_focus_finds_column_via_tracking_not_position(HINSTANCE instance) {
    printf("test_on_focus_finds_column_via_tracking_not_position\n");

    HWND window = create_offscreen_window(instance);

    int col_left = -10000;
    int col_top = -10000;
    int col_right = -9040;
    int col_bottom = -8920;

    Column columns[2];
    columns[0] = build_column("left", col_left, col_top, col_right, col_bottom);
    columns[1] = build_column("right", -9040, -10000, -8080, -8920);

    tracking_add_window(&columns[0], window);

    Layout layout = {0};
    layout.monitor_count = 1;
    layout.monitors[0].column_count = 2;
    layout.monitors[0].columns[0] = columns[0];
    layout.monitors[0].columns[1] = columns[1];

    Column *found = tracking_find_column_for_window(&layout, window);

    ASSERT(found != NULL, "column found via tracking lookup");
    ASSERT(strcmp(found->name, "left") == 0,
           "correct column found regardless of window position");

    WindowInfo siblings[WINDOW_MAX_SIBLINGS];
    int count = window_get_tracked_in_column(found, siblings, WINDOW_MAX_SIBLINGS);
    ASSERT_INT_EQ(count, 1, "found 1 tracked sibling");

    resize_column(window, siblings, count, 0.75f, found, 8, &real_ops);

    RECT actual;
    window_get_rect(window, &actual);
    int actual_width = actual.right - actual.left;
    int actual_height = actual.bottom - actual.top;

    ASSERT_INT_EQ(actual.left, col_left, "window placed at tracked column left");
    ASSERT_INT_EQ(actual_width, 960, "window fills tracked column width");
    ASSERT_INT_EQ(actual_height, 1080, "window fills tracked column height");

    DestroyWindow(window);
}

static void test_full_focus_flow_two_tracked_windows(HINSTANCE instance) {
    printf("test_full_focus_flow_two_tracked_windows\n");

    HWND window_a = create_offscreen_window(instance);
    HWND window_b = create_offscreen_window(instance);

    int col_left = -10000;
    int col_top = -10000;
    int col_right = -9040;
    int col_bottom = -8920;
    int zone_gap = 8;
    float focus_ratio = 0.75f;
    int column_height = col_bottom - col_top;

    Column columns[1];
    columns[0] = build_column("left", col_left, col_top, col_right, col_bottom);
    tracking_add_window(&columns[0], window_a);
    tracking_add_window(&columns[0], window_b);

    Layout layout = {0};
    layout.monitor_count = 1;
    layout.monitors[0].column_count = 1;
    layout.monitors[0].columns[0] = columns[0];

    /* Simulate: window_a gets focus */
    HWND focused = window_a;

    ASSERT(tracking_is_window_tracked(&layout, focused) == true,
           "focused window passes tracking guard");

    Column *found = tracking_find_column_for_window(&layout, focused);
    ASSERT(found != NULL, "column found for focused window");

    WindowInfo siblings[WINDOW_MAX_SIBLINGS];
    int count = window_get_tracked_in_column(found, siblings, WINDOW_MAX_SIBLINGS);
    ASSERT_INT_EQ(count, 2, "both tracked windows returned as siblings");

    resize_column(focused, siblings, count, focus_ratio, found, zone_gap, &real_ops);

    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        DispatchMessage(&msg);
    }

    RECT rect_a, rect_b;
    window_get_rect(window_a, &rect_a);
    window_get_rect(window_b, &rect_b);

    int height_a = rect_a.bottom - rect_a.top;
    int height_b = rect_b.bottom - rect_b.top;

    /* Determine which is on top vs bottom */
    RECT *top_rect = (rect_a.top < rect_b.top) ? &rect_a : &rect_b;
    RECT *bottom_rect = (rect_a.top < rect_b.top) ? &rect_b : &rect_a;
    int top_height = bottom_rect == &rect_b ? height_a : height_b;
    int bottom_height = bottom_rect == &rect_b ? height_b : height_a;
    int gap = bottom_rect->top - top_rect->bottom;
    int total = top_height + gap + bottom_height;

    ASSERT(height_a > height_b, "focused window (A) is taller than unfocused (B)");
    ASSERT_INT_EQ(gap, zone_gap, "gap between windows matches zone_gap");
    ASSERT_INT_EQ(total, column_height, "both windows + gap fill entire column");

    ASSERT_INT_EQ(top_rect->top, col_top, "top window starts at column top");

    int usable = column_height - zone_gap;
    int expected_unfocused = (int)(usable * (1.0f - focus_ratio));
    int expected_focused = usable - expected_unfocused;
    ASSERT_INT_EQ(height_a, expected_focused, "focused height = usable * ratio");
    ASSERT_INT_EQ(height_b, expected_unfocused, "unfocused height = usable * (1 - ratio)");

    DestroyWindow(window_a);
    DestroyWindow(window_b);
}

static void test_swap_query_only_returns_tracked_windows(HINSTANCE instance) {
    printf("test_swap_query_only_returns_tracked_windows\n");

    HWND tracked_a = create_offscreen_window(instance);
    HWND tracked_b = create_offscreen_window(instance);
    HWND untracked = create_offscreen_window(instance);

    Column column = build_column("center", -10000, -10000, -9040, -8920);
    tracking_add_window(&column, tracked_a);
    tracking_add_window(&column, tracked_b);
    /* untracked is NOT added — simulates window that happens to be in the column area */

    WindowInfo results[WINDOW_MAX_SIBLINGS];
    int count = window_get_tracked_in_column(&column, results, WINDOW_MAX_SIBLINGS);

    ASSERT_INT_EQ(count, 2, "only 2 tracked windows returned, not 3");

    bool found_untracked = false;
    for (int i = 0; i < count; i++) {
        if (results[i].hwnd == untracked) found_untracked = true;
    }
    ASSERT(found_untracked == false, "untracked window never appears in results");

    DestroyWindow(tracked_a);
    DestroyWindow(tracked_b);
    DestroyWindow(untracked);
}


/* ── E2E: swap_track_start → swap_track_end registration flow ──── */

#include "swap.h"

static HWND create_offscreen_window_at(HINSTANCE instance, int x, int y, int w, int h) {
    register_test_window_class(instance);
    HWND hwnd = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        OFFSCREEN_CLASS, "TestWindow", WS_POPUP,
        x, y, w, h, NULL, NULL, instance, NULL
    );
    ShowWindow(hwnd, SW_SHOWNA);
    return hwnd;
}

static Layout build_test_layout(void) {
    Layout layout = {0};
    layout.monitor_count = 1;
    layout.monitors[0].column_count = 3;
    layout.monitors[0].columns[0] = build_column("left", -15000, -10000, -14040, -8920);
    layout.monitors[0].columns[1] = build_column("center", -14040, -10000, -12080, -8920);
    layout.monitors[0].columns[1].is_dynamic = false;
    layout.monitors[0].columns[2] = build_column("right", -12080, -10000, -11120, -8920);
    return layout;
}

static void test_swap_track_start_works_when_window_outside_columns(HINSTANCE instance) {
    printf("test_swap_track_start_works_when_window_outside_columns\n");
    HWND window = create_offscreen_window_at(instance, -20000, -20000, 400, 300);
    Layout layout = build_test_layout();
    SwapTracker tracker = {0};

    swap_track_start(&tracker, window, &layout);

    ASSERT(tracker.is_tracking == true,
           "tracker starts even when window is outside all columns");
    ASSERT(tracker.hwnd == window, "tracker records correct hwnd");
    ASSERT(tracker.source_column_name[0] == '\0',
           "source_column_name empty when window outside all columns");
    ASSERT(tracker.shift_held == false,
           "shift_held is false when no key pressed in test");

    DestroyWindow(window);
}

static void test_shift_drag_from_outside_to_column_registers(HINSTANCE instance) {
    printf("test_shift_drag_from_outside_to_column_registers\n");
    int col_center_x = (-15000 + -14040) / 2;
    HWND window = create_offscreen_window_at(instance, -20000, -20000, 400, 300);
    Layout layout = build_test_layout();
    SwapTracker tracker = {0};

    swap_track_start(&tracker, window, &layout);
    tracker.shift_held = true;  /* Simulate shift held during drag */
    /* Move window into the left column before drop */
    window_set_pos(window, col_center_x - 200, -10000, 400, 300);
    swap_track_end(&tracker, window, &layout, 0.75f, 8, &real_ops);

    ASSERT(tracking_is_window_tracked(&layout, window) == true,
           "window registered after shift+drag from outside to column");

    Column *found = tracking_find_column_for_window(&layout, window);
    ASSERT(found != NULL, "window found in a column after registration");
    ASSERT(strcmp(found->name, "left") == 0, "window registered in left column specifically");

    DestroyWindow(window);
}

static void test_shift_drag_within_same_column_registers(HINSTANCE instance) {
    printf("test_shift_drag_within_same_column_registers\n");
    int col_center_x = (-15000 + -14040) / 2;
    HWND window = create_offscreen_window_at(instance, col_center_x - 200, -10000, 400, 300);
    Layout layout = build_test_layout();
    SwapTracker tracker = {0};

    swap_track_start(&tracker, window, &layout);
    tracker.shift_held = true;  /* Simulate shift held during drag */
    /* Window stays in same column */
    swap_track_end(&tracker, window, &layout, 0.75f, 8, &real_ops);

    ASSERT(tracking_is_window_tracked(&layout, window) == true,
           "window registered when shift+drag stays in same column");

    DestroyWindow(window);
}

static void test_full_user_flow_register_focus_resize(HINSTANCE instance) {
    printf("test_full_user_flow_register_focus_resize\n");
    int col_left = -15000;
    int col_top = -10000;
    int col_right = -14040;
    int col_bottom = -8920;
    int column_width = col_right - col_left;
    int column_height = col_bottom - col_top;
    int col_center_x = (col_left + col_right) / 2;

    HWND window_a = create_offscreen_window_at(instance, -20000, -20000, 400, 300);
    HWND window_b = create_offscreen_window_at(instance, -21000, -21000, 400, 300);
    Layout layout = build_test_layout();
    SwapTracker tracker = {0};

    /* Shift+drag window_a into left column */
    swap_track_start(&tracker, window_a, &layout);
    tracker.shift_held = true;
    window_set_pos(window_a, col_center_x - 200, col_top, 400, 300);
    swap_track_end(&tracker, window_a, &layout, 0.75f, 8, &real_ops);

    ASSERT(tracking_is_window_tracked(&layout, window_a) == true,
           "window_a tracked after shift+drag");
    Column *a_col = tracking_find_column_for_window(&layout, window_a);
    ASSERT(a_col != NULL && strcmp(a_col->name, "left") == 0,
           "window_a registered in left column specifically");

    /* Shift+drag window_b into same left column */
    swap_track_start(&tracker, window_b, &layout);
    tracker.shift_held = true;
    window_set_pos(window_b, col_center_x - 200, col_top + 500, 400, 300);
    swap_track_end(&tracker, window_b, &layout, 0.75f, 8, &real_ops);

    ASSERT(tracking_is_window_tracked(&layout, window_b) == true,
           "window_b tracked after shift+drag");
    Column *b_col = tracking_find_column_for_window(&layout, window_b);
    ASSERT(b_col != NULL && strcmp(b_col->name, "left") == 0,
           "window_b registered in left column specifically");

    /* Simulate on_focus for window_a */
    Column *focused_column = tracking_find_column_for_window(&layout, window_a);
    ASSERT(focused_column != NULL, "column found for focused window_a");

    if (focused_column) {
        WindowInfo siblings[WINDOW_MAX_SIBLINGS];
        int count = window_get_tracked_in_column(focused_column, siblings, WINDOW_MAX_SIBLINGS);
        ASSERT_INT_EQ(count, 2, "both windows returned as siblings");

        resize_column(window_a, siblings, count, 0.75f, focused_column, 8, &real_ops);

        RECT rect_a;
        window_get_rect(window_a, &rect_a);
        int width_a = rect_a.right - rect_a.left;

        ASSERT_INT_EQ(width_a, column_width, "window_a fills column width after resize");
    }

    DestroyWindow(window_a);
    DestroyWindow(window_b);
}

/* ── E2E: Cross-Column Swap ─────────────────────────────────────── */

static void test_cross_column_swap_exchanges_positions(HINSTANCE instance) {
    printf("test_cross_column_swap_exchanges_positions\n");
    Layout layout = build_test_layout();
    Column *left_column = &layout.monitors[0].columns[0];
    Column *right_column = &layout.monitors[0].columns[2];

    int left_center_x = (left_column->bounds.left + left_column->bounds.right) / 2;
    int right_center_x = (right_column->bounds.left + right_column->bounds.right) / 2;
    int column_top = left_column->bounds.top;
    int column_height = left_column->bounds.bottom - left_column->bounds.top;
    int column_width = left_column->bounds.right - left_column->bounds.left;

    HWND window_a = create_offscreen_window_at(
        instance, left_center_x - column_width / 2, column_top,
        column_width, column_height
    );
    HWND window_b = create_offscreen_window_at(
        instance, right_center_x - column_width / 2, column_top,
        column_width, column_height
    );

    tracking_add_window(left_column, window_a);
    tracking_add_window(right_column, window_b);

    RECT original_a_rect;
    window_get_rect(window_a, &original_a_rect);

    SwapTracker tracker = {0};
    swap_track_start(&tracker, window_a, &layout);
    tracker.shift_held = true;

    window_set_pos(window_a, right_center_x - column_width / 2, column_top,
                   column_width, column_height);
    swap_track_end(&tracker, window_a, &layout, 0.75f, 8, &real_ops);

    Column *column_of_a = tracking_find_column_for_window(&layout, window_a);
    Column *column_of_b = tracking_find_column_for_window(&layout, window_b);

    ASSERT(column_of_a != NULL, "window_a found in a column after swap");
    ASSERT(column_of_b != NULL, "window_b found in a column after swap");

    if (column_of_a) {
        ASSERT(strcmp(column_of_a->name, "right") == 0,
               "window_a now tracked in right column");
    }
    if (column_of_b) {
        ASSERT(strcmp(column_of_b->name, "left") == 0,
               "window_b now tracked in left column");
    }

    RECT swapped_b_rect;
    window_get_rect(window_b, &swapped_b_rect);
    ASSERT_INT_EQ(swapped_b_rect.left, original_a_rect.left,
                  "window_b moved to window_a original left");
    ASSERT_INT_EQ(swapped_b_rect.top, original_a_rect.top,
                  "window_b moved to window_a original top");

    if (column_of_a) {
        WindowInfo siblings_right[WINDOW_MAX_SIBLINGS];
        int right_count = window_get_tracked_in_column(
            column_of_a, siblings_right, WINDOW_MAX_SIBLINGS
        );
        resize_column(window_a, siblings_right, right_count,
                      0.75f, column_of_a, 8, &real_ops);
        RECT resized_a;
        window_get_rect(window_a, &resized_a);
        int right_col_width = right_column->bounds.right - right_column->bounds.left;
        int right_col_height = right_column->bounds.bottom - right_column->bounds.top;
        ASSERT_INT_EQ(resized_a.right - resized_a.left, right_col_width,
                      "single occupant fills right column width");
        ASSERT_INT_EQ(resized_a.bottom - resized_a.top, right_col_height,
                      "single occupant fills right column height");
    }

    if (column_of_b) {
        WindowInfo siblings_left[WINDOW_MAX_SIBLINGS];
        int left_count = window_get_tracked_in_column(
            column_of_b, siblings_left, WINDOW_MAX_SIBLINGS
        );
        resize_column(window_b, siblings_left, left_count,
                      0.75f, column_of_b, 8, &real_ops);
        RECT resized_b;
        window_get_rect(window_b, &resized_b);
        int left_col_width = left_column->bounds.right - left_column->bounds.left;
        int left_col_height = left_column->bounds.bottom - left_column->bounds.top;
        ASSERT_INT_EQ(resized_b.right - resized_b.left, left_col_width,
                      "single occupant fills left column width");
        ASSERT_INT_EQ(resized_b.bottom - resized_b.top, left_col_height,
                      "single occupant fills left column height");
    }

    DestroyWindow(window_a);
    DestroyWindow(window_b);
}

static void test_plain_drag_to_different_column_untracks_window(HINSTANCE instance) {
    printf("test_plain_drag_to_different_column_untracks_window\n");
    Layout layout = build_test_layout();
    Column *left_column = &layout.monitors[0].columns[0];
    Column *right_column = &layout.monitors[0].columns[2];

    int left_center_x = (left_column->bounds.left + left_column->bounds.right) / 2;
    int right_center_x = (right_column->bounds.left + right_column->bounds.right) / 2;
    int column_top = left_column->bounds.top;

    HWND window = create_offscreen_window_at(
        instance, left_center_x - 200, column_top, 400, 300
    );
    tracking_add_window(left_column, window);

    ASSERT(tracking_is_window_tracked(&layout, window) == true,
           "window is tracked before plain drag");

    SwapTracker tracker = {0};
    swap_track_start(&tracker, window, &layout);

    window_set_pos(window, right_center_x - 200, column_top, 400, 300);
    swap_track_end(&tracker, window, &layout, 0.75f, 8, &real_ops);

    ASSERT(tracking_is_window_tracked(&layout, window) == false,
           "window untracked after plain drag to different column");

    DestroyWindow(window);
}

static void test_cross_column_drag_outside_dead_zone_no_swap(HINSTANCE instance) {
    printf("test_cross_column_drag_outside_dead_zone_no_swap\n");
    Layout layout = build_test_layout();
    Column *left_column = &layout.monitors[0].columns[0];
    Column *right_column = &layout.monitors[0].columns[2];

    int left_center_x = (left_column->bounds.left + left_column->bounds.right) / 2;
    int right_center_x = (right_column->bounds.left + right_column->bounds.right) / 2;
    int column_top = right_column->bounds.top;
    int column_bottom = right_column->bounds.bottom;
    int half_height = (column_bottom - column_top) / 2;
    int column_width = left_column->bounds.right - left_column->bounds.left;

    HWND window_a = create_offscreen_window_at(
        instance, left_center_x - column_width / 2, column_top,
        column_width, half_height
    );
    HWND window_b = create_offscreen_window_at(
        instance, right_center_x - column_width / 2, column_bottom - half_height,
        column_width, half_height
    );

    tracking_add_window(left_column, window_a);
    tracking_add_window(right_column, window_b);

    RECT original_b_rect;
    window_get_rect(window_b, &original_b_rect);

    int dead_zone_margin_top = original_b_rect.top + half_height / 4;
    int drop_center_y = dead_zone_margin_top - 1;
    int drop_top = drop_center_y - half_height / 2;

    SwapTracker tracker = {0};
    swap_track_start(&tracker, window_a, &layout);
    tracker.shift_held = true;

    window_set_pos(window_a, right_center_x - column_width / 2, drop_top,
                   column_width, half_height);
    swap_track_end(&tracker, window_a, &layout, 0.75f, 8, &real_ops);

    Column *column_of_a = tracking_find_column_for_window(&layout, window_a);
    Column *column_of_b = tracking_find_column_for_window(&layout, window_b);

    ASSERT(column_of_a != NULL && strcmp(column_of_a->name, "right") == 0,
           "window_a registered in right column");
    ASSERT(column_of_b != NULL && strcmp(column_of_b->name, "right") == 0,
           "window_b still in right column (no swap — not moved to left)");

    DestroyWindow(window_a);
    DestroyWindow(window_b);
}

static void test_shift_drag_to_static_column_registers(HINSTANCE instance) {
    printf("test_shift_drag_to_static_column_registers\n");
    Layout layout = build_test_layout();
    Column *left_column = &layout.monitors[0].columns[0];
    Column *center_column = &layout.monitors[0].columns[1];

    int left_center_x = (left_column->bounds.left + left_column->bounds.right) / 2;
    int center_center_x = (center_column->bounds.left + center_column->bounds.right) / 2;
    int column_top = left_column->bounds.top;

    HWND window = create_offscreen_window_at(
        instance, left_center_x - 200, column_top, 400, 300
    );
    tracking_add_window(left_column, window);

    SwapTracker tracker = {0};
    swap_track_start(&tracker, window, &layout);
    tracker.shift_held = true;

    window_set_pos(window, center_center_x - 200, column_top, 400, 300);
    swap_track_end(&tracker, window, &layout, 0.75f, 8, &real_ops);

    Column *found = tracking_find_column_for_window(&layout, window);
    ASSERT(found != NULL, "window found in a column after shift+drag to static");
    if (found) {
        ASSERT(strcmp(found->name, "center") == 0,
               "window registered in static center column");
    }

    DestroyWindow(window);
}

static void test_remaining_window_auto_resizes_when_sibling_moves_out(HINSTANCE instance) {
    printf("test_remaining_window_auto_resizes_when_sibling_moves_out\n");
    Layout layout = build_test_layout();
    Column *left_column = &layout.monitors[0].columns[0];
    Column *right_column = &layout.monitors[0].columns[2];

    int left_center_x = (left_column->bounds.left + left_column->bounds.right) / 2;
    int right_center_x = (right_column->bounds.left + right_column->bounds.right) / 2;
    int col_top = left_column->bounds.top;
    int col_height = left_column->bounds.bottom - left_column->bounds.top;
    int col_width = left_column->bounds.right - left_column->bounds.left;

    /* Track two windows in left column */
    HWND window_a = create_offscreen_window_at(
        instance, left_center_x - col_width / 2, col_top, col_width, col_height / 2
    );
    HWND window_b = create_offscreen_window_at(
        instance, left_center_x - col_width / 2, col_top + col_height / 2, col_width, col_height / 2
    );
    tracking_add_window(left_column, window_a);
    tracking_add_window(left_column, window_b);

    /* Apply initial 75/25 resize so windows are NOT full-column */
    WindowInfo siblings[WINDOW_MAX_SIBLINGS];
    int count = window_get_tracked_in_column(left_column, siblings, WINDOW_MAX_SIBLINGS);
    ASSERT_INT_EQ(count, 2, "two tracked windows in left column");
    resize_column(window_a, siblings, count, 0.75f, left_column, 8, &real_ops);

    RECT before_a;
    window_get_rect(window_a, &before_a);
    int before_height = before_a.bottom - before_a.top;
    ASSERT(before_height < col_height, "window_a is NOT full height before move-out");

    /* Shift+drag window_b from left to right column */
    SwapTracker tracker = {0};
    swap_track_start(&tracker, window_b, &layout);
    tracker.shift_held = true;
    window_set_pos(window_b, right_center_x - col_width / 2, col_top, col_width, col_height);
    swap_track_end(&tracker, window_b, &layout, 0.75f, 8, &real_ops);

    /* Verify: window_a auto-resized to fill left column (no click required) */
    RECT after_a;
    window_get_rect(window_a, &after_a);
    ASSERT_INT_EQ(after_a.left, left_column->bounds.left,
                  "remaining window snapped to column left");
    ASSERT_INT_EQ(after_a.top, left_column->bounds.top,
                  "remaining window snapped to column top");
    ASSERT_INT_EQ(after_a.right - after_a.left, col_width,
                  "remaining window fills column width");
    ASSERT_INT_EQ(after_a.bottom - after_a.top, col_height,
                  "remaining window fills column height");

    DestroyWindow(window_a);
    DestroyWindow(window_b);
}

static void test_snap_resizes_both_windows_immediately(HINSTANCE instance) {
    printf("test_snap_resizes_both_windows_immediately\n");
    Layout layout = build_test_layout();
    Column *left_column = &layout.monitors[0].columns[0];

    int col_left = left_column->bounds.left;
    int col_top = left_column->bounds.top;
    int col_width = left_column->bounds.right - left_column->bounds.left;
    int col_height = left_column->bounds.bottom - left_column->bounds.top;
    int left_center_x = (left_column->bounds.left + left_column->bounds.right) / 2;
    int zone_gap = 8;
    float focus_ratio = 0.75f;

    /* Track window_a in left column, fills the column */
    HWND window_a = create_offscreen_window_at(
        instance, col_left, col_top, col_width, col_height
    );
    tracking_add_window(left_column, window_a);

    /* Snap window_b into the same column via shift+drag */
    HWND window_b = create_offscreen_window_at(
        instance, -20000, -20000, 400, 300
    );
    SwapTracker tracker = {0};
    swap_track_start(&tracker, window_b, &layout);
    tracker.shift_held = true;
    window_set_pos(window_b, left_center_x - 200, col_top + col_height / 2, 400, 300);
    swap_track_end(&tracker, window_b, &layout, focus_ratio, zone_gap, &real_ops);

    /* Both windows should be resized immediately — no click needed */
    RECT rect_a, rect_b;
    window_get_rect(window_a, &rect_a);
    window_get_rect(window_b, &rect_b);

    int height_a = rect_a.bottom - rect_a.top;
    int height_b = rect_b.bottom - rect_b.top;

    ASSERT_INT_EQ(rect_a.left, col_left, "existing window at column left");
    ASSERT_INT_EQ(rect_b.left, col_left, "snapped window at column left");
    ASSERT(height_a + height_b + zone_gap == col_height,
           "both windows + gap fill column immediately after snap");
    ASSERT(height_a != col_height, "existing window was resized (not still full height)");

    DestroyWindow(window_a);
    DestroyWindow(window_b);
}

static bool is_window_above(HWND above, HWND below) {
    /* Walk z-order from top; if we find 'above' before 'below', it's higher */
    HWND hwnd = GetTopWindow(GetDesktopWindow());
    while (hwnd) {
        if (hwnd == above) return true;
        if (hwnd == below) return false;
        hwnd = GetNextWindow(hwnd, GW_HWNDNEXT);
    }
    return false;
}

static void test_snapped_window_goes_behind_existing(HINSTANCE instance) {
    printf("test_snapped_window_goes_behind_existing\n");
    Layout layout = build_test_layout();
    Column *left_column = &layout.monitors[0].columns[0];

    int col_left = left_column->bounds.left;
    int col_top = left_column->bounds.top;
    int col_width = left_column->bounds.right - left_column->bounds.left;
    int col_height = left_column->bounds.bottom - left_column->bounds.top;
    int left_center_x = (left_column->bounds.left + left_column->bounds.right) / 2;

    /* Track window_a (existing) in left column */
    HWND window_a = create_offscreen_window_at(
        instance, col_left, col_top, col_width, col_height
    );
    tracking_add_window(left_column, window_a);

    /* Snap window_b into the same column via shift+drag */
    HWND window_b = create_offscreen_window_at(
        instance, -20000, -20000, 400, 300
    );
    SwapTracker tracker = {0};
    swap_track_start(&tracker, window_b, &layout);
    tracker.shift_held = true;
    window_set_pos(window_b, left_center_x - 200, col_top + col_height / 2, 400, 300);
    swap_track_end(&tracker, window_b, &layout, 0.75f, 8, &real_ops);

    /* Existing window_a should be above (in front of) newly snapped window_b */
    ASSERT(is_window_above(window_a, window_b),
           "existing window is in front of newly snapped window");

    DestroyWindow(window_a);
    DestroyWindow(window_b);
}

/* ── Runner ─────────────────────────────────────────────────────── */

int main(void) {
    HINSTANCE instance = GetModuleHandle(NULL);

    printf("\n=== E2E: Tracking with Real Windows ===\n\n");
    test_real_window_can_be_tracked(instance);
    test_real_window_appears_in_tracked_query(instance);
    test_destroyed_window_pruned_from_tracked(instance);
    test_untracked_window_excluded_from_query(instance);
    test_focus_guard_with_real_windows(instance);

    printf("\n=== E2E: Resize Dimensions with Real Windows ===\n\n");
    test_single_real_window_fills_column(instance);
    test_two_real_windows_ratio_split(instance);
    test_three_real_windows_edges_meet(instance);

    printf("\n=== E2E: on_focus Flow ===\n\n");
    test_on_focus_finds_column_via_tracking_not_position(instance);
    test_full_focus_flow_two_tracked_windows(instance);
    test_swap_query_only_returns_tracked_windows(instance);

    printf("\n=== E2E: Swap Registration Flow ===\n\n");
    test_swap_track_start_works_when_window_outside_columns(instance);
    test_shift_drag_from_outside_to_column_registers(instance);
    test_shift_drag_within_same_column_registers(instance);
    test_full_user_flow_register_focus_resize(instance);

    printf("\n=== E2E: Cross-Column Swap ===\n\n");
    test_cross_column_swap_exchanges_positions(instance);
    test_plain_drag_to_different_column_untracks_window(instance);
    test_cross_column_drag_outside_dead_zone_no_swap(instance);
    test_shift_drag_to_static_column_registers(instance);
    test_remaining_window_auto_resizes_when_sibling_moves_out(instance);

    printf("\n=== E2E: Snap Behavior ===\n\n");
    test_snap_resizes_both_windows_immediately(instance);
    test_snapped_window_goes_behind_existing(instance);

    printf("\n=== Results: %d/%d passed, %d failed ===\n\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
