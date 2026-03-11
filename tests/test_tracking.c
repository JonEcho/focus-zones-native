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

/* ── Mock ResizeOps ─────────────────────────────────────────────── */

static int mock_set_pos_call_count = 0;
static HWND mock_set_pos_last_hwnd = NULL;

static void mock_set_pos(HWND hwnd, int x, int y, int w, int h) {
    (void)x; (void)y; (void)w; (void)h;
    mock_set_pos_call_count++;
    mock_set_pos_last_hwnd = hwnd;
}

static void mock_get_rect(HWND hwnd, RECT *rect) {
    (void)hwnd;
    *rect = (RECT){0, 0, 500, 1000};
}

static void mock_get_min_size(HWND hwnd, int *min_w, int *min_h) {
    (void)hwnd;
    *min_w = 50;
    *min_h = 50;
}

static ResizeOps mock_ops = {
    .set_pos = mock_set_pos,
    .get_rect = mock_get_rect,
    .get_min_size = mock_get_min_size,
};

static void reset_mocks(void) {
    mock_set_pos_call_count = 0;
    mock_set_pos_last_hwnd = NULL;
}

/* ── Factories ──────────────────────────────────────────────────── */

static Column build_empty_column(const char *name) {
    Column column = {0};
    strncpy(column.name, name, LAYOUT_NAME_MAX - 1);
    column.bounds = (RECT){0, 0, 500, 1000};
    column.is_dynamic = true;
    column.tracked_window_count = 0;
    return column;
}

static Layout build_single_monitor_layout(Column *columns, int column_count) {
    Layout layout = {0};
    layout.monitor_count = 1;
    layout.monitors[0].column_count = column_count;
    for (int index = 0; index < column_count; index++) {
        layout.monitors[0].columns[index] = columns[index];
    }
    return layout;
}

/* ── tracking_add_window tests ──────────────────────────────────── */

static void test_add_window_to_empty_column(void) {
    printf("test_add_window_to_empty_column\n");
    Column column = build_empty_column("left");
    HWND fake_handle = (HWND)0x1001;

    bool result = tracking_add_window(&column, fake_handle);

    ASSERT(result == true, "returns true on success");
    ASSERT(column.tracked_window_count == 1, "count increments to 1");
    ASSERT(column.tracked_windows[0].window_handle == fake_handle, "stores the window handle");
    ASSERT(column.tracked_windows[0].is_tracked == true, "marks as tracked");
}

static void test_add_window_is_idempotent(void) {
    printf("test_add_window_is_idempotent\n");
    Column column = build_empty_column("left");
    HWND fake_handle = (HWND)0x1001;

    tracking_add_window(&column, fake_handle);
    bool result = tracking_add_window(&column, fake_handle);

    ASSERT(result == true, "returns true on duplicate add");
    ASSERT(column.tracked_window_count == 1, "count stays at 1");
}

static void test_add_window_reactivates_untracked(void) {
    printf("test_add_window_reactivates_untracked\n");
    Column column = build_empty_column("left");
    HWND fake_handle = (HWND)0x1001;

    tracking_add_window(&column, fake_handle);
    tracking_remove_window(&column, fake_handle);
    ASSERT(column.tracked_windows[0].is_tracked == false, "is_tracked is false after remove");

    bool result = tracking_add_window(&column, fake_handle);
    ASSERT(result == true, "returns true on reactivation");
    ASSERT(column.tracked_windows[0].is_tracked == true, "is_tracked restored to true");
    ASSERT(column.tracked_window_count == 1, "count unchanged at 1");
}

static void test_add_multiple_windows(void) {
    printf("test_add_multiple_windows\n");
    Column column = build_empty_column("left");
    HWND handle_a = (HWND)0x1001;
    HWND handle_b = (HWND)0x1002;
    HWND handle_c = (HWND)0x1003;

    tracking_add_window(&column, handle_a);
    tracking_add_window(&column, handle_b);
    tracking_add_window(&column, handle_c);

    ASSERT(column.tracked_window_count == 3, "count is 3");
    ASSERT(column.tracked_windows[0].window_handle == handle_a, "first is handle_a");
    ASSERT(column.tracked_windows[1].window_handle == handle_b, "second is handle_b");
    ASSERT(column.tracked_windows[2].window_handle == handle_c, "third is handle_c");
}

