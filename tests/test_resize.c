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

/* ── Mock ResizeOps with full call recording ────────────────────── */

#define MOCK_MAX_CALLS 16

typedef struct {
    HWND hwnd;
    int x;
    int y;
    int width;
    int height;
} SetPosCall;

static SetPosCall mock_calls[MOCK_MAX_CALLS];
static int mock_call_count = 0;

static void mock_set_pos(HWND hwnd, int x, int y, int w, int h) {
    if (mock_call_count < MOCK_MAX_CALLS) {
        mock_calls[mock_call_count].hwnd = hwnd;
        mock_calls[mock_call_count].x = x;
        mock_calls[mock_call_count].y = y;
        mock_calls[mock_call_count].width = w;
        mock_calls[mock_call_count].height = h;
        mock_call_count++;
    }
}

static void mock_get_rect(HWND hwnd, RECT *rect) {
    (void)hwnd;
    *rect = (RECT){0, 0, 500, 500};
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
    mock_call_count = 0;
    memset(mock_calls, 0, sizeof(mock_calls));
}

static Column build_column(const char *name, int left, int top, int right, int bottom) {
    Column column = {0};
    strncpy(column.name, name, LAYOUT_NAME_MAX - 1);
    column.bounds = (RECT){left, top, right, bottom};
    column.is_dynamic = true;
    column.tracked_window_count = 0;
    return column;
}

/* ── Single window fills entire column ──────────────────────────── */

static void test_single_window_fills_entire_column(void) {
    printf("test_single_window_fills_entire_column\n");
    reset_mocks();

    Column column = build_column("left", 0, 0, 960, 1080);
    HWND window = (HWND)0x1001;

    WindowInfo siblings[1];
    siblings[0].hwnd = window;
    siblings[0].rect = (RECT){0, 0, 500, 500};

    resize_column(window, siblings, 1, 0.75f, &column, 8, &mock_ops);

    ASSERT_INT_EQ(mock_call_count, 1, "exactly one set_pos call");
    ASSERT_INT_EQ(mock_calls[0].x, 0, "x starts at column left edge");
    ASSERT_INT_EQ(mock_calls[0].y, 0, "y starts at column top edge");
    ASSERT_INT_EQ(mock_calls[0].width, 960, "width fills entire column width");
    ASSERT_INT_EQ(mock_calls[0].height, 1080, "height fills entire column height");
}

/* ── Two windows: focused gets 75%, unfocused gets 25% ──────────── */

static void test_two_windows_focused_gets_ratio(void) {
    printf("test_two_windows_focused_gets_ratio\n");
    reset_mocks();

    Column column = build_column("left", 0, 0, 960, 1080);
    HWND focused = (HWND)0x1001;
    HWND unfocused = (HWND)0x1002;
    int zone_gap = 8;
    float focus_ratio = 0.75f;

    WindowInfo siblings[2];
    siblings[0].hwnd = focused;
    siblings[0].rect = (RECT){0, 0, 960, 500};
    siblings[1].hwnd = unfocused;
    siblings[1].rect = (RECT){0, 500, 960, 1080};

    resize_column(focused, siblings, 2, focus_ratio, &column, zone_gap, &mock_ops);

    int usable = 1080 - zone_gap;  /* 1072 */
    int unfocused_height = (int)(usable * (1.0f - focus_ratio));  /* 268 */
    int focused_height = usable - unfocused_height;  /* 804 */

    ASSERT_INT_EQ(mock_call_count, 2, "two set_pos calls");

    ASSERT_INT_EQ(mock_calls[0].hwnd == focused, 1, "first call is focused window");
    ASSERT_INT_EQ(mock_calls[0].x, 0, "focused: x at column left");
    ASSERT_INT_EQ(mock_calls[0].y, 0, "focused: y at column top");
    ASSERT_INT_EQ(mock_calls[0].width, 960, "focused: full column width");
    ASSERT_INT_EQ(mock_calls[0].height, focused_height, "focused: gets focus_ratio of usable height");

    ASSERT_INT_EQ(mock_calls[1].hwnd == unfocused, 1, "second call is unfocused window");
    ASSERT_INT_EQ(mock_calls[1].x, 0, "unfocused: x at column left");
    ASSERT_INT_EQ(mock_calls[1].y, focused_height + zone_gap, "unfocused: y starts after focused + gap");
    ASSERT_INT_EQ(mock_calls[1].width, 960, "unfocused: full column width");
    ASSERT_INT_EQ(mock_calls[1].height, unfocused_height, "unfocused: gets remainder of usable height");
}

