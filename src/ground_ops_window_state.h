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
#define GROUND_OPS_COLLAPSE_DWELL_SECONDS 1.0
#define GROUND_OPS_AUTO_COLLAPSE_DELAY_SECONDS 1.0

typedef struct {
    bool tracking;
    bool fired;
    double entered;
} ground_ops_dwell_t;

typedef struct {
    bool action_active;
    bool collapse_pending;
    double action_completed_at;
} ground_ops_auto_expand_state_t;

typedef enum {
    GROUND_OPS_AUTO_PRESENTATION_NONE = 0,
    GROUND_OPS_AUTO_PRESENTATION_EXPAND,
    GROUND_OPS_AUTO_PRESENTATION_COLLAPSE
} ground_ops_auto_presentation_t;

#ifndef APL
#define APL 0
#endif

#ifndef BP_EMULATE_MAC_UI_SCALE
#define BP_EMULATE_MAC_UI_SCALE 0
#endif

#if APL || BP_EMULATE_MAC_UI_SCALE
#define GROUND_OPS_UI_SCALE 1.35
#else
#define GROUND_OPS_UI_SCALE 1.0
#endif

typedef enum {
    GROUND_OPS_UI_SIZE_STANDARD = 0,
    GROUND_OPS_UI_SIZE_LARGE = 1,
    GROUND_OPS_UI_SIZE_EXTRA_LARGE = 2
} ground_ops_ui_size_t;

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
ground_ops_presentation_t ground_ops_startup_presentation(
    int saved_presentation, int saved_last_visible);
bool ground_ops_window_mode_valid(int mode);
bool ground_ops_window_effectively_visible(bool window_exists,
    bool planner_suspended, bool legacy_gate_hidden,
    bool manual_visibility_override, bool aircraft_eligible);
bool ground_ops_ui_size_valid(int size);
double ground_ops_ui_size_multiplier(ground_ops_ui_size_t size);
ground_ops_ui_size_t ground_ops_ui_size_get(void);
void ground_ops_ui_size_set(ground_ops_ui_size_t size);
double ground_ops_ui_scale(void);
int ground_ops_scaled_pixels(int logical_pixels);
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
bool ground_ops_dwell_update(ground_ops_dwell_t *state, bool eligible,
    double now);
void ground_ops_auto_expand_reset(ground_ops_auto_expand_state_t *state);
void ground_ops_auto_expand_note_manual(
    ground_ops_auto_expand_state_t *state, bool action_required);
ground_ops_auto_presentation_t ground_ops_auto_expand_update(
    ground_ops_auto_expand_state_t *state, bool enabled,
    bool window_visible, ground_ops_presentation_t presentation,
    bool action_required, bool allow_expansion, double now);
bool ground_ops_rect_nearest_right(const ground_ops_rect_t *rect,
    const ground_ops_monitor_t *monitor);
void ground_ops_rect_clamp(ground_ops_rect_t *rect,
    const ground_ops_monitor_t *monitor, int margin);
void ground_ops_rect_dock(ground_ops_rect_t *rect, int width, int height,
    const ground_ops_monitor_t *monitor, const ground_ops_rect_t *rest_offset);
bool ground_ops_rect_restore_compact(ground_ops_rect_t *rect, int width, int height,
    const ground_ops_monitor_t *monitor, const ground_ops_rect_t *expanded,
    const ground_ops_rect_t *compact);

#ifdef __cplusplus
}
#endif

#endif /* _GROUND_OPS_WINDOW_STATE_H_ */