static void test_add_window_fails_at_capacity(void) {
    printf("test_add_window_fails_at_capacity\n");
    Column column = build_empty_column("left");

    for (int index = 0; index < MAXIMUM_TRACKED_WINDOWS; index++) {
        tracking_add_window(&column, (HWND)(intptr_t)(0x1000 + index));
    }

    HWND overflow_handle = (HWND)0x9999;
    bool result = tracking_add_window(&column, overflow_handle);

    ASSERT(result == false, "returns false when at capacity");
    ASSERT(column.tracked_window_count == MAXIMUM_TRACKED_WINDOWS, "count stays at maximum");
}

/* ── tracking_remove_window tests ───────────────────────────────── */

static void test_remove_window_marks_untracked(void) {
    printf("test_remove_window_marks_untracked\n");
    Column column = build_empty_column("left");
    HWND fake_handle = (HWND)0x1001;

    tracking_add_window(&column, fake_handle);
    tracking_remove_window(&column, fake_handle);

    ASSERT(column.tracked_windows[0].is_tracked == false, "is_tracked set to false");
    ASSERT(column.tracked_window_count == 1, "count unchanged (soft delete)");
}

static void test_remove_nonexistent_window_is_safe(void) {
    printf("test_remove_nonexistent_window_is_safe\n");
    Column column = build_empty_column("left");

    tracking_remove_window(&column, (HWND)0x9999);

    ASSERT(column.tracked_window_count == 0, "count stays at 0");
}

/* ── tracking_remove_window_from_all tests ──────────────────────── */

static void test_remove_window_from_all_columns(void) {
    printf("test_remove_window_from_all_columns\n");
    Column columns[2];
    columns[0] = build_empty_column("left");
    columns[1] = build_empty_column("right");
    HWND shared_handle = (HWND)0x1001;

    tracking_add_window(&columns[0], shared_handle);
    tracking_add_window(&columns[1], shared_handle);
    Layout layout = build_single_monitor_layout(columns, 2);

    tracking_remove_window_from_all(&layout, shared_handle);

    ASSERT(layout.monitors[0].columns[0].tracked_windows[0].is_tracked == false,
           "untracked in left column");
    ASSERT(layout.monitors[0].columns[1].tracked_windows[0].is_tracked == false,
           "untracked in right column");
}

/* ── tracking_is_window_tracked tests ───────────────────────────── */

static void test_is_window_tracked_returns_true_for_tracked(void) {
    printf("test_is_window_tracked_returns_true_for_tracked\n");
    Column columns[1];
    columns[0] = build_empty_column("left");
    tracking_add_window(&columns[0], (HWND)0x1001);
    Layout layout = build_single_monitor_layout(columns, 1);

    ASSERT(tracking_is_window_tracked(&layout, (HWND)0x1001) == true,
           "returns true for tracked window");
}

static void test_is_window_tracked_returns_false_for_untracked(void) {
    printf("test_is_window_tracked_returns_false_for_untracked\n");
    Column columns[1];
    columns[0] = build_empty_column("left");
    Layout layout = build_single_monitor_layout(columns, 1);

    ASSERT(tracking_is_window_tracked(&layout, (HWND)0x9999) == false,
           "returns false for untracked window");
}

static void test_is_window_tracked_returns_false_after_remove(void) {
    printf("test_is_window_tracked_returns_false_after_remove\n");
    Column columns[1];
    columns[0] = build_empty_column("left");
    tracking_add_window(&columns[0], (HWND)0x1001);
    tracking_remove_window(&columns[0], (HWND)0x1001);
    Layout layout = build_single_monitor_layout(columns, 1);

    ASSERT(tracking_is_window_tracked(&layout, (HWND)0x1001) == false,
           "returns false after removal");
}

/* ── tracking_find_column_for_window tests ──────────────────────── */

static void test_find_column_for_tracked_window(void) {
    printf("test_find_column_for_tracked_window\n");
    Column columns[2];
    columns[0] = build_empty_column("left");
    columns[1] = build_empty_column("right");
    tracking_add_window(&columns[1], (HWND)0x1001);
    Layout layout = build_single_monitor_layout(columns, 2);

    Column *found = tracking_find_column_for_window(&layout, (HWND)0x1001);
    ASSERT(found != NULL, "returns non-null");
    ASSERT(strcmp(found->name, "right") == 0, "finds correct column by name");
}