/* ── Two windows cover full column height (no dead pixels) ──────── */

static void test_two_windows_cover_full_column_height(void) {
    printf("test_two_windows_cover_full_column_height\n");
    reset_mocks();

    Column column = build_column("left", 0, 0, 960, 1080);
    HWND focused = (HWND)0x1001;
    HWND unfocused = (HWND)0x1002;
    int zone_gap = 8;

    WindowInfo siblings[2];
    siblings[0].hwnd = focused;
    siblings[0].rect = (RECT){0, 0, 960, 500};
    siblings[1].hwnd = unfocused;
    siblings[1].rect = (RECT){0, 500, 960, 1080};

    resize_column(focused, siblings, 2, 0.75f, &column, zone_gap, &mock_ops);

    int total_covered = mock_calls[0].height + zone_gap + mock_calls[1].height;
    ASSERT_INT_EQ(total_covered, 1080, "windows + gap cover full column height");

    int bottom_edge = mock_calls[1].y + mock_calls[1].height;
    ASSERT_INT_EQ(bottom_edge, 1080, "bottom edge reaches column bottom");
}

/* ── Three windows: focused + two unfocused split evenly ────────── */

static void test_three_windows_unfocused_split_evenly(void) {
    printf("test_three_windows_unfocused_split_evenly\n");
    reset_mocks();

    Column column = build_column("center", 960, 0, 1920, 1080);
    HWND focused = (HWND)0x1001;
    HWND upper = (HWND)0x1002;
    HWND lower = (HWND)0x1003;
    int zone_gap = 8;
    float focus_ratio = 0.75f;

    WindowInfo siblings[3];
    siblings[0].hwnd = upper;
    siblings[0].rect = (RECT){960, 0, 1920, 300};
    siblings[1].hwnd = focused;
    siblings[1].rect = (RECT){960, 300, 1920, 800};
    siblings[2].hwnd = lower;
    siblings[2].rect = (RECT){960, 800, 1920, 1080};

    resize_column(focused, siblings, 3, focus_ratio, &column, zone_gap, &mock_ops);

    int gap_count = 2;
    int usable = 1080 - (zone_gap * gap_count);  /* 1064 */
    int unfocused_each = (int)(usable * (1.0f - focus_ratio)) / 2;  /* 133 */
    int focused_height = usable - (unfocused_each * 2);  /* 798 */

    ASSERT_INT_EQ(mock_call_count, 3, "three set_pos calls");

    ASSERT_INT_EQ(mock_calls[0].height, unfocused_each, "upper unfocused gets even split");
    ASSERT_INT_EQ(mock_calls[1].height, focused_height, "focused gets remainder");
    ASSERT_INT_EQ(mock_calls[2].height, unfocused_each, "lower unfocused gets even split");

    ASSERT_INT_EQ(mock_calls[0].x, 960, "all windows at column left (960)");
    ASSERT_INT_EQ(mock_calls[1].x, 960, "focused at column left (960)");
    ASSERT_INT_EQ(mock_calls[2].x, 960, "lower at column left (960)");

    ASSERT_INT_EQ(mock_calls[0].width, 960, "all windows span column width (960)");
    ASSERT_INT_EQ(mock_calls[1].width, 960, "focused spans column width");
    ASSERT_INT_EQ(mock_calls[2].width, 960, "lower spans column width");
}

/* ── Three windows cover full column height ─────────────────────── */

