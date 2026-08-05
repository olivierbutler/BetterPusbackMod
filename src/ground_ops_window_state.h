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

#ifndef _GROUND_OPS_WINDOW_STATE_H_
#define _GROUND_OPS_WINDOW_STATE_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GROUND_OPS_ORB_WIDTH 58
#define GROUND_OPS_ORB_HEIGHT 244
#define GROUND_OPS_PANEL_WIDTH 292
#define GROUND_OPS_PANEL_HEIGHT 420
#define GROUND_OPS_VISIBLE_MINIMUM 40
#define GROUND_OPS_WINDOW_MARGIN 24
#define GROUND_OPS_CLICK_DRAG_THRESHOLD 5

typedef enum {
    GROUND_OPS_PRESENTATION_HIDDEN = 0,
    GROUND_OPS_PRESENTATION_ORB = 1,
    GROUND_OPS_PRESENTATION_PANEL = 2
} ground_ops_presentation_t;

typedef enum {
    GROUND_OPS_WINDOW_FLOAT = 0,
    GROUND_OPS_WINDOW_POPOUT = 1
} ground_ops_window_mode_t;

typedef struct {
    int left;
    int top;
    int right;
    int bottom;
} ground_ops_rect_t;

typedef struct {
    int id;
    ground_ops_rect_t bounds;
} ground_ops_monitor_t;

bool ground_ops_presentation_valid(int presentation);
bool ground_ops_window_mode_valid(int mode);
void ground_ops_presentation_size(ground_ops_presentation_t presentation,
    int *width, int *height);
void ground_ops_rect_resize_top_right(ground_ops_rect_t *rect, int width,
    int height);
bool ground_ops_rect_resize_visible(ground_ops_rect_t *rect, int width,
    int height, const ground_ops_monitor_t *monitors, size_t monitor_count,
    int preferred_monitor, int margin);
bool ground_ops_rect_has_visible_area(const ground_ops_rect_t *rect,
    const ground_ops_monitor_t *monitors, size_t monitor_count,
    int minimum_visible);
bool ground_ops_rect_recover(ground_ops_rect_t *rect, int width, int height,
    const ground_ops_monitor_t *monitors, size_t monitor_count,
    int preferred_monitor, int margin, int minimum_visible);
int ground_ops_rect_monitor(const ground_ops_rect_t *rect,
    const ground_ops_monitor_t *monitors, size_t monitor_count);
bool ground_ops_click_is_activation(int horizontal_displacement,
    int vertical_displacement, int threshold);

#ifdef __cplusplus
}
#endif

#endif /* _GROUND_OPS_WINDOW_STATE_H_ */