static void test_find_column_returns_null_for_untracked(void) {
    printf("test_find_column_returns_null_for_untracked\n");
    Column columns[1];
    columns[0] = build_empty_column("left");
    Layout layout = build_single_monitor_layout(columns, 1);

    ASSERT(tracking_find_column_for_window(&layout, (HWND)0x9999) == NULL,
           "returns null for untracked window");
}

static void test_find_column_returns_null_after_remove(void) {
    printf("test_find_column_returns_null_after_remove\n");
    Column columns[1];
    columns[0] = build_empty_column("left");
    tracking_add_window(&columns[0], (HWND)0x1001);
    tracking_remove_window(&columns[0], (HWND)0x1001);
    Layout layout = build_single_monitor_layout(columns, 1);

    ASSERT(tracking_find_column_for_window(&layout, (HWND)0x1001) == NULL,
           "returns null after removal");
}

/* ── Edge cases ─────────────────────────────────────────────────── */

static void test_empty_layout_tracking_queries(void) {
    printf("test_empty_layout_tracking_queries\n");
    Layout layout = {0};
    layout.monitor_count = 0;

    ASSERT(tracking_is_window_tracked(&layout, (HWND)0x1001) == false,
           "is_tracked returns false on empty layout");
    ASSERT(tracking_find_column_for_window(&layout, (HWND)0x1001) == NULL,
           "find_column returns null on empty layout");
}

static void test_multi_monitor_tracking(void) {
    printf("test_multi_monitor_tracking\n");
    Layout layout = {0};
    layout.monitor_count = 2;

    layout.monitors[0].column_count = 1;
    strncpy(layout.monitors[0].columns[0].name, "monitor1_left", LAYOUT_NAME_MAX - 1);
    layout.monitors[0].columns[0].tracked_window_count = 0;

    layout.monitors[1].column_count = 1;
    strncpy(layout.monitors[1].columns[0].name, "monitor2_left", LAYOUT_NAME_MAX - 1);
    layout.monitors[1].columns[0].tracked_window_count = 0;

    tracking_add_window(&layout.monitors[1].columns[0], (HWND)0x2001);

    ASSERT(tracking_is_window_tracked(&layout, (HWND)0x2001) == true,
           "tracked across second monitor");

    Column *found = tracking_find_column_for_window(&layout, (HWND)0x2001);
    ASSERT(found != NULL, "found on second monitor");
    ASSERT(strcmp(found->name, "monitor2_left") == 0, "correct monitor column name");

    tracking_remove_window_from_all(&layout, (HWND)0x2001);
    ASSERT(tracking_is_window_tracked(&layout, (HWND)0x2001) == false,
           "removed from all monitors");
}

/* ── Behavioral: untracked windows must not affect tracked ones ── */

static void test_resize_single_occupant_ignores_empty_tracked_columns(void) {
    printf("test_resize_single_occupant_ignores_empty_tracked_columns\n");
    reset_mocks();

    Column columns[2];
    columns[0] = build_empty_column("left");
    columns[1] = build_empty_column("right");
    columns[0].is_dynamic = true;
    columns[1].is_dynamic = true;

    Layout layout = build_single_monitor_layout(columns, 2);

    resize_single_occupant_columns(&layout, 0.75f, 8, NULL, &mock_ops);

    ASSERT(mock_set_pos_call_count == 0,
           "no set_pos calls when no windows are tracked");
}

static void test_resize_column_positions_only_provided_siblings(void) {
    printf("test_resize_column_positions_only_provided_siblings\n");
    reset_mocks();

    Column column = build_empty_column("left");
    HWND tracked_handle = (HWND)0x2001;

    WindowInfo siblings[1];
    siblings[0].hwnd = tracked_handle;
    siblings[0].rect = (RECT){0, 0, 500, 1000};

    resize_column(tracked_handle, siblings, 1, 0.75f, &column, 8, &mock_ops);

    ASSERT(mock_set_pos_call_count == 1, "set_pos called exactly once");
    ASSERT(mock_set_pos_last_hwnd == tracked_handle,
           "set_pos called for the tracked window only");
}