static void test_three_windows_cover_full_height(void) {
    printf("test_three_windows_cover_full_height\n");
    reset_mocks();

    Column column = build_column("center", 0, 0, 960, 1080);
    HWND focused = (HWND)0x1001;
    int zone_gap = 8;

    WindowInfo siblings[3];
    siblings[0].hwnd = (HWND)0x1002;
    siblings[0].rect = (RECT){0, 0, 960, 300};
    siblings[1].hwnd = focused;
    siblings[1].rect = (RECT){0, 300, 960, 800};
    siblings[2].hwnd = (HWND)0x1003;
    siblings[2].rect = (RECT){0, 800, 960, 1080};

    resize_column(focused, siblings, 3, 0.75f, &column, zone_gap, &mock_ops);

    int total_covered = mock_calls[0].height + zone_gap
                      + mock_calls[1].height + zone_gap
                      + mock_calls[2].height;
    ASSERT_INT_EQ(total_covered, 1080, "three windows + gaps cover full column height");
}

/* ── Windows stack top-to-bottom with gaps between ──────────────── */

static void test_windows_stack_with_correct_gaps(void) {
    printf("test_windows_stack_with_correct_gaps\n");
    reset_mocks();

    Column column = build_column("left", 0, 0, 960, 1080);
    HWND focused = (HWND)0x1001;
    int zone_gap = 12;

    WindowInfo siblings[2];
    siblings[0].hwnd = focused;
    siblings[0].rect = (RECT){0, 0, 960, 500};
    siblings[1].hwnd = (HWND)0x1002;
    siblings[1].rect = (RECT){0, 500, 960, 1080};

    resize_column(focused, siblings, 2, 0.75f, &column, zone_gap, &mock_ops);

    int gap_between = mock_calls[1].y - (mock_calls[0].y + mock_calls[0].height);
    ASSERT_INT_EQ(gap_between, zone_gap, "gap between windows equals zone_gap");
}

/* ── Column with offset (not at screen origin) ──────────────────── */

static void test_column_with_offset_origin(void) {
    printf("test_column_with_offset_origin\n");
    reset_mocks();

    Column column = build_column("right", 1920, 50, 2880, 1130);
    HWND window = (HWND)0x1001;

    WindowInfo siblings[1];
    siblings[0].hwnd = window;
    siblings[0].rect = (RECT){1920, 50, 2880, 1130};

    resize_column(window, siblings, 1, 0.75f, &column, 8, &mock_ops);

    ASSERT_INT_EQ(mock_calls[0].x, 1920, "x respects column left offset");
    ASSERT_INT_EQ(mock_calls[0].y, 50, "y respects column top offset");
    ASSERT_INT_EQ(mock_calls[0].width, 960, "width = right - left");
    ASSERT_INT_EQ(mock_calls[0].height, 1080, "height = bottom - top");
}

/* ── Zero gap: windows are edge-to-edge with no spacing ─────────── */

static void test_zero_gap_windows_are_edge_to_edge(void) {
    printf("test_zero_gap_windows_are_edge_to_edge\n");
    reset_mocks();

    Column column = build_column("left", 0, 0, 960, 1080);
    HWND focused = (HWND)0x1001;

    WindowInfo siblings[2];
    siblings[0].hwnd = focused;
    siblings[0].rect = (RECT){0, 0, 960, 500};
    siblings[1].hwnd = (HWND)0x1002;
    siblings[1].rect = (RECT){0, 500, 960, 1080};

    resize_column(focused, siblings, 2, 0.75f, &column, 0, &mock_ops);

    int gap = mock_calls[1].y - (mock_calls[0].y + mock_calls[0].height);
    ASSERT_INT_EQ(gap, 0, "no gap between windows when zone_gap is 0");

    int total = mock_calls[0].height + mock_calls[1].height;
    ASSERT_INT_EQ(total, 1080, "windows cover full height with no gaps");
}

/* ── 50/50 ratio gives equal heights to both windows ────────────── */

static void test_fifty_fifty_ratio(void) {
    printf("test_fifty_fifty_ratio\n");
    reset_mocks();

    Column column = build_column("left", 0, 0, 1000, 1000);
    HWND focused = (HWND)0x1001;
    HWND other = (HWND)0x1002;
    int zone_gap = 0;

    WindowInfo siblings[2];
    siblings[0].hwnd = focused;
    siblings[0].rect = (RECT){0, 0, 1000, 500};
    siblings[1].hwnd = other;
    siblings[1].rect = (RECT){0, 500, 1000, 1000};

    resize_column(focused, siblings, 2, 0.50f, &column, zone_gap, &mock_ops);

    ASSERT_INT_EQ(mock_calls[0].height, mock_calls[1].height,
                  "50/50 ratio gives equal heights");
    ASSERT_INT_EQ(mock_calls[0].height + mock_calls[1].height, 1000,
                  "both windows together fill the column");
}

