/*
 * CDDL HEADER START
 *
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * CDDL HEADER END
 */

#include <stdlib.h>

#include "ground_ops_window_state.h"

static int
minimum(int first, int second)
{
    return (first < second ? first : second);
}

static int
maximum(int first, int second)
{
    return (first > second ? first : second);
}

bool
ground_ops_presentation_valid(int presentation)
{
    return (presentation >= GROUND_OPS_PRESENTATION_HIDDEN &&
        presentation <= GROUND_OPS_PRESENTATION_PANEL);
}

bool
ground_ops_window_mode_valid(int mode)
{
    return (mode >= GROUND_OPS_WINDOW_FLOAT &&
        mode <= GROUND_OPS_WINDOW_POPOUT);
}

double
ground_ops_ui_scale(void)
{
    return (GROUND_OPS_UI_SCALE);
}

int
ground_ops_scaled_pixels(int logical_pixels)
{
    return ((int)(logical_pixels * ground_ops_ui_scale() + 0.5));
}

void
ground_ops_presentation_size(ground_ops_presentation_t presentation,
    int *width, int *height)
{
    if (presentation == GROUND_OPS_PRESENTATION_PANEL) {
        *width = ground_ops_scaled_pixels(GROUND_OPS_PANEL_WIDTH);
        *height = ground_ops_scaled_pixels(GROUND_OPS_PANEL_HEIGHT);
    } else {
        *width = ground_ops_scaled_pixels(GROUND_OPS_ORB_WIDTH);
        *height = ground_ops_scaled_pixels(GROUND_OPS_ORB_HEIGHT);
    }
}

void
ground_ops_rect_resize_top_right(ground_ops_rect_t *rect, int width,
    int height)
{
    rect->left = rect->right - width;
    rect->bottom = rect->top - height;
}

bool
ground_ops_rect_resize_visible(ground_ops_rect_t *rect, int width, int height,
    const ground_ops_monitor_t *monitors, size_t monitor_count,
    int preferred_monitor, int margin)
{
    const ground_ops_monitor_t *target = NULL;
    int center_x = rect->left + (rect->right - rect->left) / 2;
    int center_y = rect->bottom + (rect->top - rect->bottom) / 2;
    int left_gap, right_gap;

    for (size_t index = 0; index < monitor_count; index++) {
        const ground_ops_rect_t *bounds = &monitors[index].bounds;

        if (center_x >= bounds->left && center_x <= bounds->right &&
            center_y >= bounds->bottom && center_y <= bounds->top) {
            target = &monitors[index];
            break;
        }
    }
    if (target == NULL) {
        for (size_t index = 0; index < monitor_count; index++) {
            if (monitors[index].id == preferred_monitor) {
                target = &monitors[index];
                break;
            }
        }
    }
    if (target == NULL && monitor_count != 0)
        target = &monitors[0];
    if (target == NULL) {
        ground_ops_rect_resize_top_right(rect, width, height);
        return (false);
    }

    left_gap = abs(rect->left - target->bounds.left);
    right_gap = abs(target->bounds.right - rect->right);
    if (left_gap <= right_gap)
        rect->right = rect->left + width;
    else
        rect->left = rect->right - width;
    rect->bottom = rect->top - height;

    if (rect->left < target->bounds.left + margin) {
        rect->left = target->bounds.left + margin;
        rect->right = rect->left + width;
    }
    if (rect->right > target->bounds.right - margin) {
        rect->right = target->bounds.right - margin;
        rect->left = rect->right - width;
    }
    if (rect->bottom < target->bounds.bottom + margin) {
        rect->bottom = target->bounds.bottom + margin;
        rect->top = rect->bottom + height;
    }
    if (rect->top > target->bounds.top - margin) {
        rect->top = target->bounds.top - margin;
        rect->bottom = rect->top - height;
    }
    return (true);
}

bool
ground_ops_rect_has_visible_area(const ground_ops_rect_t *rect,
    const ground_ops_monitor_t *monitors, size_t monitor_count,
    int minimum_visible)
{
    for (size_t index = 0; index < monitor_count; index++) {
        const ground_ops_rect_t *bounds = &monitors[index].bounds;
        int width = minimum(rect->right, bounds->right) -
            maximum(rect->left, bounds->left);
        int height = minimum(rect->top, bounds->top) -
            maximum(rect->bottom, bounds->bottom);

        if (width >= minimum_visible && height >= minimum_visible)
            return (true);
    }
    return (false);
}

bool
ground_ops_rect_recover(ground_ops_rect_t *rect, int width, int height,
    const ground_ops_monitor_t *monitors, size_t monitor_count,
    int preferred_monitor, int margin, int minimum_visible)
{
    const ground_ops_monitor_t *target = NULL;

    ground_ops_rect_resize_top_right(rect, width, height);
    if (ground_ops_rect_has_visible_area(rect, monitors, monitor_count,
        minimum_visible))
        return (false);
    if (monitor_count == 0)
        return (false);

    for (size_t index = 0; index < monitor_count; index++) {
        if (monitors[index].id == preferred_monitor) {
            target = &monitors[index];
            break;
        }
    }
    if (target == NULL)
        target = &monitors[0];

    rect->right = target->bounds.right - margin;
    rect->top = target->bounds.top - margin;
    ground_ops_rect_resize_top_right(rect, width, height);
    return (true);
}

int
ground_ops_rect_monitor(const ground_ops_rect_t *rect,
    const ground_ops_monitor_t *monitors, size_t monitor_count)
{
    int center_x = rect->left + (rect->right - rect->left) / 2;
    int center_y = rect->bottom + (rect->top - rect->bottom) / 2;

    for (size_t index = 0; index < monitor_count; index++) {
        const ground_ops_rect_t *bounds = &monitors[index].bounds;

        if (center_x >= bounds->left && center_x <= bounds->right &&
            center_y >= bounds->bottom && center_y <= bounds->top)
            return (monitors[index].id);
    }
    return (-1);
}

bool
ground_ops_click_is_activation(int horizontal_displacement,
    int vertical_displacement, int threshold)
{
    return (abs(horizontal_displacement) <= threshold &&
        abs(vertical_displacement) <= threshold);
}