static void test_untracked_window_does_not_appear_in_tracked_query(void) {
    printf("test_untracked_window_does_not_appear_in_tracked_query\n");
    Column column = build_empty_column("left");
    HWND tracked = (HWND)0x1001;
    HWND untracked = (HWND)0x1002;

    tracking_add_window(&column, tracked);
    tracking_add_window(&column, untracked);
    tracking_remove_window(&column, untracked);

    ASSERT(column.tracked_window_count == 2, "array has 2 entries");
    ASSERT(column.tracked_windows[0].is_tracked == true, "first is tracked");
    ASSERT(column.tracked_windows[1].is_tracked == false, "second is untracked");
}

static void test_focus_guard_blocks_untracked_window(void) {
    printf("test_focus_guard_blocks_untracked_window\n");
    Column columns[1];
    columns[0] = build_empty_column("left");
    tracking_add_window(&columns[0], (HWND)0x1001);
    Layout layout = build_single_monitor_layout(columns, 1);

    HWND untracked_window = (HWND)0x9999;
    bool should_process = tracking_is_window_tracked(&layout, untracked_window);

    ASSERT(should_process == false,
           "untracked window rejected by tracking guard");
}

static void test_focus_guard_allows_tracked_window(void) {
    printf("test_focus_guard_allows_tracked_window\n");
    Column columns[1];
    columns[0] = build_empty_column("left");
    tracking_add_window(&columns[0], (HWND)0x1001);
    Layout layout = build_single_monitor_layout(columns, 1);

    bool should_process = tracking_is_window_tracked(&layout, (HWND)0x1001);

    ASSERT(should_process == true,
           "tracked window passes tracking guard");
}

static void test_removed_window_no_longer_triggers_resize(void) {
    printf("test_removed_window_no_longer_triggers_resize\n");
    reset_mocks();

    Column columns[1];
    columns[0] = build_empty_column("left");
    columns[0].is_dynamic = true;

    HWND window = (HWND)0x1001;
    tracking_add_window(&columns[0], window);
    tracking_remove_window(&columns[0], window);

    Layout layout = build_single_monitor_layout(columns, 1);

    resize_single_occupant_columns(&layout, 0.75f, 8, NULL, &mock_ops);

    ASSERT(mock_set_pos_call_count == 0,
           "removed window does not trigger single-occupant resize");
}

static void test_only_tracked_window_gets_resized_among_mixed(void) {
    printf("test_only_tracked_window_gets_resized_among_mixed\n");
    reset_mocks();

    Column column = build_empty_column("left");
    HWND tracked = (HWND)0x2001;
    HWND untracked = (HWND)0x2002;

    tracking_add_window(&column, tracked);
    tracking_add_window(&column, untracked);
    tracking_remove_window(&column, untracked);

    WindowInfo siblings[1];
    siblings[0].hwnd = tracked;
    siblings[0].rect = (RECT){0, 0, 500, 500};

    resize_column(tracked, siblings, 1, 0.75f, &column, 8, &mock_ops);

    ASSERT(mock_set_pos_call_count == 1, "only one set_pos call");
    ASSERT(mock_set_pos_last_hwnd == tracked, "set_pos called for tracked window");
}

/* ── Runner ─────────────────────────────────────────────────────── */

int main(void) {
    printf("\n=== Tracking Data Tests ===\n\n");

    test_add_window_to_empty_column();
    test_add_window_is_idempotent();
    test_add_window_reactivates_untracked();
    test_add_multiple_windows();
    test_add_window_fails_at_capacity();
    test_remove_window_marks_untracked();
    test_remove_nonexistent_window_is_safe();
    test_remove_window_from_all_columns();
    test_is_window_tracked_returns_true_for_tracked();
    test_is_window_tracked_returns_false_for_untracked();
    test_is_window_tracked_returns_false_after_remove();
    test_find_column_for_tracked_window();
    test_find_column_returns_null_for_untracked();
    test_find_column_returns_null_after_remove();
    test_empty_layout_tracking_queries();
    test_multi_monitor_tracking();

    printf("\n=== Behavioral: Untracked Windows Ignored ===\n\n");

    test_resize_single_occupant_ignores_empty_tracked_columns();
    test_resize_column_positions_only_provided_siblings();
    test_untracked_window_does_not_appear_in_tracked_query();
    test_focus_guard_blocks_untracked_window();
    test_focus_guard_allows_tracked_window();
    test_removed_window_no_longer_triggers_resize();
    test_only_tracked_window_gets_resized_among_mixed();

    printf("\n=== Results: %d/%d passed, %d failed ===\n\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