/* ── All windows span the full column width ─────────────────────── */

static void test_all_windows_span_full_column_width(void) {
    printf("test_all_windows_span_full_column_width\n");
    reset_mocks();

    Column column = build_column("wide", 100, 0, 1860, 1080);
    HWND focused = (HWND)0x1001;
    int expected_width = 1860 - 100;

    WindowInfo siblings[3];
    siblings[0].hwnd = focused;
    siblings[0].rect = (RECT){0, 0, 500, 300};
    siblings[1].hwnd = (HWND)0x1002;
    siblings[1].rect = (RECT){0, 300, 500, 600};
    siblings[2].hwnd = (HWND)0x1003;
    siblings[2].rect = (RECT){0, 600, 500, 900};

    resize_column(focused, siblings, 3, 0.75f, &column, 8, &mock_ops);

    ASSERT_INT_EQ(mock_calls[0].width, expected_width, "window 0 full column width");
    ASSERT_INT_EQ(mock_calls[1].width, expected_width, "window 1 full column width");
    ASSERT_INT_EQ(mock_calls[2].width, expected_width, "window 2 full column width");
}

/* ── Adjacent columns: right column starts where left column ends ── */

static void test_adjacent_columns_share_edge(void) {
    printf("test_adjacent_columns_share_edge\n");
    reset_mocks();

    Column left_column = build_column("left", 0, 0, 960, 1080);
    Column right_column = build_column("right", 960, 0, 1920, 1080);

    HWND left_window = (HWND)0x1001;
    HWND right_window = (HWND)0x1002;

    WindowInfo left_siblings[1] = {{left_window, {0, 0, 960, 1080}}};
    WindowInfo right_siblings[1] = {{right_window, {960, 0, 1920, 1080}}};

    resize_column(left_window, left_siblings, 1, 0.75f, &left_column, 8, &mock_ops);
    int left_set_pos_index = mock_call_count - 1;

    resize_column(right_window, right_siblings, 1, 0.75f, &right_column, 8, &mock_ops);
    int right_set_pos_index = mock_call_count - 1;

    int left_right_edge = mock_calls[left_set_pos_index].x + mock_calls[left_set_pos_index].width;
    int right_left_edge = mock_calls[right_set_pos_index].x;

    ASSERT_INT_EQ(left_right_edge, right_left_edge,
                  "left column right edge meets right column left edge");
    ASSERT_INT_EQ(left_right_edge, 960, "shared edge at 960px");
}

/* ── No calls when sibling_count is 0 ───────────────────────────── */

static void test_zero_siblings_no_calls(void) {
    printf("test_zero_siblings_no_calls\n");
    reset_mocks();

    Column column = build_column("left", 0, 0, 960, 1080);
    resize_column(NULL, NULL, 0, 0.75f, &column, 8, &mock_ops);

    ASSERT_INT_EQ(mock_call_count, 0, "no set_pos calls with 0 siblings");
}

/* ── Runner ─────────────────────────────────────────────────────── */

int main(void) {
    printf("\n=== Resize Dimension Tests ===\n\n");

    test_single_window_fills_entire_column();
    test_two_windows_focused_gets_ratio();
    test_two_windows_cover_full_column_height();
    test_three_windows_unfocused_split_evenly();
    test_three_windows_cover_full_height();
    test_windows_stack_with_correct_gaps();
    test_column_with_offset_origin();
    test_zero_gap_windows_are_edge_to_edge();
    test_fifty_fifty_ratio();
    test_all_windows_span_full_column_width();
    test_adjacent_columns_share_edge();
    test_zero_siblings_no_calls();

    printf("\n=== Results: %d/%d passed, %d failed ===\n\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
