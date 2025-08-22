/*
 * CDDL HEADER START
 *
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 *
 * CDDL HEADER END
*/
/*
 * Copyright 2017 Saso Kiselkov. All rights reserved.
 * Copyright 2024 Robert Wellinger. All rights reserved.
 * */

#ifndef    _CFG_H_
#define    _CFG_H_

#include <acfutils/conf.h>

#ifdef    __cplusplus
extern "C" {
#endif

typedef struct {
    bool_t planner_running;
    float fov_h_deg;
    float fov_h_ratio;
    float fov_roll;
    float fov_v_deg;
    float fov_v_ratio;
} fov_t;

extern conf_t *bp_conf;

typedef struct {
    bool_t monitor_found; // to clear before the search
    int monitor_requested;
    int monitor_count;
    int monitor_id;
    int x_origin;
    int y_origin;
    int h;
    int w;
    int magic_squares_height;
} monitors_t;

enum  {
    DOOR_CHECK_ActiveWithMessage = 0,
    DOOR_CHECK_ActiveSilent,
    DOOR_CHECK_Ignore
};

extern  monitors_t monitor_def;
extern bool_t setup_view_callback_is_alive;

bool_t bp_conf_init();

bool_t bp_conf_save();

void bp_conf_fini();

void bp_conf_set_save_enabled(bool_t flag);

void bp_conf_open(void);

bool_t conf_get_b_per_acf(char *my_key,  bool_t *value);

void conf_set_b_per_acf(char *my_key,  bool_t value);

bool_t conf_get_i_per_acf(char *my_key,  int *value);

void conf_set_i_per_acf(char *my_key,  int value);

void push_reset_fov_values(void);

void pop_fov_values(void);

char * getPluginUpdateStatus(void);

void initMonitorOrigin(void);

void cfg_cleanup(void);

#ifdef    __cplusplus
}
#endif


#endif    /* _CFG_H_ */