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
 */

#ifndef    _BP_CAM_H_
#define    _BP_CAM_H_

#include <XPLMDisplay.h>
#include <XPLMUtilities.h>

#include <acfutils/geom.h>
#include <acfutils/glew.h>
#include <acfutils/types.h>

#ifdef    __cplusplus
extern "C" {
#endif

typedef struct {
    const char *filename;    /* PNG filename in data/icons/<lang> */
    const int vk;        /* function virtual key, -1 if none */
    GLuint tex;        /* OpenGL texture object */
    GLbyte *tex_data;    /* RGBA texture data */
    int w, h;        /* button width & height in pixels */
    XPLMWindowID wind_id;   /* win id */
} button_t;

typedef struct {
    const char *name;
    XPLMCommandRef cmd;
    vect3_t pos;
    vect2_t rot;
    double zoom;
} view_cmd_info_t;
#define    VCI(cmdname, x, y, z, rot_x, rot_y, zoom_incr) \
    {.name = (cmdname), .pos = VECT3((x), (y), (z)), \
    .rot = VECT2((rot_x), (rot_y)), .zoom = 1 + (zoom_incr)}
#define    VCI_POS(cmdname, x, y, z)    VCI(cmdname, x, y, z, 0, 0, 0)
#define    VCI_ROT(cmdname, x, y)        VCI(cmdname, 0, 0, 0, (x), (y), 0)
#define    VCI_ZOOM(cmdname, z)        VCI(cmdname, 0, 0, 0, 0, 0, (z))


extern int bp_plan_callback_is_alive;
extern bool_t cam_inited;

bool_t bp_cam_start(void);

bool_t bp_cam_stop(void);

bool_t bp_cam_is_running(void);

void draw_icon(button_t *btn, int x, int y, double scale,
               bool_t is_clicked, bool_t is_lit);

bool_t load_icon(button_t *btn);

void unload_icon(button_t *btn);

bool_t load_buttons(void);

void unload_buttons(void);

void nil_win_key(XPLMWindowID inWindowID, char inKey, XPLMKeyFlags inFlags,
                 char inVirtualKey, void *inRefcon, int losingFocus);

void draw_bottom_msg(int screen_x, int screen_y);

void clear_bottom_msg(void);

void init_bottom_msg(char *msg);

void eye_track_debut(void);

void eye_track_fini(void);

#ifdef    __cplusplus
}
#endif

#endif    /* _BP_CAM_H_ */
