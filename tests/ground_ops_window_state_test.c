#include <assert.h>
#include <stdio.h>

#include "ground_ops_window_state.h"

static const ground_ops_monitor_t monitors[] = {
    {0, {0, 1080, 1920, 0}},
    {1, {1920, 1440, 4480, 0}}
};

static void
test_contract_sizes(void)
{
    int width = 0, height = 0;

    ground_ops_presentation_size(GROUND_OPS_PRESENTATION_ORB,
        &width, &height);
    assert(width == 58 && height == 244);
    ground_ops_presentation_size(GROUND_OPS_PRESENTATION_PANEL,
        &width, &height);
    assert(width == 292 && height == 420);
    assert(ground_ops_presentation_valid(GROUND_OPS_PRESENTATION_HIDDEN));
    assert(ground_ops_presentation_valid(GROUND_OPS_PRESENTATION_PANEL));
    assert(!ground_ops_presentation_valid(3));
    assert(ground_ops_window_mode_valid(GROUND_OPS_WINDOW_FLOAT));
    assert(ground_ops_window_mode_valid(GROUND_OPS_WINDOW_POPOUT));
    assert(!ground_ops_window_mode_valid(2));
}

static void
test_anchor_and_visibility(void)
{
    ground_ops_rect_t rect = {1568, 1000, 1860, 620};

    assert(ground_ops_rect_has_visible_area(&rect, monitors, 2, 40));
    ground_ops_rect_resize_top_right(&rect, 58, 58);
    assert(rect.right == 1860 && rect.top == 1000);
    assert(rect.left == 1802 && rect.bottom == 942);
    assert(ground_ops_rect_monitor(&rect, monitors, 2) == 0);
}

static void
test_edge_aware_expansion(void)
{
    ground_ops_rect_t left_edge = {0, 500, 58, 256};
    ground_ops_rect_t right_edge = {1862, 800, 1920, 556};
    ground_ops_rect_t bottom_edge = {400, 244, 458, 0};

    assert(ground_ops_rect_resize_visible(&left_edge, 292, 420,
        monitors, 2, 0, 0));
    assert(left_edge.left == 0);
    assert(left_edge.right == 292);
    assert(left_edge.top == 500);
    assert(left_edge.bottom == 80);

    assert(ground_ops_rect_resize_visible(&right_edge, 292, 420,
        monitors, 2, 0, 0));
    assert(right_edge.left == 1628);
    assert(right_edge.right == 1920);
    assert(right_edge.top == 800);
    assert(right_edge.bottom == 380);

    assert(ground_ops_rect_resize_visible(&bottom_edge, 292, 420,
        monitors, 2, 0, 0));
    assert(bottom_edge.left == 400);
    assert(bottom_edge.right == 692);
    assert(bottom_edge.bottom == 0);
    assert(bottom_edge.top == 420);
}

static void
test_missing_monitor_recovery(void)
{
    ground_ops_rect_t rect = {9000, 9000, 9292, 8620};

    assert(ground_ops_rect_recover(&rect, 292, 420, monitors, 2,
        1, 24, 40));
    assert(rect.right == 4456);
    assert(rect.top == 1416);
    assert(rect.left == 4164);
    assert(rect.bottom == 996);
    assert(ground_ops_rect_monitor(&rect, monitors, 2) == 1);

    rect = (ground_ops_rect_t) {9000, 9000, 9058, 8942};
    assert(ground_ops_rect_recover(&rect, 58, 58, monitors, 2,
        99, 24, 40));
    assert(ground_ops_rect_monitor(&rect, monitors, 2) == 0);
}

static void
test_click_drag_threshold(void)
{
    assert(ground_ops_click_is_activation(0, 0, 5));
    assert(ground_ops_click_is_activation(5, -5, 5));
    assert(!ground_ops_click_is_activation(6, 0, 5));
    assert(!ground_ops_click_is_activation(0, -6, 5));
}

int
main(void)
{
    test_contract_sizes();
    test_anchor_and_visibility();
    test_edge_aware_expansion();
    test_missing_monitor_recovery();
    test_click_drag_threshold();
    puts("ground operations window state tests passed");
    return (0);
}
