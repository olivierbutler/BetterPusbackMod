/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license in the file COPYING
 * or http://www.opensource.org/licenses/CDDL-1.0.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file COPYING.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2022 Saso Kiselkov. All rights reserved.
 */

#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>

#include <png.h>

#include "text_rendering.h"

#include <XPLMCamera.h>
#include <XPLMGraphics.h>
#include <XPLMInstance.h>
#include <XPLMNavigation.h>
#include <XPLMScenery.h>
#include <XPLMUtilities.h>
#include <XPLMPlanes.h>
#include <XPLMProcessing.h>
#include <XPLMPlugin.h>

#include <acfutils/assert.h>
#include <acfutils/dr.h>
#include <acfutils/geom.h>
#include <acfutils/glew.h>
#include <acfutils/intl.h>
#include <acfutils/math.h>
#include <acfutils/list.h>
#include <acfutils/safe_alloc.h>
#include <acfutils/time.h>
#include <acfutils/wav.h>

#include <cglm/cglm.h>

#include "bp.h"
#include "bp_cam.h"
#include "driving.h"
#include "emergency_tow.h"
#include "gate_route_cache.h"
#include "gate_route_slots.h"
#include "planner_cache.h"
#include "xplane.h"
#include "cfg.h"
#include "msg.h"

#define MAX_PRED_DISTANCE 10000 /* meters */
#define ANGLE_DRAW_STEP 5
#define ORIENTATION_LINE_LEN 200
#define PREDICTION_KEY_POSITION_EPSILON 0.01
#define PREDICTION_KEY_HEADING_EPSILON 0.01
#define INCR_SMALL 5
#define INCR_MED 25
#define INCR_BIG 125

#define AMBER_TUPLE VECT3(0.9, 0.9, 0) /* RGB color */
#define RED_TUPLE VECT3(1, 0, 0)       /* RGB color */
#define GREEN_TUPLE VECT3(0, 1, 0)     /* RGB color */

#define CLICK_DISPL_THRESH 5        /* pixels */
#define US_PER_CLICK_ACCEL 60000    /* microseconds */
#define US_PER_CLICK_DEACCEL 120000 /* microseconds */
#define MAX_ACCEL_MULT 10
#define WHEEL_ANGLE_MULT 0.5

#define MIN_BUTTON_SCALE 0.5

#define BP_PLANNER_VISIBILITY 40000 /* meters */
#define BP_PLANNER_VISIBILITY_SM 25 /* statut miles */

#define COMPASS_ROSE_RADIUS 36
#define COMPASS_ROSE_MARGIN 18
#define COMPASS_ROSE_TICK_LONG 10
#define COMPASS_ROSE_TICK_SHORT 6
#define COMPASS_ROSE_ARROW_HEAD 7
#define COMPASS_ROSE_ARROW_DEPTH 10
#define COMPASS_ROSE_SEGMENTS 48
#define COMPASS_ROSE_RIGHT_CLEARANCE 64

#define ROUTE_PROMPT_WIDTH 640
#define ROUTE_PROMPT_OPTION_HEIGHT 46
#define ROUTE_PROMPT_OPTION_GAP 10
#define ROUTE_PROMPT_HEADER_HEIGHT 82
#define ROUTE_PROMPT_SIDE_PADDING 24

#define PREDICTION_DRAWING_PHASE xplm_Phase_Window
#define PREDICTION_DRAWING_PHASE_BEFORE 1

#if IBM
#define BOTTOM_MSG_FONT "Aileron\\Aileron-Regular.otf"
#else /* !IBM */
#define BOTTOM_MSG_FONT "Aileron/Aileron-Regular.otf"
#endif /* !IBM */
#define BOTTOM_MSG_FONT_SIZE 18

enum
{
    MARGIN_SIZE = 10
};

static vect3_t cam_pos;
static double cam_height;
static double cam_hdg;
static double cursor_hdg;
static list_t pred_segs;
static XPLMCommandRef circle_view_cmd;
static XPLMWindowID fake_win;
static vect2_t cursor_world_pos;
static bool_t force_root_win_focus = B_TRUE;
static float saved_visibility;
static int saved_cloud_types[3];

static planner_prediction_key_t planner_pred_key;
static gate_route_context_t planner_gate_context;

typedef enum {
    PLANNER_ROUTE_PROMPT_NONE,
    PLANNER_ROUTE_PROMPT_SELECT,
    PLANNER_ROUTE_PROMPT_REPLACE
} planner_route_prompt_t;

typedef struct {
    int left;
    int bottom;
    int right;
    int top;
} planner_route_prompt_rect_t;

typedef struct {
    planner_route_prompt_t prompt;
    gate_route_slot_info_t slots[GATE_ROUTE_CACHE_SLOT_COUNT];
    unsigned slot_count;
    int loaded_slot;
    int save_slot;
    int hover_choice;
    int mouse_down_choice;
    bool_t dirty;
    bool_t new_route;
    bool_t suppress_save;
} planner_gate_route_state_t;

static planner_gate_route_state_t planner_gate_routes;
static uint64_t planner_cursor_solve_count;
static uint64_t planner_cursor_reuse_count;
static float fsaved_cloud_types[3];
static bool_t saved_real_wx;
static XPLMObjectRef cam_lamp_obj = NULL;
static XPLMInstanceRef cam_lamp_inst = NULL;
static const char *cam_lamp_drefs[] = {NULL};
int bp_plan_callback_is_alive = CAMERA_IS_OFF;

static int key_sniffer(char inChar, XPLMKeyFlags inFlags, char inVirtualKey,
                       void *refcon);

static XPLMFlightLoopID bp_floop_nightlamp = NULL;
float loop_nightlamp(float elapsed, float elapsed2, int counter, void *refcon);                       

static struct
{
    dr_t local_x, local_y, local_z;
    dr_t local_vx, local_vy, local_vz;
    dr_t cam_x, cam_y, cam_z;
    dr_t hdg;
    dr_t mtow;
    dr_t tire_x, tire_z;
    dr_t tirrad;
    dr_t view_is_ext;
    dr_t visibility;
    dr_t cloud_types[3];
    dr_t use_real_wx;
    dr_t proj_matrix_3d;
    dr_t viewport;
} drs;

static view_cmd_info_t view_cmds[] = {
    VCI_POS("sim/general/left", -INCR_MED, 0, 0),
    VCI_POS("sim/general/right", INCR_MED, 0, 0),
    VCI_POS("sim/general/up", 0, 0, INCR_MED),
    VCI_POS("sim/general/down", 0, 0, -INCR_MED),
    VCI_POS("sim/general/forward", 0, -INCR_MED, 0),
    VCI_POS("sim/general/backward", 0, INCR_MED, 0),
    VCI_POS("sim/general/zoom_in", 0, -INCR_MED, 0),
    VCI_POS("sim/general/zoom_out", 0, INCR_MED, 0),
    VCI_POS("sim/general/hat_switch_left", -INCR_MED, 0, 0),
    VCI_POS("sim/general/hat_switch_right", INCR_MED, 0, 0),
    VCI_POS("sim/general/hat_switch_up", 0, 0, INCR_MED),
    VCI_POS("sim/general/hat_switch_down", 0, 0, -INCR_MED),
    VCI_POS("sim/general/hat_switch_up_left", -INCR_MED, 0, INCR_MED),
    VCI_POS("sim/general/hat_switch_up_right", INCR_MED, 0, INCR_MED),
    VCI_POS("sim/general/hat_switch_down_left", -INCR_MED, 0, -INCR_MED),
    VCI_POS("sim/general/hat_switch_down_right", INCR_MED, 0, -INCR_MED),
    VCI_POS("sim/general/left_fast", -INCR_BIG, 0, 0),
    VCI_POS("sim/general/right_fast", INCR_BIG, 0, 0),
    VCI_POS("sim/general/up_fast", 0, 0, INCR_BIG),
    VCI_POS("sim/general/down_fast", 0, 0, -INCR_BIG),
    VCI_POS("sim/general/forward_fast", 0, -INCR_BIG, 0),
    VCI_POS("sim/general/backward_fast", 0, INCR_BIG, 0),
    VCI_POS("sim/general/zoom_in_fast", 0, -INCR_BIG, 0),
    VCI_POS("sim/general/zoom_out_fast", 0, INCR_BIG, 0),
    VCI_POS("sim/general/left_slow", -INCR_SMALL, 0, 0),
    VCI_POS("sim/general/right_slow", INCR_SMALL, 0, 0),
    VCI_POS("sim/general/up_slow", 0, 0, INCR_SMALL),
    VCI_POS("sim/general/down_slow", 0, 0, -INCR_SMALL),
    VCI_POS("sim/general/forward_slow", 0, -INCR_SMALL, 0),
    VCI_POS("sim/general/backward_slow", 0, INCR_SMALL, 0),
    VCI_POS("sim/general/zoom_in_slow", 0, -INCR_SMALL, 0),
    VCI_POS("sim/general/zoom_out_slow", 0, INCR_SMALL, 0),
    {.name = NULL}};

static button_t buttons[] = {
    {.filename = "move_view.png", .vk = -1, .tex = 0, .tex_data = NULL},
    {.filename = "place_seg.png", .vk = -1, .tex = 0, .tex_data = NULL},
    {.filename = "rotate_seg.png", .vk = -1, .tex = 0, .tex_data = NULL},
    {.filename = "", .vk = -1, .tex = 0, .tex_data = NULL, .h = 64},
    {.filename = "accept_plan.png", .vk = XPLM_VK_RETURN, .tex = 0, .tex_data = NULL},
    {.filename = "delete_seg.png", .vk = XPLM_VK_DELETE, .tex = 0, .tex_data = NULL},
    {.filename = "", .vk = -1, .tex = 0, .tex_data = NULL, .h = 64},
    {.filename = "cancel_plan.png", .vk = XPLM_VK_ESCAPE, .tex = 0, .tex_data = NULL},
    {.filename = "conn_first.png", .vk = XPLM_VK_SPACE, .tex = 0, .tex_data = NULL},
    {.filename = NULL}};
static int button_hit = -1, button_lit = -1;
bool_t cam_inited = B_FALSE;

static struct
{
    char *msg;
    int timeout;
    uint64_t end;
    int width;
    int height;
    GLuint texture;
    uint8_t *bytes;
} bottom_msg = {NULL, 0, 0, 0, 0, 0, NULL};

static FT_Library ft;
static FT_Face face;

static struct
{
    int plg_status;
    XPLMPluginID plg_id;
} eye_tracker_plg = {0, -1};

bool_t
load_icon(button_t *btn)
{
    char *filename;
    FILE *fp;
    size_t rowbytes;
    png_bytep *volatile rowp = NULL;
    png_structp pngp = NULL;
    png_infop infop = NULL;
    volatile bool_t res = B_TRUE;
    uint8_t header[8];

    /* try the localized version first */
    filename = mkpathname(bp_xpdir, bp_plugindir, "data", "icons",
                          bp_get_lang(), btn->filename, NULL);
    if (!file_exists(filename, NULL))
    {
        /* if the localized version failed, try the English version */
        free(filename);
        filename = mkpathname(bp_xpdir, bp_plugindir, "data", "icons",
                              "en", btn->filename, NULL);
    }
    fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        logMsg(BP_ERROR_LOG "Cannot open file %s: %s", filename, strerror(errno));
        res = B_FALSE;
        goto out;
    }
    if (fread(header, 1, sizeof(header), fp) != 8 ||
        png_sig_cmp(header, 0, sizeof(header)) != 0)
    {
        logMsg(BP_ERROR_LOG "Cannot open file %s: invalid PNG header", filename);
        res = B_FALSE;
        goto out;
    }
    pngp = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    VERIFY(pngp != NULL);
    infop = png_create_info_struct(pngp);
    VERIFY(infop != NULL);
    if (setjmp(png_jmpbuf(pngp)))
    {
        logMsg(BP_ERROR_LOG "Cannot open file %s: libpng error in init_io",
               filename);
        res = B_FALSE;
        goto out;
    }
    png_init_io(pngp, fp);
    png_set_sig_bytes(pngp, 8);

    if (setjmp(png_jmpbuf(pngp)))
    {
        logMsg(BP_ERROR_LOG "Cannot open file %s: libpng read info failed",
               filename);
        res = B_FALSE;
        goto out;
    }
    png_read_info(pngp, infop);
    btn->w = png_get_image_width(pngp, infop);
    btn->h = png_get_image_height(pngp, infop);

    if (png_get_color_type(pngp, infop) != PNG_COLOR_TYPE_RGBA)
    {
        logMsg(BP_ERROR_LOG "Bad icon file %s: need color type RGBA", filename);
        res = B_FALSE;
        goto out;
    }
    if (png_get_bit_depth(pngp, infop) != 8)
    {
        logMsg(BP_ERROR_LOG "Bad icon file %s: need 8-bit depth", filename);
        res = B_FALSE;
        goto out;
    }
    rowbytes = png_get_rowbytes(pngp, infop);

    rowp = safe_malloc(sizeof(*rowp) * btn->h);
    VERIFY(rowp != NULL);
    for (int i = 0; i < btn->h; i++)
    {
        rowp[i] = safe_malloc(rowbytes);
        VERIFY(rowp[i] != NULL);
    }

    if (setjmp(png_jmpbuf(pngp)))
    {
        logMsg(BP_ERROR_LOG "Bad icon file %s: error reading image file", filename);
        res = B_FALSE;
        goto out;
    }
    png_read_image(pngp, rowp);

    btn->tex_data = safe_malloc(btn->h * rowbytes);
    for (int i = 0; i < btn->h; i++)
        memcpy(&btn->tex_data[i * rowbytes], rowp[i], rowbytes);

    glGenTextures(1, &btn->tex);
    glBindTexture(GL_TEXTURE_2D, btn->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, btn->w, btn->h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, btn->tex_data);

out:
    if (pngp != NULL)
        png_destroy_read_struct(&pngp, &infop, NULL);
    if (rowp != NULL)
    {
        for (int i = 0; i < btn->h; i++)
            free(rowp[i]);
        free(rowp);
    }
    if (fp != NULL)
        fclose(fp);
    free(filename);

    return (res);
}

void unload_icon(button_t *btn)
{
    if (btn->tex != 0)
    {
        glDeleteTextures(1, &btn->tex);
        btn->tex = 0;
    }
    if (btn->tex_data != NULL)
    {
        free(btn->tex_data);
        btn->tex_data = NULL;
    }
}

bool_t
load_buttons(void)
{
    for (int i = 0; buttons[i].filename != NULL; i++)
    {
        /* skip spacers */
        if (strcmp(buttons[i].filename, "") == 0)
            continue;
        if (!load_icon(&buttons[i]))
        {
            unload_buttons();
            return (B_FALSE);
        }
    }

    return (B_TRUE);
}

void unload_buttons(void)
{
    for (int i = 0; buttons[i].filename != NULL; i++)
        unload_icon(&buttons[i]);
}

static int
move_camera(XPLMCommandRef cmd, XPLMCommandPhase phase, void *refcon)
{
    static uint64_t last_cmd_t = 0;
    UNUSED(cmd);

    if (phase == xplm_CommandBegin)
    {
        last_cmd_t = microclock();
    }
    else if (phase == xplm_CommandContinue)
    {
        uint64_t now = microclock();
        double d_t = (now - last_cmd_t) / 1000000.0;
        unsigned i = (uintptr_t)refcon;
        vect2_t v = vect2_rot(VECT2(view_cmds[i].pos.x,
                                    view_cmds[i].pos.z),
                              cam_hdg);
        cam_pos = vect3_add(cam_pos, VECT3(v.x * d_t, 0, v.y * d_t));
        cam_height += view_cmds[i].pos.y * d_t;
        last_cmd_t = now;
    }
    return (0);
}

static void
get_vp(vec4 vp)
{

    ASSERT(vp != NULL);

    vp[0] = monitor_def.x_origin;
    vp[1] = monitor_def.y_origin;
    vp[2] = monitor_def.w;
    vp[3] = monitor_def.h;
}

/*
 * Un-projects a viewport coordinate at X, Y using the current projection
 * matrix and figures out to which point on the reference plane it
 * corresponds. The reference plane is a plane that is parallel with the
 * Earth's surface at the local coordinate origin and is elevation-centered
 * on the aircraft's current local Y coordinate.
 */
static void
vp_unproject(double x, double y, double *x_phys, double *y_phys)
{
    mat4 proj;
    vec4 vp;
    vec3 out_pt;

    ASSERT(x_phys != NULL);
    ASSERT(y_phys != NULL);

    VERIFY3S(dr_getvf32(&drs.proj_matrix_3d, (float *)proj, 0, 16), ==, 16);
    get_vp(vp);
    glm_unproject((vec3){x, y, 0.5}, proj, vp, out_pt);
    /*
     * To avoid having to figure out the viewport Z coordinate that
     * matches the reference plane distance, we scale the returned
     * 3D coordinate based on Z-distance of the camera from the
     * reference plane.
     */
    ASSERT(!isnan(out_pt[0]));
    ASSERT(!isnan(out_pt[1]));
    ASSERT(out_pt[2] != 0);
    glm_vec3_scale(out_pt, ABS(cam_height / out_pt[2]), out_pt);
    ASSERT(!isnan(out_pt[0]));
    ASSERT(!isnan(out_pt[1]));
    *x_phys = out_pt[0];
    *y_phys = out_pt[1];
}

static bool_t
planner_clear_segments(list_t *segments)
{
    seg_t *seg;
    bool_t changed = B_FALSE;

    while ((seg = list_remove_head(segments)) != NULL) {
        free(seg);
        changed = B_TRUE;
    }
    return (changed);
}

static bool_t
planner_clear_predicted_segments(void)
{
    return (planner_clear_segments(&pred_segs));
}

static uint64_t
planner_hash_segments(uint64_t hash, const list_t *segments, uint64_t tag)
{
    uint64_t count = 0;

    hash = planner_hash_u64(hash, tag);
    for (const seg_t *seg = list_head(segments); seg != NULL;
        seg = list_next(segments, seg)) {
        count++;
        hash = planner_hash_u64(hash, (uint64_t)seg->type);
        hash = planner_hash_u64(hash, (uint64_t)seg->have_local_coords);
        hash = planner_hash_double(hash, seg->start_pos.x);
        hash = planner_hash_double(hash, seg->start_pos.y);
        hash = planner_hash_double(hash, seg->start_hdg);
        hash = planner_hash_double(hash, seg->end_pos.x);
        hash = planner_hash_double(hash, seg->end_pos.y);
        hash = planner_hash_double(hash, seg->end_hdg);
        hash = planner_hash_u64(hash, (uint64_t)seg->backward);
        if (seg->type == SEG_TYPE_TURN) {
            hash = planner_hash_double(hash, seg->turn.r);
            hash = planner_hash_u64(hash, (uint64_t)seg->turn.right);
        } else {
            hash = planner_hash_double(hash, seg->len);
        }
        hash = planner_hash_u64(hash, (uint64_t)seg->user_placed);
    }
    return (planner_hash_u64(hash, count));
}

static uint64_t
planner_committed_signature(void)
{
    return (planner_hash_segments(planner_hash_init(), &bp.segs,
        UINT64_C(0x434f4d4d49545445)));
}

static vect2_t
planner_aircraft_gear_position(double gear_z)
{
    vect2_t origin = VECT2(dr_getf(&drs.local_x),
        -dr_getf(&drs.local_z));
    double heading = dr_getf(&drs.hdg);

    return vect2_add(origin,
        vect2_scmul(hdg2dir(heading), -gear_z));
}

static vect2_t
planner_nosewheel_position(void)
{
    return planner_aircraft_gear_position(bp.acf.nw_z);
}

static bool_t
planner_prepare_gate_context(double *match_distance, double *match_heading)
{
    XPLMProbeRef probe;
    XPLMProbeInfo_t info = {.structSize = sizeof(info)};
    vect2_t nosewheel = planner_nosewheel_position();
    geo_pos2_t nosewheel_geo;
    char aircraft[256] = {0};
    char aircraft_path[512] = {0};
    double unused;

    gate_route_context_reset(&planner_gate_context);
    probe = XPLMCreateProbe(xplm_ProbeY);
    if (probe == NULL)
        return B_FALSE;
    if (XPLMProbeTerrainXYZ(probe, nosewheel.x, 0, -nosewheel.y,
        &info) != xplm_ProbeHitTerrain) {
        XPLMDestroyProbe(probe);
        return B_FALSE;
    }
    XPLMDestroyProbe(probe);

    XPLMLocalToWorld(nosewheel.x, info.locationY, -nosewheel.y,
        &nosewheel_geo.lat, &nosewheel_geo.lon, &unused);
    XPLMGetNthAircraftModel(0, aircraft, aircraft_path);
    return gate_route_find_published_start(airportdb, nosewheel_geo,
        nosewheel, dr_getf(&drs.hdg), aircraft, bp.veh.wheelbase,
        bp.acf.nw_z, bp.acf.main_z, &planner_gate_context,
        match_distance, match_heading);
}

static int
cam_ctl(XPLMCameraPosition_t *pos, int losing_control, void *refcon)
{
    int x, y;
    double dx, dy, start_hdg;
    vect2_t start_pos, end_pos;
    seg_t *seg;
    int n;

    UNUSED(refcon);

    if (pos == NULL || losing_control || !cam_inited)
        return (0);

    bp_plan_callback_is_alive = CAMERA_TIMEOUT;

    pos->x = cam_pos.x;
    pos->y = cam_pos.y + cam_height;
    pos->z = -cam_pos.z;
    pos->pitch = -90;
    pos->heading = cam_hdg;
    pos->roll = 0;
    pos->zoom = 1;

    XPLMGetMouseLocationGlobal(&x, &y);
    vp_unproject(x, y, &dx, &dy);

    /*
     * Don't make predictions if due to the camera FOV angle (>= 180 deg)
     * we could be placing the prediction object very far away.
     */
    if (fabs(dx) > MAX_PRED_DISTANCE || fabs(dy) > MAX_PRED_DISTANCE) {
        planner_clear_predicted_segments();
        planner_prediction_key_invalidate(&planner_pred_key);
        return (1);
    }

    seg = list_tail(&bp.segs);
    if (seg != NULL)
    {
        start_pos = seg->end_pos;
        start_hdg = seg->end_hdg;
    }
    else
    {
        start_pos = VECT2(dr_getf(&drs.local_x),
            -dr_getf(&drs.local_z)); /* inverted X-Plane Z */
        start_hdg = dr_getf(&drs.hdg);
    }

    end_pos = vect2_add(VECT2(cam_pos.x, cam_pos.z),
                        vect2_rot(VECT2(dx, dy), pos->heading));
    cursor_world_pos = VECT2(end_pos.x, end_pos.y);

    {
        uint64_t base_signature = planner_committed_signature();

        if (planner_prediction_key_matches(&planner_pred_key,
            base_signature, start_pos.x, start_pos.y, start_hdg,
            end_pos.x, end_pos.y, cursor_hdg,
            PREDICTION_KEY_POSITION_EPSILON,
            PREDICTION_KEY_HEADING_EPSILON)) {
            planner_cursor_reuse_count++;
            return (1);
        }

        planner_clear_predicted_segments();
        planner_cursor_solve_count++;
        n = compute_segs(&bp.veh, start_pos, start_hdg, end_pos,
            cursor_hdg, &pred_segs);
        planner_prediction_key_set(&planner_pred_key, base_signature,
            start_pos.x, start_pos.y, start_hdg, end_pos.x, end_pos.y,
            cursor_hdg);
    }

    if (n > 0)
    {
        seg = list_tail(&pred_segs);
        seg->user_placed = B_TRUE;
    }

    return (1);
}

#if 0
/* Retained for reference only; live and preview steering use legacy paths. */
static double
planner_tail_arm(void)
{
    double aft_y = -HUGE_VAL;

    if (bp_ls.outline != NULL) {
        for (size_t i = 0; i < bp_ls.outline->num_pts; i++) {
            vect2_t point = bp_ls.outline->pts[i];

            if (!IS_NULL_VECT(point) && isfinite(point.y))
                aft_y = MAX(aft_y, point.y);
        }
        if (isfinite(aft_y) && aft_y > bp.acf.main_z)
            return (aft_y - bp.acf.main_z);
        if (bp_ls.outline->length > 0)
            return (MAX(bp_ls.outline->length * 0.4,
                bp.veh.wheelbase / 2));
    }

    return (MAX(bp.veh.wheelbase, 1));
}

static double
planner_deadband(double value, double deadband)
{
    if (fabs(value) <= deadband)
        return (0);
    return (value - copysign(deadband, value));
}

static const seg_t *
planner_tail_target(const list_t *segs, const seg_t *current)
{
    const seg_t *target = current;

    if (current->type == SEG_TYPE_STRAIGHT)
        return (current);

    for (const seg_t *candidate = current; candidate != NULL;
        candidate = list_next(segs, candidate)) {
        target = candidate;
        if (candidate->user_placed)
            break;
    }
    return (target);
}

/* Mirrors automatic_steer_target without touching live pushback state. */
static double
planner_steer_target(const list_t *segs, const seg_t *seg,
    const vehicle_t *veh, const vehicle_pos_t *pose, double route_steer,
    double tail_arm, planner_prediction_state_t *state)
{
    const seg_t *control_target = planner_tail_target(segs, seg);
    double signed_turn = 0, total_distance = 0, direction = 0;
    double profile_target = 0, capture_fraction = 1;
    double cross_track, along_remaining, heading_error, feedback;
    double path_weight, path_correction;
    vect2_t aircraft_dir = hdg2dir(pose->hdg);
    vect2_t target_dir = hdg2dir(control_target->end_hdg);
    vect2_t target_right = vect2_norm(target_dir, B_TRUE);
    vect2_t travel_dir = (control_target->backward ?
        vect2_neg(target_dir) : target_dir);
    vect2_t tail_pos = vect2_add(pose->pos,
        vect2_scmul(aircraft_dir, -tail_arm));
    vect2_t target_tail_pos = vect2_add(control_target->end_pos,
        vect2_scmul(target_dir, -tail_arm));

    if (seg->type == SEG_TYPE_TURN) {
        signed_turn = rel_hdg(seg->start_hdg, seg->end_hdg);
        total_distance = DEG2RAD(fabs(signed_turn)) * seg->turn.r;
        if (!state->turn_active ||
            fabs(rel_hdg(state->turn_end_hdg, seg->end_hdg)) > 1e-6) {
            state->turn_active = B_TRUE;
            state->turn_distance = 0;
            state->turn_end_hdg = seg->end_hdg;
        }
        state->turn_distance = MIN(state->turn_distance +
            fabs(pose->spd) * PREDICTION_DT, total_distance);
        direction = (signed_turn >= 0 ? 1 : -1) *
            (seg->backward ? -1 : 1);
        profile_target = vehicle_turn_profile_steer(veh->wheelbase,
            seg->turn.r, total_distance, state->turn_distance,
            TURN_PROFILE_TRANSITION_DIST, direction, veh->max_steer);
        capture_fraction = (total_distance > 0 ?
            2 * (state->turn_distance / total_distance - 0.5) : 1);
        capture_fraction = MIN(MAX(capture_fraction, 0), 1);
    } else {
        state->turn_active = B_FALSE;
        state->turn_distance = 0;
    }

    cross_track = vect2_dotprod(vect2_sub(tail_pos, target_tail_pos),
        target_right);
    along_remaining = vect2_dotprod(vect2_sub(target_tail_pos, tail_pos),
        travel_dir);
    heading_error = rel_hdg(pose->hdg, control_target->end_hdg);
    feedback = vehicle_tail_steer_correction(
        planner_deadband(cross_track, TAIL_CROSS_TRACK_DEADBAND),
        planner_deadband(heading_error, TAIL_HEADING_DEADBAND),
        capture_fraction, TAIL_CROSS_TRACK_GAIN, TAIL_HEADING_GAIN,
        TAIL_MAX_STEER_CORRECTION, seg->backward);
    path_weight = vehicle_path_terminal_weight(along_remaining,
        ROUTE_PATH_TERMINAL_FADE_DIST, list_next(segs, seg) == NULL);
    path_correction = vehicle_path_steer_correction(route_steer,
        profile_target, ROUTE_PATH_STEER_DEADBAND,
        ROUTE_PATH_MAX_CORRECTION, path_weight);

    return (MIN(MAX(profile_target + path_correction + feedback,
        -veh->max_steer), veh->max_steer));
}

static void
planner_copy_segments(const list_t *source, list_t *destination)
{
    for (const seg_t *seg = list_head(source); seg != NULL;
        seg = list_next(source, seg)) {
        seg_t *copy = safe_calloc(1, sizeof(*copy));

        *copy = *seg;
        memset(&copy->node, 0, sizeof(copy->node));
        list_insert_tail(destination, copy);
    }
}

static void
planner_path_append(const vehicle_pos_t *pose)
{
    if (planner_path_count >= MAX_PREDICTION_POINTS)
        return;
    planner_path[planner_path_count++] = (planner_path_point_t) {
        pose->pos, pose->hdg
    };
}

/*
 * Simulate the planned route through the same steering target and rate limits
 * used during pushback. The controller pose is the aircraft main-gear point;
 * drawing converts each sampled pose to the corresponding nosewheel point.
 */
static bool_t
planner_build_controller_path(void)
{
    list_t work;
    vehicle_t veh = bp.veh;
    vehicle_pos_t pose;
    planner_prediction_state_t state = {0};
    seg_t *seg;
    double last_mis_hdg = 0, sample_distance = 0;
    double tail_arm = planner_tail_arm();
    bool_t complete = B_FALSE;

    planner_path_count = 0;
    list_create(&work, sizeof(seg_t), offsetof(seg_t, node));
    planner_copy_segments(&bp.segs, &work);
    planner_copy_segments(&pred_segs, &work);
    seg = list_head(&work);
    if (seg == NULL) {
        list_destroy(&work);
        return (B_FALSE);
    }

    veh.fixed_z_off = 0;
    veh.use_rear_pos = B_FALSE;
    pose = (vehicle_pos_t) {seg->start_pos, seg->start_hdg, 0};
    planner_path_append(&pose);

    for (unsigned step = 0; step < MAX_PREDICTION_STEPS; step++) {
        double route_steer, target_speed, target_steer;
        double next_speed, next_steer, distance;
        bool_t decelerating = B_FALSE;

        seg = list_head(&work);
        if (seg == NULL) {
            complete = B_TRUE;
            break;
        }
        if (!drive_segs(&pose, &veh, &work, &last_mis_hdg,
            PREDICTION_DT, &route_steer, &target_speed, &decelerating))
            continue;

        target_steer = planner_steer_target(&work, seg, &veh, &pose,
            route_steer, tail_arm, &state);
        next_steer = vehicle_steering_step(state.steer, target_steer,
            TURN_PROFILE_STEER_RATE, PREDICTION_DT);
        next_speed = vehicle_speed_step(pose.spd, target_speed,
            veh.max_accel, veh.max_decel, PREDICTION_DT);
        distance = (pose.spd + next_speed) / 2 * PREDICTION_DT;
        vehicle_bicycle_step(veh.wheelbase,
            (pose.spd + next_speed) / 2,
            (state.steer + next_steer) / 2, PREDICTION_DT,
            &pose.pos.x, &pose.pos.y, &pose.hdg);
        pose.spd = next_speed;
        state.steer = next_steer;
        sample_distance += fabs(distance);
        if (sample_distance >= PREDICTION_SAMPLE_DISTANCE) {
            planner_path_append(&pose);
            sample_distance = 0;
        }
        if (planner_path_count >= MAX_PREDICTION_POINTS)
            break;
    }

    if (planner_path_count < MAX_PREDICTION_POINTS)
        planner_path_append(&pose);
    while ((seg = list_remove_head(&work)) != NULL)
        free(seg);
    list_destroy(&work);

    return (complete && planner_path_count > 1);
}

static bool_t
planner_draw_danger_zone_geometry(void)
{
    const double radius = bp_ls.outline->semispan;

    glBegin(GL_QUADS);
    for (size_t i = 1; i < planner_path_count; i++) {
        vect2_t p1 = planner_path[i - 1].pos;
        vect2_t p2 = planner_path[i].pos;
        vect2_t tangent = vect2_sub(p2, p1);
        double length = vect2_abs(tangent);
        vect2_t normal;

        if (length < 1e-6)
            continue;
        normal = VECT2(-tangent.y / length * radius,
            tangent.x / length * radius);
        glVertex3f(p1.x + normal.x, planner_path_height[i - 1],
            -(p1.y + normal.y));
        glVertex3f(p1.x - normal.x, planner_path_height[i - 1],
            -(p1.y - normal.y));
        glVertex3f(p2.x - normal.x, planner_path_height[i],
            -(p2.y - normal.y));
        glVertex3f(p2.x + normal.x, planner_path_height[i],
            -(p2.y + normal.y));
    }
    glEnd();

    /* Round joins union neighboring segment rectangles into one smooth band. */
    glBegin(GL_TRIANGLES);
    for (size_t i = 1; i + 1 < planner_path_count; i++) {
        vect2_t center = planner_path[i].pos;
        vect2_t incoming = vect2_sub(center, planner_path[i - 1].pos);
        vect2_t outgoing = vect2_sub(planner_path[i + 1].pos, center);
        double incoming_len = vect2_abs(incoming);
        double outgoing_len = vect2_abs(outgoing);
        double turn_sine;

        if (incoming_len < 1e-6 || outgoing_len < 1e-6)
            continue;
        turn_sine = (incoming.x * outgoing.y - incoming.y * outgoing.x) /
            (incoming_len * outgoing_len);
        if (fabs(turn_sine) < 0.001)
            continue;

        for (unsigned j = 0; j < DANGER_ZONE_CIRCLE_SEGMENTS; j++) {
            double a1 = 2 * M_PI * j / DANGER_ZONE_CIRCLE_SEGMENTS;
            double a2 = 2 * M_PI * (j + 1) /
                DANGER_ZONE_CIRCLE_SEGMENTS;

            glVertex3f(center.x, planner_path_height[i], -center.y);
            glVertex3f(center.x + cos(a1) * radius,
                planner_path_height[i], -(center.y + sin(a1) * radius));
            glVertex3f(center.x + cos(a2) * radius,
                planner_path_height[i], -(center.y + sin(a2) * radius));
        }
    }
    glEnd();
    return (B_TRUE);
}

static bool_t
planner_draw_danger_zone(void)
{
    GLint stencil_bits = 0;

    glGetIntegerv(GL_STENCIL_BITS, &stencil_bits);
    if (stencil_bits <= 0)
        return (B_FALSE);

    /*
     * First build the union in the stencil buffer. The color pass clears a
     * stencil pixel as it shades it, so overlapping rectangles and round
     * joins can never accumulate alpha or expose their individual triangles.
     */
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xff);
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glStencilFunc(GL_ALWAYS, 1, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    planner_draw_danger_zone_geometry();

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glStencilFunc(GL_EQUAL, 1, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glColor4f(1, 0.05, 1, 0.22);
    planner_draw_danger_zone_geometry();

    glClear(GL_STENCIL_BUFFER_BIT);
    glPopAttrib();
    return (B_TRUE);
}

static bool_t
planner_build_terrain_heights(uint64_t *probe_count)
{
    XPLMProbeRef probe;

    ASSERT(probe_count != NULL);
    *probe_count = 0;
    probe = XPLMCreateProbe(xplm_ProbeY);
    if (probe == NULL)
        return (B_FALSE);
    for (size_t i = 0; i < planner_path_count; i++) {
        XPLMProbeInfo_t info = {.structSize = sizeof(info)};

        (*probe_count)++;
        if (XPLMProbeTerrainXYZ(probe, planner_path[i].pos.x, 0,
            -planner_path[i].pos.y,
            &info) == xplm_ProbeHitTerrain) {
            planner_path_height[i] = info.locationY + 0.05f;
        } else if (i > 0) {
            planner_path_height[i] = planner_path_height[i - 1];
        } else {
            XPLMDestroyProbe(probe);
            return (B_FALSE);
        }
    }
    XPLMDestroyProbe(probe);
    return (B_TRUE);
}

static bool_t
draw_controller_path(void)
{
    uint64_t signature, build_start, build_us, terrain_probes = 0;

    if (bp_ls.outline == NULL)
        return (B_FALSE);

    signature = planner_prediction_signature();
    planner_cache_observe(&planner_path_cache, signature);
    if (planner_cache_needs_rebuild(&planner_path_cache)) {
        bool_t success;

        build_start = microclock();
        success = planner_build_controller_path();
        if (success)
            success = planner_build_terrain_heights(&terrain_probes);
        build_us = microclock() - build_start;
        planner_cache_commit(&planner_path_cache, success != B_FALSE,
            planner_path_count, build_us, terrain_probes);

        if (!success) {
            if (!planner_prediction_failure_announced &&
                (list_head(&bp.segs) != NULL ||
                list_head(&pred_segs) != NULL)) {
                logMsg(BP_WARN_LOG "Controller-matched planner preview "
                    "could not complete; cached geometric fallback for "
                    "route revision %llu",
                    (unsigned long long)planner_path_cache.revision);
                planner_prediction_failure_announced = B_TRUE;
            }
        } else {
            planner_prediction_failure_announced = B_FALSE;
            if (!planner_prediction_announced) {
                const seg_t *last = list_tail(&pred_segs);

                if (last == NULL)
                    last = list_tail(&bp.segs);
                logMsg(BP_INFO_LOG "Controller-matched cached planner "
                    "preview active (%u path points, endpoint offset "
                    "%.2f m, first build %.2f ms)",
                    (unsigned)planner_path_count,
                    last != NULL ? vect2_dist(
                    planner_path[planner_path_count - 1].pos,
                    last->end_pos) : 0, build_us / 1000.0);
                planner_prediction_announced = B_TRUE;
            }
        }
    } else {
        planner_cache_note_hit(&planner_path_cache);
    }

    if (!planner_path_cache.have_result ||
        !planner_path_cache.result_success)
        return (B_FALSE);

    if (!planner_draw_danger_zone()) {
        if (!planner_band_failure_announced) {
            logMsg(BP_WARN_LOG "Planner danger-zone fill unavailable; "
                "drawing controller path without the band");
            planner_band_failure_announced = B_TRUE;
        }
    } else {
        planner_band_failure_announced = B_FALSE;
    }

    /*
     * The controller works from the main gear, while the pilot-facing blue
     * trajectory shows the corresponding nosewheel path. This makes its first
     * point the published ramp-start anchor without changing steering physics.
     */
    XPLMSetGraphicsState(0, 0, 0, 0, 0, 0, 0);
    glColor3f(0, 0, 1);
    glLineWidth(3);
    glBegin(GL_LINE_STRIP);
    for (size_t i = 0; i < planner_path_count; i++) {
        vect2_t nosewheel = planner_nosewheel_from_main_gear(
            planner_path[i].pos, planner_path[i].hdg);

        glVertex3f(nosewheel.x, planner_path_height[i], -nosewheel.y);
    }
    glEnd();
    return (B_TRUE);
}
#endif

static void
draw_segment(const seg_t *seg)
{
    XPLMProbeRef probe = XPLMCreateProbe(xplm_ProbeY);
    XPLMProbeInfo_t info = {.structSize = sizeof(XPLMProbeInfo_t)};
    double wing_long_off = bp.acf.main_z - bp_ls.outline->wingtip.y;
    vect2_t wing_off_l = VECT2(-bp_ls.outline->semispan, wing_long_off);
    vect2_t wing_off_r = VECT2(bp_ls.outline->semispan, wing_long_off);

    switch (seg->type)
    {
    case SEG_TYPE_STRAIGHT:
    {
        float h1, h2;
        vect2_t wing_l, wing_r, p;

        // VERIFY3U(XPLMProbeTerrainXYZ(probe, seg->start_pos.x, 0,
        //                              -seg->start_pos.y, &info), ==, xplm_ProbeHitTerrain);
        if (XPLMProbeTerrainXYZ(probe, seg->start_pos.x, 0,
                                -seg->start_pos.y, &info) != xplm_ProbeHitTerrain)
        {
            XPLMDestroyProbe(probe);
            return;
        }

        h1 = info.locationY;
        // VERIFY3U(XPLMProbeTerrainXYZ(probe, seg->end_pos.x, 0,
        //                              -seg->end_pos.y, &info), ==, xplm_ProbeHitTerrain);
        if (XPLMProbeTerrainXYZ(probe, seg->end_pos.x, 0,
                                -seg->end_pos.y, &info) != xplm_ProbeHitTerrain)
        {
            XPLMDestroyProbe(probe);
            return;
        }
        h2 = info.locationY;

        glColor3f(0, 0, 1);
        glLineWidth(3);
        glBegin(GL_LINES);
        glVertex3f(seg->start_pos.x, h1, -seg->start_pos.y);
        glVertex3f(seg->end_pos.x, h2, -seg->end_pos.y);
        glEnd();

        wing_l = vect2_rot(wing_off_l, seg->start_hdg);
        wing_r = vect2_rot(wing_off_r, seg->start_hdg);

        glColor3f(1, 0.25, 1);
        glLineWidth(2);
        glBegin(GL_LINES);
        p = vect2_add(seg->start_pos, wing_r);
        glVertex3f(p.x, h1, -p.y);
        p = vect2_add(seg->end_pos, wing_r);
        glVertex3f(p.x, h1, -p.y);
        p = vect2_add(seg->end_pos, wing_l);
        glVertex3f(p.x, h1, -p.y);
        p = vect2_add(seg->start_pos, wing_l);
        glVertex3f(p.x, h1, -p.y);
        glEnd();
        break;
    }
    case SEG_TYPE_TURN:
    {
        vect2_t c = vect2_add(seg->start_pos, vect2_scmul(
                                                  vect2_norm(hdg2dir(seg->start_hdg), seg->turn.right),
                                                  seg->turn.r));
        vect2_t c2s = vect2_sub(seg->start_pos, c);
        double s, e, rhdg;

        rhdg = rel_hdg(seg->start_hdg, seg->end_hdg);
        s = MIN(0, rhdg);
        e = MAX(0, rhdg);
        ASSERT3F(s, <=, e);
        for (double a = s; a < e; a += ANGLE_DRAW_STEP)
        {
            vect2_t p1, p2, p;
            vect2_t wing1_l, wing1_r, wing2_l, wing2_r;
            double step = MIN(ANGLE_DRAW_STEP, e - a);

            wing1_l = vect2_rot(wing_off_l, seg->start_hdg + a);
            wing1_r = vect2_rot(wing_off_r, seg->start_hdg + a);
            wing2_l = vect2_rot(wing_off_l,
                                seg->start_hdg + a + step);
            wing2_r = vect2_rot(wing_off_r,
                                seg->start_hdg + a + step);

            p1 = vect2_add(c, vect2_rot(c2s, a));
            p2 = vect2_add(c, vect2_rot(c2s, a + step));

            // VERIFY3U(XPLMProbeTerrainXYZ(probe, p1.x, 0, -p1.y,
            //                              &info), ==, xplm_ProbeHitTerrain);

            if (XPLMProbeTerrainXYZ(probe, p1.x, 0, -p1.y,
                                    &info) != xplm_ProbeHitTerrain)
            {
                XPLMDestroyProbe(probe);
                return;
            }
            glColor3f(0, 0, 1);
            glLineWidth(3);
            glBegin(GL_LINES);
            glVertex3f(p1.x, info.locationY, -p1.y);
            glVertex3f(p2.x, info.locationY, -p2.y);
            glEnd();

            glColor3f(1, 0.25, 1);
            glLineWidth(2);
            glBegin(GL_LINES);
            p = vect2_add(p1, wing1_r);
            glVertex3f(p.x, info.locationY, -p.y);
            p = vect2_add(p2, wing2_r);
            glVertex3f(p.x, info.locationY, -p.y);
            p = vect2_add(p1, wing1_l);
            glVertex3f(p.x, info.locationY, -p.y);
            p = vect2_add(p2, wing2_l);
            glVertex3f(p.x, info.locationY, -p.y);
            glEnd();
        }
        break;
    }
    }

    XPLMDestroyProbe(probe);
}

static void
draw_acf_symbol(vect3_t pos, double hdg, vect3_t color)
{
    vect2_t v;
    vect3_t p;
    double tire_x[10], tire_z[10], tirrad[10];

    glLineWidth(2);
    glColor3f(color.x, color.y, color.z);

    glBegin(GL_LINES);
    for (size_t i = 0; i + 1 < bp_ls.outline->num_pts; i++)
    {
        /* skip gaps in outline */
        if (IS_NULL_VECT(bp_ls.outline->pts[i]) ||
            IS_NULL_VECT(bp_ls.outline->pts[i + 1]))
            continue;

        v = bp_ls.outline->pts[i];
        v = vect2_rot(VECT2(v.x, bp.acf.main_z - v.y), hdg);
        p = vect3_add(pos, VECT3(v.x, 0, v.y));
        glVertex3f(p.x, p.y, -p.z);

        v = bp_ls.outline->pts[i + 1];
        v = vect2_rot(VECT2(v.x, bp.acf.main_z - v.y), hdg);
        p = vect3_add(pos, VECT3(v.x, 0, v.y));
        glVertex3f(p.x, p.y, -p.z);

        v = bp_ls.outline->pts[i];
        v = vect2_rot(VECT2(-v.x, bp.acf.main_z - v.y), hdg);
        p = vect3_add(pos, VECT3(v.x, 0, v.y));
        glVertex3f(p.x, p.y, -p.z);

        v = bp_ls.outline->pts[i + 1];
        v = vect2_rot(VECT2(-v.x, bp.acf.main_z - v.y), hdg);
        p = vect3_add(pos, VECT3(v.x, 0, v.y));
        glVertex3f(p.x, p.y, -p.z);
    }
    glEnd();

    dr_getvf(&drs.tire_x, tire_x, 0, 10);
    dr_getvf(&drs.tire_z, tire_z, 0, 10);
    dr_getvf(&drs.tirrad, tirrad, 0, 10);

    glBegin(GL_QUADS);
    for (int i = 0; i < bp.acf.n_gear; i++)
    {
        double tr = tirrad[bp.acf.gear_is[i]];
        vect2_t c;

        v = VECT2(tire_x[bp.acf.gear_is[i]],
                  bp.acf.main_z - tire_z[bp.acf.gear_is[i]]);

        c = vect2_rot(vect2_add(v, VECT2(-tr, -tr)), hdg);
        p = vect3_add(pos, VECT3(c.x, 0, c.y));
        glVertex3f(p.x, p.y, -p.z);
        c = vect2_rot(vect2_add(v, VECT2(-tr, tr)), hdg);
        p = vect3_add(pos, VECT3(c.x, 0, c.y));
        glVertex3f(p.x, p.y, -p.z);
        c = vect2_rot(vect2_add(v, VECT2(tr, tr)), hdg);
        p = vect3_add(pos, VECT3(c.x, 0, c.y));
        glVertex3f(p.x, p.y, -p.z);
        c = vect2_rot(vect2_add(v, VECT2(tr, -tr)), hdg);
        p = vect3_add(pos, VECT3(c.x, 0, c.y));
        glVertex3f(p.x, p.y, -p.z);
    }
    glEnd();
}

static int
draw_prediction(XPLMDrawingPhase phase, int before, void *refcon)
{
    seg_t *seg;
    XPLMProbeRef probe = XPLMCreateProbe(xplm_ProbeY);
    XPLMProbeInfo_t info = {.structSize = sizeof(XPLMProbeInfo_t)};
    mat4 view, proj;
    vec3 up = {sin(DEG2RAD(cam_hdg)), 0, -cos(DEG2RAD(cam_hdg))};
    vec3 fwd = {0, -1, 0};
    vec3 cam_posx = {dr_getf(&drs.cam_x), dr_getf(&drs.cam_y),
                     dr_getf(&drs.cam_z)};

    UNUSED(phase);
    UNUSED(before);
    UNUSED(refcon);

    VERIFY3S(dr_getvf32(&drs.proj_matrix_3d, (float *)proj, 0, 16), ==, 16);
    glm_look(cam_posx, fwd, up, view);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixf((float *)proj);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf((float *)view);

    if (dr_geti(&drs.view_is_ext) != 1)
        XPLMCommandOnce(circle_view_cmd);

    XPLMSetGraphicsState(0, 0, 0, 0, 0, 0, 0);

    for (seg = list_head(&bp.segs); seg != NULL;
         seg = list_next(&bp.segs, seg))
        draw_segment(seg);

    for (seg = list_head(&pred_segs); seg != NULL;
         seg = list_next(&pred_segs, seg))
        draw_segment(seg);

    if ((seg = list_tail(&pred_segs)) != NULL)
    {
        vect2_t dir_v = hdg2dir(seg->end_hdg);
        vect2_t x;

        // VERIFY3U(XPLMProbeTerrainXYZ(probe, seg->end_pos.x, 0,
        //                              -seg->end_pos.y, &info), ==, xplm_ProbeHitTerrain);

        if (XPLMProbeTerrainXYZ(probe, seg->end_pos.x, 0,
                                -seg->end_pos.y, &info))
        {
            XPLMDestroyProbe(probe);
            return (1);
        }

        if (seg->type == SEG_TYPE_TURN || !seg->backward)
        {
            glBegin(GL_LINES);
            glColor3f(0, 1, 0);
            glVertex3f(seg->end_pos.x, info.locationY,
                       -seg->end_pos.y);
            x = vect2_add(seg->end_pos, vect2_scmul(dir_v,
                                                    ORIENTATION_LINE_LEN));
            glVertex3f(x.x, info.locationY, -x.y);
            glEnd();
        }
        if (seg->type == SEG_TYPE_TURN || seg->backward)
        {
            glBegin(GL_LINES);
            glColor3f(1, 0, 0);
            glVertex3f(seg->end_pos.x, info.locationY,
                       -seg->end_pos.y);
            x = vect2_add(seg->end_pos, vect2_neg(vect2_scmul(
                                            dir_v, ORIENTATION_LINE_LEN)));
            glVertex3f(x.x, info.locationY, -x.y);
            glEnd();
        }
        draw_acf_symbol(VECT3(seg->end_pos.x, info.locationY,
                              seg->end_pos.y),
                        seg->end_hdg, AMBER_TUPLE);
    }
    else
    {
        // VERIFY3U(XPLMProbeTerrainXYZ(probe, cursor_world_pos.x, 0,
        //                              -cursor_world_pos.y, &info), ==, xplm_ProbeHitTerrain);
        if (XPLMProbeTerrainXYZ(probe, cursor_world_pos.x, 0,
                                -cursor_world_pos.y, &info))
        {
            XPLMDestroyProbe(probe);
            return (1);
        }
        draw_acf_symbol(VECT3(cursor_world_pos.x, info.locationY,
                              cursor_world_pos.y),
                        cursor_hdg, RED_TUPLE);
    }

    if ((seg = list_tail(&bp.segs)) != NULL)
    {
        // VERIFY3U(XPLMProbeTerrainXYZ(probe, seg->end_pos.x, 0,
        //                              -seg->end_pos.y, &info), ==, xplm_ProbeHitTerrain);
        if (XPLMProbeTerrainXYZ(probe, seg->end_pos.x, 0,
                                -seg->end_pos.y, &info))
        {
            XPLMDestroyProbe(probe);
            return (1);
        }
        draw_acf_symbol(VECT3(seg->end_pos.x, info.locationY,
                              seg->end_pos.y),
                        seg->end_hdg, GREEN_TUPLE);
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    return (1);
}

void draw_icon(button_t *btn, int x, int y, double scale, bool_t is_clicked,
               bool_t is_lit)
{
    glBindTexture(GL_TEXTURE_2D, btn->tex);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 1);
    glVertex2f(x, y);
    glTexCoord2f(0, 0);
    glVertex2f(x, y + btn->h * scale);
    glTexCoord2f(1, 0);
    glVertex2f(x + btn->w * scale, y + btn->h * scale);
    glTexCoord2f(1, 1);
    glVertex2f(x + btn->w * scale, y);
    glEnd();

    if (is_clicked || is_lit)
    {
        /*
         * If this button was hit by a mouse click, highlight
         * it by drawing a translucent white quad over it.
         */
        XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);
        glColor4f(1, 1, 1, 0.3);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x, y + btn->h * scale);
        glVertex2f(x + btn->w * scale, y + btn->h * scale);
        glVertex2f(x + btn->w * scale, y);
        glEnd();
        XPLMSetGraphicsState(0, 1, 0, 0, 1, 0, 0);
    }
    else if (is_lit)
    {
        XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);
        glColor4f(1, 1, 1, 1);
        glLineWidth(1);
        glBegin(GL_LINES);
        glVertex2f(x, y);
        glVertex2f(x, y + btn->h * scale);
        glVertex2f(x, y + btn->h * scale);
        glVertex2f(x + btn->w * scale, y + btn->h * scale);
        glVertex2f(x + btn->w * scale, y + btn->h * scale);
        glVertex2f(x + btn->w * scale, y);
        glVertex2f(x + btn->w * scale, y);
        glVertex2f(x, y);
        glEnd();
        XPLMSetGraphicsState(0, 1, 0, 0, 1, 0, 0);
    }
}

static void
draw_compass_rose(void)
{
    const float cx = monitor_def.x_origin + monitor_def.w/2 ; 
    const float cy = monitor_def.y_origin + monitor_def.h -
        COMPASS_ROSE_MARGIN - COMPASS_ROSE_RADIUS;
    const float r = COMPASS_ROSE_RADIUS;
    const float box_pad = 10;
    const double north_angle = DEG2RAD(-cam_hdg);
    const float north_x = sin(north_angle);
    const float north_y = cos(north_angle);
    const vect2_t north_dir = VECT2(north_x, north_y);
    const vect2_t right_dir = VECT2(north_y, -north_x);
    const vect2_t text_right_dir = VECT2(-right_dir.x, -right_dir.y);
    const vect2_t rose_center = VECT2(cx, cy);

    XPLMDrawTranslucentDarkBox(cx - r - box_pad, cy + r + box_pad,
                               cx + r + box_pad, cy - r - box_pad);
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);

    glColor4f(1, 1, 1, 0.9);
    glLineWidth(1);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < COMPASS_ROSE_SEGMENTS; i++)
    {
        double angle = DEG2RAD((360.0 * i) / COMPASS_ROSE_SEGMENTS);
        glVertex2f(cx + sin(angle) * r, cy + cos(angle) * r);
    }
    glEnd();

    glBegin(GL_LINES);
    for (int hdg = 0; hdg < 360; hdg += 45)
    {
        const bool_t cardinal = ((hdg % 90) == 0);
        const float tick_len = cardinal ? COMPASS_ROSE_TICK_LONG :
            COMPASS_ROSE_TICK_SHORT;
        const double angle = DEG2RAD(hdg - cam_hdg);
        const float dir_x = sin(angle);
        const float dir_y = cos(angle);

        glVertex2f(cx + dir_x * (r - tick_len), cy + dir_y * (r - tick_len));
        glVertex2f(cx + dir_x * r, cy + dir_y * r);
    }
    glEnd();

    glLineWidth(2);
    glColor4f(0.95, 0.85, 0.1, 1);
    glBegin(GL_LINES);
    glVertex2f(cx, cy);
    glVertex2f(cx + north_x * (r - COMPASS_ROSE_ARROW_DEPTH),
               cy + north_y * (r - COMPASS_ROSE_ARROW_DEPTH));
    glEnd();

    {
        const vect2_t arrow_tip = vect2_add(rose_center,
            vect2_scmul(north_dir, r));
        const vect2_t arrow_base = vect2_add(rose_center,
            vect2_scmul(north_dir, r - COMPASS_ROSE_ARROW_DEPTH));
        const vect2_t arrow_base_l = vect2_add(arrow_base,
            vect2_scmul(right_dir, -COMPASS_ROSE_ARROW_HEAD));
        const vect2_t arrow_base_r = vect2_add(arrow_base,
            vect2_scmul(right_dir, COMPASS_ROSE_ARROW_HEAD));

        glBegin(GL_TRIANGLES);
        glVertex2f(arrow_tip.x, arrow_tip.y);
        glVertex2f(arrow_base_l.x, arrow_base_l.y);
        glVertex2f(arrow_base_r.x, arrow_base_r.y);
        glEnd();
    }

    glBegin(GL_LINES);
    glVertex2f(cx + north_x * r, cy + north_y * r);
    glVertex2f(cx + north_x * (r - COMPASS_ROSE_ARROW_DEPTH) +
               right_dir.x * COMPASS_ROSE_ARROW_HEAD,
               cy + north_y * (r - COMPASS_ROSE_ARROW_DEPTH) +
               right_dir.y * COMPASS_ROSE_ARROW_HEAD);
    glVertex2f(cx + north_x * r, cy + north_y * r);
    glVertex2f(cx + north_x * (r - COMPASS_ROSE_ARROW_DEPTH) -
               right_dir.x * COMPASS_ROSE_ARROW_HEAD,
               cy + north_y * (r - COMPASS_ROSE_ARROW_DEPTH) -
               right_dir.y * COMPASS_ROSE_ARROW_HEAD);
    glEnd();

    /*
     * Draw a tiny "N" aligned with the north arrow so the overlay reads as
     * a compass rose rather than just a heading indicator.
     */
    {
        const vect2_t label_center = vect2_add(rose_center,
            vect2_scmul(north_dir, r + 12));
        const float half_h = 5;
        const float half_w = 3;
        const vect2_t base_l = vect2_add(label_center,
            vect2_add(vect2_scmul(north_dir, -half_h),
                      vect2_scmul(text_right_dir, -half_w)));
        const vect2_t top_l = vect2_add(label_center,
            vect2_add(vect2_scmul(north_dir, half_h),
                      vect2_scmul(text_right_dir, -half_w)));
        const vect2_t base_r = vect2_add(label_center,
            vect2_add(vect2_scmul(north_dir, -half_h),
                      vect2_scmul(text_right_dir, half_w)));
        const vect2_t top_r = vect2_add(label_center,
            vect2_add(vect2_scmul(north_dir, half_h),
                      vect2_scmul(text_right_dir, half_w)));

        glBegin(GL_LINES);
        glVertex2f(base_l.x, base_l.y);
        glVertex2f(top_l.x, top_l.y);
        glVertex2f(base_l.x, base_l.y);
        glVertex2f(top_r.x, top_r.y);
        glVertex2f(base_r.x, base_r.y);
        glVertex2f(top_r.x, top_r.y);
        glEnd();
    }
}

static char planner_route_message[256];

static void
planner_gate_route_state_reset(void)
{
    memset(&planner_gate_routes, 0, sizeof(planner_gate_routes));
    planner_gate_routes.loaded_slot = -1;
    planner_gate_routes.save_slot = -1;
    planner_gate_routes.hover_choice = -1;
    planner_gate_routes.mouse_down_choice = -1;
}

static int
planner_first_empty_route_slot(void)
{
    bool occupied[GATE_ROUTE_CACHE_SLOT_COUNT];

    for (unsigned slot = 0; slot < GATE_ROUTE_CACHE_SLOT_COUNT; slot++) {
        occupied[slot] = planner_gate_routes.slots[slot].valid != B_FALSE;
    }
    return gate_route_first_empty_slot(occupied,
        GATE_ROUTE_CACHE_SLOT_COUNT);
}

static unsigned
planner_route_prompt_option_count(void)
{
    if (planner_gate_routes.prompt == PLANNER_ROUTE_PROMPT_SELECT)
        return 3;
    if (planner_gate_routes.prompt == PLANNER_ROUTE_PROMPT_REPLACE)
        return 4;
    return 0;
}

static planner_route_prompt_rect_t
planner_route_prompt_panel_rect(void)
{
    unsigned options = planner_route_prompt_option_count();
    int width = MIN(ROUTE_PROMPT_WIDTH, monitor_def.w - 80);
    int height = ROUTE_PROMPT_HEADER_HEIGHT + ROUTE_PROMPT_SIDE_PADDING +
        (int)options * ROUTE_PROMPT_OPTION_HEIGHT +
        (int)(options - 1) * ROUTE_PROMPT_OPTION_GAP;
    int center_x = monitor_def.x_origin + monitor_def.w / 2;
    int center_y = monitor_def.y_origin + monitor_def.h / 2;

    return (planner_route_prompt_rect_t){
        .left = center_x - width / 2,
        .bottom = center_y - height / 2,
        .right = center_x + width / 2,
        .top = center_y + height / 2
    };
}

static planner_route_prompt_rect_t
planner_route_prompt_option_rect(unsigned choice)
{
    planner_route_prompt_rect_t panel = planner_route_prompt_panel_rect();
    int top = panel.top - ROUTE_PROMPT_HEADER_HEIGHT -
        (int)choice * (ROUTE_PROMPT_OPTION_HEIGHT + ROUTE_PROMPT_OPTION_GAP);

    return (planner_route_prompt_rect_t){
        .left = panel.left + ROUTE_PROMPT_SIDE_PADDING,
        .bottom = top - ROUTE_PROMPT_OPTION_HEIGHT,
        .right = panel.right - ROUTE_PROMPT_SIDE_PADDING,
        .top = top
    };
}

static bool_t
planner_route_prompt_choice_enabled(unsigned choice)
{
    if (planner_gate_routes.prompt == PLANNER_ROUTE_PROMPT_SELECT &&
        choice < GATE_ROUTE_CACHE_SLOT_COUNT) {
        return planner_gate_routes.slots[choice].valid;
    }
    return (choice < planner_route_prompt_option_count());
}

static int
planner_route_prompt_hit_check(int x, int y)
{
    for (unsigned choice = 0; choice < planner_route_prompt_option_count();
         choice++) {
        planner_route_prompt_rect_t rect =
            planner_route_prompt_option_rect(choice);

        if (planner_route_prompt_choice_enabled(choice) && x >= rect.left &&
            x <= rect.right && y >= rect.bottom && y <= rect.top) {
            return (int)choice;
        }
    }
    return -1;
}

static void
planner_route_prompt_option_text(unsigned choice, char *text,
    size_t capacity)
{
    ASSERT(text != NULL);
    ASSERT(capacity != 0);
    text[0] = '\0';
    if (planner_gate_routes.prompt == PLANNER_ROUTE_PROMPT_SELECT) {
        if (choice < GATE_ROUTE_CACHE_SLOT_COUNT) {
            const gate_route_slot_info_t *slot =
                &planner_gate_routes.slots[choice];

            if (slot->valid) {
                (void)snprintf(text, capacity,
                    "%u  Use Route %u - Tail %s - aircraft heading %03.0f deg",
                    choice + 1, choice + 1, slot->tail_direction,
                    slot->final_aircraft_hdg);
            } else {
                (void)snprintf(text, capacity, "Route %u - not saved",
                    choice + 1);
            }
        } else {
            (void)snprintf(text, capacity, "N  Plan a new manual route");
        }
    } else if (planner_gate_routes.prompt == PLANNER_ROUTE_PROMPT_REPLACE) {
        if (choice < GATE_ROUTE_CACHE_SLOT_COUNT) {
            const gate_route_slot_info_t *slot =
                &planner_gate_routes.slots[choice];

            (void)snprintf(text, capacity,
                "%u  Replace Route %u - Tail %s - aircraft heading %03.0f deg",
                choice + 1, choice + 1, slot->tail_direction,
                slot->final_aircraft_hdg);
        } else if (choice == GATE_ROUTE_CACHE_SLOT_COUNT) {
            (void)snprintf(text, capacity, "U  Use once without saving");
        } else {
            (void)snprintf(text, capacity, "B  Back to the planner");
        }
    }
}

static void
draw_planner_route_prompt(void)
{
    planner_route_prompt_rect_t panel;
    float title_color[3] = {1.0f, 1.0f, 1.0f};
    float text_color[3] = {0.88f, 0.90f, 0.94f};
    char title[160];
    char instruction[200];

    if (planner_gate_routes.prompt == PLANNER_ROUTE_PROMPT_NONE)
        return;
    panel = planner_route_prompt_panel_rect();
    XPLMDrawTranslucentDarkBox(panel.left, panel.top, panel.right,
        panel.bottom);
    if (planner_gate_routes.prompt == PLANNER_ROUTE_PROMPT_SELECT) {
        (void)snprintf(title, sizeof(title), "Saved routes for %s %s",
            planner_gate_context.airport, planner_gate_context.ramp);
        (void)snprintf(instruction, sizeof(instruction),
            "Choose a saved route or begin a new manual plan.");
    } else {
        (void)snprintf(title, sizeof(title),
            "Both saved-route slots are occupied");
        (void)snprintf(instruction, sizeof(instruction),
            "Choose which route to replace, use this plan once, or go back.");
    }
    XPLMDrawString(title_color, panel.left + ROUTE_PROMPT_SIDE_PADDING,
        panel.top - 30, title, NULL, xplmFont_Proportional);
    XPLMDrawString(text_color, panel.left + ROUTE_PROMPT_SIDE_PADDING,
        panel.top - 56, instruction, NULL, xplmFont_Proportional);

    for (unsigned choice = 0; choice < planner_route_prompt_option_count();
         choice++) {
        planner_route_prompt_rect_t rect =
            planner_route_prompt_option_rect(choice);
        bool_t enabled = planner_route_prompt_choice_enabled(choice);
        char label[192];
        float label_color[3] = {enabled ? 1.0f : 0.52f,
            enabled ? 1.0f : 0.54f, enabled ? 1.0f : 0.58f};

        XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);
        glColor4f(choice == (unsigned)planner_gate_routes.hover_choice ?
            0.18f : 0.08f, choice ==
            (unsigned)planner_gate_routes.hover_choice ? 0.48f : 0.20f,
            choice == (unsigned)planner_gate_routes.hover_choice ?
            0.62f : 0.28f, enabled ? 0.92f : 0.55f);
        glBegin(GL_QUADS);
        glVertex2i(rect.left, rect.bottom);
        glVertex2i(rect.right, rect.bottom);
        glVertex2i(rect.right, rect.top);
        glVertex2i(rect.left, rect.top);
        glEnd();
        planner_route_prompt_option_text(choice, label, sizeof(label));
        XPLMDrawString(label_color, rect.left + 16, rect.bottom + 16, label,
            NULL, xplmFont_Proportional);
    }
}

static void
planner_route_show_message(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    (void)vsnprintf(planner_route_message, sizeof(planner_route_message),
        format, args);
    va_end(args);
    init_bottom_msg(planner_route_message);
}

static void
planner_route_select_saved(unsigned slot)
{
    gate_route_slot_info_t info;

    ASSERT(slot < GATE_ROUTE_CACHE_SLOT_COUNT);
    bp_delete_all_segs();
    planner_clear_predicted_segments();
    planner_prediction_key_invalidate(&planner_pred_key);
    if (!gate_route_cache_load(&planner_gate_context, slot, &bp.segs, &info)) {
        planner_gate_routes.slots[slot].valid = B_FALSE;
        planner_gate_routes.slot_count = gate_route_cache_list(
            &planner_gate_context, planner_gate_routes.slots);
        planner_gate_routes.prompt = planner_gate_routes.slot_count != 0 ?
            PLANNER_ROUTE_PROMPT_SELECT : PLANNER_ROUTE_PROMPT_NONE;
        planner_gate_routes.save_slot = planner_first_empty_route_slot();
        planner_gate_routes.new_route = B_TRUE;
        logMsg(BP_WARN_LOG "Gate route slot %u could not be loaded for %s "
            "%s; planner remains in manual mode", slot + 1,
            planner_gate_context.airport, planner_gate_context.ramp);
        return;
    }
    planner_gate_routes.slots[slot] = info;
    planner_gate_routes.loaded_slot = (int)slot;
    planner_gate_routes.save_slot = (int)slot;
    planner_gate_routes.dirty = B_FALSE;
    planner_gate_routes.new_route = B_FALSE;
    planner_gate_routes.suppress_save = B_FALSE;
    planner_gate_routes.prompt = PLANNER_ROUTE_PROMPT_NONE;
    planner_route_show_message("Loaded Route %u - Tail %s - aircraft heading "
        "%03.0f deg", slot + 1, info.tail_direction,
        info.final_aircraft_hdg);
    logMsg(BP_INFO_LOG "Gate route slot %u recalled for %s %s; Tail %s, "
        "final aircraft heading %.2f degrees", slot + 1,
        planner_gate_context.airport, planner_gate_context.ramp,
        info.tail_direction, info.final_aircraft_hdg);
}

static void
planner_route_begin_new(void)
{
    bp_delete_all_segs();
    planner_clear_predicted_segments();
    planner_prediction_key_invalidate(&planner_pred_key);
    planner_gate_routes.loaded_slot = -1;
    planner_gate_routes.save_slot = planner_first_empty_route_slot();
    planner_gate_routes.dirty = B_FALSE;
    planner_gate_routes.new_route = B_TRUE;
    planner_gate_routes.suppress_save = B_FALSE;
    planner_gate_routes.prompt = PLANNER_ROUTE_PROMPT_NONE;
    if (planner_gate_routes.save_slot >= 0) {
        planner_route_show_message("New manual route will be saved as Route "
            "%u", planner_gate_routes.save_slot + 1);
    } else {
        planner_route_show_message("Plan a new manual route; Enter will ask "
            "which saved route to replace");
    }
    logMsg(BP_INFO_LOG "New manual route selected for %s %s; save slot %d",
        planner_gate_context.airport, planner_gate_context.ramp,
        planner_gate_routes.save_slot + 1);
}

static void
planner_route_accept_and_close(void)
{
    gate_route_save_policy_t policy;

    if (planner_gate_routes.prompt == PLANNER_ROUTE_PROMPT_SELECT)
        return;
    policy = gate_route_slot_save_policy(
        planner_gate_context.recognized != B_FALSE,
        list_head(&bp.segs) != NULL,
        planner_gate_routes.suppress_save != B_FALSE,
        planner_gate_routes.loaded_slot, planner_gate_routes.save_slot,
        planner_gate_routes.dirty != B_FALSE,
        planner_gate_routes.new_route != B_FALSE);
    if (policy == GATE_ROUTE_SAVE_NEEDS_REPLACEMENT) {
        planner_gate_routes.prompt = PLANNER_ROUTE_PROMPT_REPLACE;
        planner_gate_routes.hover_choice = -1;
        planner_gate_routes.mouse_down_choice = -1;
        return;
    }
    XPLMCommandOnce(XPLMFindCommand("BetterPushback/stop_planner"));
}

static void
planner_route_prompt_choose(unsigned choice)
{
    if (!planner_route_prompt_choice_enabled(choice))
        return;
    if (planner_gate_routes.prompt == PLANNER_ROUTE_PROMPT_SELECT) {
        if (choice < GATE_ROUTE_CACHE_SLOT_COUNT)
            planner_route_select_saved(choice);
        else
            planner_route_begin_new();
        return;
    }
    if (planner_gate_routes.prompt != PLANNER_ROUTE_PROMPT_REPLACE)
        return;
    if (choice < GATE_ROUTE_CACHE_SLOT_COUNT) {
        planner_gate_routes.save_slot = (int)choice;
        planner_gate_routes.suppress_save = B_FALSE;
        planner_gate_routes.prompt = PLANNER_ROUTE_PROMPT_NONE;
        planner_route_accept_and_close();
    } else if (choice == GATE_ROUTE_CACHE_SLOT_COUNT) {
        planner_gate_routes.save_slot = -1;
        planner_gate_routes.suppress_save = B_TRUE;
        planner_gate_routes.prompt = PLANNER_ROUTE_PROMPT_NONE;
        planner_route_accept_and_close();
    } else {
        planner_gate_routes.prompt = PLANNER_ROUTE_PROMPT_NONE;
        planner_gate_routes.hover_choice = -1;
        planner_gate_routes.mouse_down_choice = -1;
    }
}

static void
fake_win_draw(XPLMWindowID inWindowID, void *inRefcon)
{
    double scale;
    int w, h, h_buttons, h_off;
    UNUSED(inWindowID);
    UNUSED(inRefcon);

    h = monitor_def.h;
    w = monitor_def.w;

if (bp_plan_callback_is_alive == 0) {
    // if camera is lost we simulate ESC button
    logMsg(BP_INFO_LOG "VK_ESCAPE simulated fake_win_draw");
    bp_plan_callback_is_alive = CAMERA_IS_OFF;
    key_sniffer(0, xplm_DownFlag, XPLM_VK_ESCAPE, NULL);
    return;
}


    if (!XPLMIsWindowInFront(fake_win))
        XPLMBringWindowToFront(fake_win);
    if (force_root_win_focus)
        XPLMTakeKeyboardFocus(0);

    XPLMSetGraphicsState(0, 1, 0, 0, 1, 0, 0);

    h_buttons = 0;
    for (int i = 0; buttons[i].filename != NULL; i++)
        h_buttons += buttons[i].h;

    scale = (double)h / h_buttons;
    scale = MIN(scale, 1);
    /* don't draw the buttons if we don't have enough space for them */
    if (scale < MIN_BUTTON_SCALE) {
        draw_planner_route_prompt();
        return;
    }

    h_off = (h + (h_buttons * scale)) / 2;
    for (int i = 0; buttons[i].filename != NULL;
         i++, h_off -= buttons[i].h * scale)
    {
        button_t *btn = &buttons[i];

        if (btn->tex == 0)
            continue;

        draw_icon(btn, monitor_def.x_origin + w - btn->w * scale, monitor_def.y_origin + h_off - btn->h * scale,
                  scale, i == button_hit, i == button_lit);
    }

    if (bottom_msg.msg != NULL)
    {
        draw_bottom_msg(w, h);
    }

    if (planner_gate_routes.prompt != PLANNER_ROUTE_PROMPT_SELECT)
        draw_prediction(0, 0, NULL);
    draw_compass_rose();
    draw_planner_route_prompt();
}

static int
button_hit_check(int x, int y)
{
    double scale;
    int w, h, h_buttons, h_off;

    h = monitor_def.h;
    w = monitor_def.w;

    h_buttons = 0;
    for (int i = 0; buttons[i].filename != NULL; i++)
        h_buttons += buttons[i].h;

    scale = (double)h / h_buttons;
    scale = MIN(scale, 1);
    if (scale < MIN_BUTTON_SCALE)
        return (-1);

    h_off = (h + (h_buttons * scale)) / 2;
    for (int i = 0; buttons[i].filename != NULL;
         i++, h_off -= buttons[i].h * scale)
    {
        if (x >= monitor_def.x_origin + w - buttons[i].w * scale && x <= monitor_def.x_origin + w &&
            y >= monitor_def.y_origin + h_off - buttons[i].h * scale && y <= monitor_def.y_origin + h_off &&
            buttons[i].vk != -1)
            return (i);
    }

    return (-1);
}

void nil_win_key(XPLMWindowID inWindowID, char inKey, XPLMKeyFlags inFlags,
                 char inVirtualKey, void *inRefcon, int losingFocus)
{
    UNUSED(inWindowID);
    UNUSED(inKey);
    UNUSED(inFlags);
    UNUSED(inVirtualKey);
    UNUSED(inRefcon);
    UNUSED(losingFocus);
}

static XPLMCursorStatus
fake_win_cursor(XPLMWindowID inWindowID, int x, int y, void *inRefcon)
{
    int lit;

    UNUSED(inWindowID);
    UNUSED(inRefcon);

    if (planner_gate_routes.prompt != PLANNER_ROUTE_PROMPT_NONE) {
        planner_gate_routes.hover_choice =
            planner_route_prompt_hit_check(x, y);
        button_lit = -1;
        return (xplm_CursorDefault);
    }

    if ((lit = button_hit_check(x, y)) != -1 && buttons[lit].vk != -1)
        button_lit = lit;
    else
        button_lit = -1;

    return (xplm_CursorDefault);
}

static int
fake_win_click(XPLMWindowID inWindowID, int x, int y, XPLMMouseStatus inMouse,
               void *inRefcon)
{
    static int last_x = 0, last_y = 0;
    static int down_x = 0, down_y = 0;
    static bool_t dragging = B_FALSE;

    UNUSED(inWindowID);
    UNUSED(inRefcon);

    if (planner_gate_routes.prompt != PLANNER_ROUTE_PROMPT_NONE) {
        int choice = planner_route_prompt_hit_check(x, y);

        if (inMouse == xplm_MouseDown) {
            planner_gate_routes.mouse_down_choice = choice;
            force_root_win_focus = B_FALSE;
        } else if (inMouse == xplm_MouseDrag) {
            planner_gate_routes.hover_choice = choice;
        } else {
            if (choice != -1 &&
                choice == planner_gate_routes.mouse_down_choice) {
                planner_route_prompt_choose((unsigned)choice);
            }
            planner_gate_routes.mouse_down_choice = -1;
            force_root_win_focus = B_TRUE;
        }
        return (1);
    }

    /*
     * The mouse handling logic is as follows:
     * 1) On mouse-down, we memorize where the click started, check if
     *	we hit a button and reset the dragging variable.
     * 2) While receiving dragging events, we check if the displacement
     *	from the original start position is sufficient to consider
     *	this click-and-drag. If it is, we set the `dragging' flag
     *	to true. This inhibits clicking.
     * 3) On mouse-up, if there was no click-and-drag, check if the
     *	button we're on is still the same as what we hit on the
     *	initial mouse-down (prevents from clicking one button and
     *	sliding onto another one). If no button was hit and no
     *	click-and-drag took place, count it as a click on the screen
     *	attempting to place a segment.
     */
    if (inMouse == xplm_MouseDown)
    {
        last_x = down_x = x;
        last_y = down_y = y;
        force_root_win_focus = B_FALSE;
        button_hit = button_hit_check(x, y);
        dragging = B_FALSE;
    }
    else if (inMouse == xplm_MouseDrag)
    {
        if (!dragging)
        {
            dragging = (ABS(x - down_x) >= CLICK_DISPL_THRESH ||
                        ABS(y - down_y) >= CLICK_DISPL_THRESH);
        }
        if (dragging && (x != last_x || y != last_y) &&
            button_hit == -1)
        {
            double x_phys, last_x_phys, y_phys, last_y_phys, dx, dy;
            vect2_t v;

            vp_unproject(x, y, &x_phys, &y_phys);
            vp_unproject(last_x, last_y,
                         &last_x_phys, &last_y_phys);
            dx = x_phys - last_x_phys;
            dy = y_phys - last_y_phys;

            v = vect2_rot(VECT2(dx, dy), cam_hdg);
            cam_pos.x -= v.x;
            cam_pos.z -= v.y;
            last_x = x;
            last_y = y;
        }
    }
    else
    {
        if (!dragging)
        {
            if (button_hit != -1 &&
                button_hit == button_hit_check(x, y))
            {
                /* simulate a key press */
                ASSERT(buttons[button_hit].vk != -1);
                key_sniffer(0, xplm_DownFlag,
                            buttons[button_hit].vk, NULL);
            }
            else
            {
                /*
                 * Transfer whatever is in pred_segs to
                 * the normal segments and clear pred_segs.
                 */
                if (list_head(&pred_segs) != NULL)
                    planner_gate_routes.dirty = B_TRUE;
                list_move_tail(&bp.segs, &pred_segs);
            }
        }
        button_hit = -1;
        force_root_win_focus = B_TRUE;
    }

    return (1);
}

static int
fake_win_wheel(XPLMWindowID inWindowID, int x, int y, int wheel, int clicks,
               void *inRefcon)
{
    UNUSED(inWindowID);
    UNUSED(x);
    UNUSED(y);
    UNUSED(wheel);
    UNUSED(clicks);
    UNUSED(inRefcon);

    if (planner_gate_routes.prompt != PLANNER_ROUTE_PROMPT_NONE)
        return (1);

    if (wheel == 0 && clicks != 0)
    {
        static int accel = 1;
        static uint64_t last_wheel_t = 0;
        uint64_t now = microclock();
        uint64_t us_per_click = (now - last_wheel_t) / ABS(clicks);

        if (us_per_click < US_PER_CLICK_ACCEL)
            accel = MIN(accel + 1, MAX_ACCEL_MULT);
        else if (us_per_click > US_PER_CLICK_DEACCEL)
            accel = 1;

        cursor_hdg = normalize_hdg(cursor_hdg +
                                   clicks * accel * WHEEL_ANGLE_MULT);
        last_wheel_t = now;
    }

    return (0);
}

static int
key_sniffer(char inChar, XPLMKeyFlags inFlags, char inVirtualKey, void *refcon)
{
    UNUSED(refcon);

    /* Only allow the plain key to be pressed, no modifiers */
    if (inFlags != xplm_DownFlag)
        return (1);

    if (planner_gate_routes.prompt != PLANNER_ROUTE_PROMPT_NONE) {
        unsigned char key = (unsigned char)toupper((unsigned char)inChar);

        if (key == '1') {
            planner_route_prompt_choose(0);
            return (0);
        }
        if (key == '2') {
            planner_route_prompt_choose(1);
            return (0);
        }
        if (planner_gate_routes.prompt == PLANNER_ROUTE_PROMPT_SELECT &&
            key == 'N') {
            planner_route_prompt_choose(GATE_ROUTE_CACHE_SLOT_COUNT);
            return (0);
        }
        if (planner_gate_routes.prompt == PLANNER_ROUTE_PROMPT_REPLACE) {
            if (key == 'U') {
                planner_route_prompt_choose(GATE_ROUTE_CACHE_SLOT_COUNT);
                return (0);
            }
            if (key == 'B' || inVirtualKey == XPLM_VK_ESCAPE) {
                planner_route_prompt_choose(GATE_ROUTE_CACHE_SLOT_COUNT + 1);
                return (0);
            }
        }
        if (inVirtualKey != XPLM_VK_ESCAPE)
            return (0);
    }

    switch ((unsigned char)inVirtualKey)
    {
    case XPLM_VK_RETURN:
    case XPLM_VK_ENTER:
    case XPLM_VK_NUMPAD_ENT:
        planner_route_accept_and_close();
        return (0);
    case XPLM_VK_ESCAPE:
        bp_delete_all_segs();
        XPLMCommandOnce(XPLMFindCommand("BetterPushback/stop_planner"));
        return (0);
    case XPLM_VK_CLEAR:
    case XPLM_VK_BACK:
    case XPLM_VK_DELETE:
        /* Delete the segments up to the next user-placed segment */
        if (list_tail(&bp.segs) != NULL)
            planner_gate_routes.dirty = B_TRUE;
        free(list_remove_tail(&bp.segs));
        for (seg_t *seg = list_tail(&bp.segs); seg != NULL &&
                                               !seg->user_placed;
             seg = list_tail(&bp.segs))
        {
            list_remove_tail(&bp.segs);
            free(seg);
        }
        return (0);
    case XPLM_VK_SPACE:
        bp_delete_all_segs();
        XPLMCommandOnce(XPLMFindCommand(
            "BetterPushback/connect_first"));
        return (0);
    }

    return (1);
}

static void
find_drs(void)
{
    fdr_find(&drs.local_vx, "sim/flightmodel/position/local_vx");
    fdr_find(&drs.local_vy, "sim/flightmodel/position/local_vy");
    fdr_find(&drs.local_vz, "sim/flightmodel/position/local_vz");
    fdr_find(&drs.local_x, "sim/flightmodel/position/local_x");
    fdr_find(&drs.local_y, "sim/flightmodel/position/local_y");
    fdr_find(&drs.local_z, "sim/flightmodel/position/local_z");
    fdr_find(&drs.hdg, "sim/flightmodel/position/psi");
    fdr_find(&drs.tire_z, "sim/flightmodel/parts/tire_z_no_deflection");
    fdr_find(&drs.tire_x, "sim/flightmodel/parts/tire_x_no_deflection");
    if (bp_xp_ver >= 12100)
    {
        fdr_find(&drs.tirrad, "sim/flightmodel2/gear/tire_radius_mtrs");
    }
    else
    {
        fdr_find(&drs.tirrad, "sim/aircraft/parts/acf_gear_tirrad");
    }
    fdr_find(&drs.mtow, "sim/aircraft/weight/acf_m_max");

    fdr_find(&drs.view_is_ext, "sim/graphics/view/view_is_external");
    /*
     * These datarefs have been removed in XP12 with no replacements in
     * sight. Disabling for now to get rid of errors/crashes with them.
     */
    if (bp_xp_ver < 12000)
    {
        fdr_find(&drs.visibility, "sim/weather/visibility_reported_m");
        fdr_find(&drs.cloud_types[0], "sim/weather/cloud_type[0]");
        fdr_find(&drs.cloud_types[1], "sim/weather/cloud_type[1]");
        fdr_find(&drs.cloud_types[2], "sim/weather/cloud_type[2]");
        fdr_find(&drs.use_real_wx, "sim/weather/use_real_weather_bool");
    }

    if ((bp_xp_ver >= 12000) && (bp_xp_ver < 13000))
    {
        fdr_find(&drs.visibility, "sim/weather/region/visibility_reported_sm");
        fdr_find(&drs.cloud_types[0], "sim/weather/region/cloud_type");
        fdr_find(&drs.use_real_wx, "sim/weather/region/change_mode");
    }

    fdr_find(&drs.cam_x, "sim/graphics/view/view_x");
    fdr_find(&drs.cam_y, "sim/graphics/view/view_y");
    fdr_find(&drs.cam_z, "sim/graphics/view/view_z");
    fdr_find(&drs.proj_matrix_3d, "sim/graphics/view/projection_matrix_3d");
    fdr_find(&drs.viewport, "sim/graphics/view/viewport");
}

float loop_nightlamp(float elapsed, float elapsed2, int counter, void *refcon)
{
    UNUSED(elapsed);
    UNUSED(elapsed2);
    UNUSED(counter);
    UNUSED(refcon);

    XPLMDrawInfo_t di;
    XPLMProbeInfo_t info = {.structSize = sizeof(XPLMProbeInfo_t)};
    XPLMProbeRef probe = XPLMCreateProbe(xplm_ProbeY);

     /* Draw the night-lighting lamp so the user can see under the cursor */
    if (XPLMProbeTerrainXYZ(probe, cursor_world_pos.x, 0,
                            -cursor_world_pos.y, &info))
    {
        XPLMDestroyProbe(probe);
        return (1);
    }

    di.structSize = sizeof(di);
    di.x = cursor_world_pos.x;
    di.y = info.locationY;
    di.z = -cursor_world_pos.y;
    di.heading = 0;
    di.pitch = 0;
    di.roll = 0;
    ASSERT(cam_lamp_inst != NULL);
    XPLMInstanceSetPosition(cam_lamp_inst, &di, NULL);
    XPLMDestroyProbe(probe);
    return (-1);
}

bool_t
bp_cam_start(void)
{
    XPLMCreateWindow_t fake_win_ops = {
        .structSize = sizeof(XPLMCreateWindow_t),
        .left = 0,
        .top = 0,
        .right = 0,
        .bottom = 0,
        .visible = 1,
        .drawWindowFunc = fake_win_draw,
        .handleMouseClickFunc = fake_win_click,
        .handleKeyFunc = nil_win_key,
        .handleCursorFunc = fake_win_cursor,
        .handleMouseWheelFunc = fake_win_wheel,
        .refcon = NULL};

    XPLMCreateFlightLoop_t floop_nightlamp = {
        .structSize = sizeof(XPLMCreateFlightLoop_t),
        .phase = xplm_FlightLoop_Phase_BeforeFlightModel,
        .callbackFunc = loop_nightlamp,
        .refcon = NULL
    };

    

    char icao[8] = {0};
    char *cam_obj_path;
    char airline[1024] = {0};
    char update_message[256] = {0};
    char *updateAvailable;
    double gate_match_distance = NAN, gate_match_heading = NAN;
    bool_t at_published_start;

    if (cam_inited || !bp_init())
        return (B_FALSE);

    find_drs();

    cam_obj_path = mkpathname(bp_xpdir, bp_plugindir, "objects",
                              "night_lamp.obj", NULL);
    cam_lamp_obj = XPLMLoadObject(cam_obj_path);
    if (cam_lamp_obj == NULL)
    {
        logMsg(BP_ERROR_LOG "Error loading pushback lamp %s. Please reinstall "
                            "BetterPushback.",
               cam_obj_path);
        free(cam_obj_path);
        return (B_FALSE);
    }
    free(cam_obj_path);

    if (!acf_is_compatible())
    {
        XPLMSpeakString(_("Pushback failure: aircraft is incompatible "
                          "with BetterPushback."));
        return (B_FALSE);
    }


    (void)find_nearest_airport(icao);
    if (acf_is_airliner())
        read_acf_airline(airline);
    if (!tug_available(dr_getf(&drs.mtow), bp.acf.nw_len, bp.acf.tirrad,
                       bp.acf.nw_type, strcmp(icao, "") != 0 ? icao : NULL, airline))
    {
        XPLMSpeakString(_("Pushback failure: no suitable tug for your "
                          "aircraft."));
        return (B_FALSE);
    }

#ifndef PB_DEBUG_INTF
    if (vect3_abs(VECT3(dr_getf(&drs.local_vx), dr_getf(&drs.local_vy),
                        dr_getf(&drs.local_vz))) > 0.1)
    {
        XPLMSpeakString(_("Can't start planner: aircraft not "
                          "stationary."));
        return (B_FALSE);
    }
    if (bp_started && !late_plan_requested)
    {
        XPLMSpeakString(_("Can't start planner: pushback already in "
                          "progress. Please stop the pushback operation first."));
        return (B_FALSE);
    }
#endif /* !PB_DEBUG_INTF */

    push_reset_fov_values();
    eye_track_debut();

    initMonitorOrigin();
    fake_win_ops.left = monitor_def.x_origin;
    fake_win_ops.right = monitor_def.x_origin + monitor_def.w;
    fake_win_ops.bottom = monitor_def.y_origin;
    fake_win_ops.top = monitor_def.y_origin + monitor_def.h;

    circle_view_cmd = XPLMFindCommand("sim/view/circle");
    ASSERT(circle_view_cmd != NULL);
    XPLMCommandOnce(circle_view_cmd);

    fake_win = XPLMCreateWindowEx(&fake_win_ops);
    ASSERT(fake_win != NULL);
    XPLMBringWindowToFront(fake_win);
    XPLMTakeKeyboardFocus(fake_win);

    list_create(&pred_segs, sizeof(seg_t), offsetof(seg_t, node));
    planner_prediction_key_init(&planner_pred_key);
    planner_cursor_solve_count = 0;
    planner_cursor_reuse_count = 0;
    planner_gate_route_state_reset();
    logMsg(BP_INFO_LOG "Legacy planner trajectory initialized");
    force_root_win_focus = B_TRUE;
    cam_height = 15 * bp.veh.wheelbase;
    /* We keep the camera position in our coordinates for ease of manip */
    cam_pos = VECT3(dr_getf(&drs.local_x),
                    dr_getf(&drs.local_y), -dr_getf(&drs.local_z));
    cam_hdg = dr_getf(&drs.hdg);
    cursor_hdg = dr_getf(&drs.hdg);
    XPLMControlCamera(xplm_ControlCameraForever, cam_ctl, NULL);

    cam_lamp_inst = XPLMCreateInstance(cam_lamp_obj, cam_lamp_drefs);

    for (int i = 0; view_cmds[i].name != NULL; i++)
    {
        view_cmds[i].cmd = XPLMFindCommand(view_cmds[i].name);
        VERIFY(view_cmds[i].cmd != NULL);
        XPLMRegisterCommandHandler(view_cmds[i].cmd, move_camera,
                                   1, (void *)(uintptr_t)i);
    }
    XPLMRegisterKeySniffer(key_sniffer, 1, NULL);

    if (!emergency_tow_allows_persistent_routes()) {
        memset(&planner_gate_context, 0, sizeof(planner_gate_context));
        bp_delete_all_segs();
        planner_gate_routes.new_route = B_TRUE;
        planner_gate_routes.suppress_save = B_TRUE;
        planner_gate_routes.loaded_slot = -1;
        planner_gate_routes.save_slot = -1;
        planner_route_show_message("Emergency Tow - manual route only; "
            "saved routes are disabled");
        logMsg(BP_INFO_LOG "Emergency Tow planner opened at the live "
            "nosewheel; saved-route listing, loading, and saving are "
            "disabled");
    } else {
        at_published_start = planner_prepare_gate_context(
            &gate_match_distance, &gate_match_heading);
        if (list_head(&bp.segs) == NULL) {
            if (at_published_start) {
                planner_gate_routes.slot_count = gate_route_cache_list(
                    &planner_gate_context, planner_gate_routes.slots);
                if (planner_gate_routes.slot_count != 0) {
                    planner_gate_routes.prompt = PLANNER_ROUTE_PROMPT_SELECT;
                    logMsg(BP_INFO_LOG "Published start recognized: %s %s "
                        "(nosewheel match %.2f m, %.2f degrees); %u "
                        "compatible saved route slot%s available for pilot "
                        "selection", planner_gate_context.airport,
                        planner_gate_context.ramp, gate_match_distance,
                        gate_match_heading, planner_gate_routes.slot_count,
                        planner_gate_routes.slot_count == 1 ? "" : "s");
                } else {
                    planner_gate_routes.save_slot = 0;
                    planner_gate_routes.new_route = B_TRUE;
                    logMsg(BP_INFO_LOG "Published start recognized: %s %s "
                        "(nosewheel match %.2f m, %.2f degrees); no "
                        "compatible saved route slots, planner opened for "
                        "manual placement", planner_gate_context.airport,
                        planner_gate_context.ramp, gate_match_distance,
                        gate_match_heading);
                }
            } else {
                planner_gate_routes.new_route = B_TRUE;
                planner_gate_routes.suppress_save = B_TRUE;
                logMsg(BP_INFO_LOG "No unique published apt.dat start "
                    "matched; planner begins at the live nosewheel and this "
                    "route will not be saved persistently");
            }
        } else if (at_published_start) {
            planner_gate_routes.slot_count = gate_route_cache_list(
                &planner_gate_context, planner_gate_routes.slots);
            planner_gate_routes.suppress_save = B_TRUE;
            logMsg(BP_INFO_LOG "Planner retained the pilot's current "
                "in-session route at published start %s %s; no saved slot "
                "will be changed", planner_gate_context.airport,
                planner_gate_context.ramp);
        } else {
            planner_gate_routes.suppress_save = B_TRUE;
            logMsg(BP_INFO_LOG "Planner retained the pilot's current "
                "in-session route; current position is not a unique "
                "published apt.dat start, so persistent saving is disabled");
        }
    }

    /*
     * While the planner is active, we override the current
     * visibility and real weather usage, so that the user can
     * clearly see the path while planning. After we're done,
     * we'll restore the settings.
     */
    saved_visibility = dr_getf(&drs.visibility);
    saved_real_wx = dr_geti(&drs.use_real_wx);

    if (bp_xp_ver < 12000)
    {
        saved_cloud_types[0] = dr_geti(&drs.cloud_types[0]);
        saved_cloud_types[1] = dr_geti(&drs.cloud_types[1]);
        saved_cloud_types[2] = dr_geti(&drs.cloud_types[2]);
        dr_setf(&drs.visibility, BP_PLANNER_VISIBILITY);
        dr_seti(&drs.cloud_types[0], 0);
        dr_seti(&drs.cloud_types[1], 0);
        dr_seti(&drs.cloud_types[2], 0);
        dr_seti(&drs.use_real_wx, 0);
    }
    if ((bp_xp_ver >= 12000) && (bp_xp_ver < 13000))
    {
        float zero_cloud[3] = {0.0};
        dr_getvf32(&drs.cloud_types[0], fsaved_cloud_types, 0, 3);
        dr_setvf32(&drs.cloud_types[0], zero_cloud, 0, 3);
        dr_setf(&drs.visibility, BP_PLANNER_VISIBILITY_SM);
        dr_seti(&drs.use_real_wx, 3);
    }

    cam_inited = B_TRUE;
    planner_open = B_TRUE; /* BP_DATAREF planner_open */
    if (!slave_mode)
        plan_complete = B_FALSE; /* BP_DATAREF plan_complete */

    updateAvailable = getPluginUpdateStatus();
    if (updateAvailable != NULL)
    {
        snprintf(update_message, sizeof(update_message), "New version of BetterPushBack available: %s (Use SkunkCrafts Updater to update)", updateAvailable);
        init_bottom_msg(update_message);
    }

    if (bp_floop_nightlamp == NULL)
        bp_floop_nightlamp = XPLMCreateFlightLoop(&floop_nightlamp);
    XPLMScheduleFlightLoop(bp_floop_nightlamp, -1, 1);

    return (B_TRUE);
}

bool_t
bp_cam_stop(void)
{
    XPLMCommandRef cockpit_view_cmd;

    if (!cam_inited)
        return (B_FALSE);

    if (bp_floop_nightlamp != NULL) {
        XPLMDestroyFlightLoop(bp_floop_nightlamp);
        bp_floop_nightlamp = NULL;
    }

    if (cam_lamp_inst != NULL)
        XPLMDestroyInstance(cam_lamp_inst);
    cam_lamp_inst = NULL;
    if (cam_lamp_obj != NULL)
        XPLMUnloadObject(cam_lamp_obj);
    cam_lamp_obj = NULL;

    logMsg(BP_INFO_LOG "Legacy planner summary: cursor solves %llu, "
        "cursor reuse %llu",
        (unsigned long long)planner_cursor_solve_count,
        (unsigned long long)planner_cursor_reuse_count);
    if (!slave_mode && list_head(&bp.segs) != NULL) {
        if (!emergency_tow_allows_persistent_routes()) {
            logMsg(BP_INFO_LOG "Emergency Tow route accepted for this "
                "session only; hard persistence guard skipped every gate "
                "route cache write");
        } else if (!planner_gate_context.recognized) {
            logMsg(BP_INFO_LOG "Route remains available for this pushback "
                "session but was not saved: aircraft did not start at a "
                "unique published apt.dat location");
        } else if (planner_gate_routes.suppress_save) {
            logMsg(BP_INFO_LOG "Route accepted for this pushback without "
                "changing either saved route slot for %s %s",
                planner_gate_context.airport, planner_gate_context.ramp);
        } else if (planner_gate_routes.loaded_slot >= 0 &&
            !planner_gate_routes.dirty) {
            logMsg(BP_INFO_LOG "Gate route slot %u accepted unchanged for "
                "%s %s; cache file was not rewritten",
                planner_gate_routes.loaded_slot + 1,
                planner_gate_context.airport, planner_gate_context.ramp);
        } else if (planner_gate_routes.save_slot >= 0) {
            gate_route_slot_info_t saved;

            if (gate_route_cache_save(&planner_gate_context,
                (unsigned)planner_gate_routes.save_slot, &bp.segs, &saved)) {
                planner_gate_routes.slots[saved.slot] = saved;
                logMsg(BP_INFO_LOG "Gate route slot %u saved for %s %s; "
                    "Tail %s, final aircraft heading %.2f degrees, anchor "
                    "%.8f, %.8f at %.2f degrees",
                    saved.slot + 1, planner_gate_context.airport,
                    planner_gate_context.ramp, saved.tail_direction,
                    saved.final_aircraft_hdg,
                    planner_gate_context.anchor_geo.lat,
                    planner_gate_context.anchor_geo.lon,
                    planner_gate_context.anchor_hdg);
            } else {
                logMsg(BP_WARN_LOG "Gate route slot %u was not saved for "
                    "%s %s: route start or slot file validation failed",
                    planner_gate_routes.save_slot + 1,
                    planner_gate_context.airport,
                    planner_gate_context.ramp);
            }
        } else {
            logMsg(BP_INFO_LOG "Route accepted for this pushback without "
                "changing either saved route slot for %s %s because no "
                "replacement slot was selected",
                planner_gate_context.airport, planner_gate_context.ramp);
        }
    }
    planner_clear_predicted_segments();
    planner_prediction_key_invalidate(&planner_pred_key);
    list_destroy(&pred_segs);

    XPLMDestroyWindow(fake_win);

    for (int i = 0; view_cmds[i].name != NULL; i++)
    {
        XPLMUnregisterCommandHandler(view_cmds[i].cmd, move_camera,
                                     1, (void *)(uintptr_t)i);
    }
    XPLMUnregisterKeySniffer(key_sniffer, 1, NULL);

    cockpit_view_cmd = XPLMFindCommand("sim/view/3d_cockpit_cmnd_look");
    ASSERT(cockpit_view_cmd != NULL);
    XPLMCommandOnce(cockpit_view_cmd);

    dr_setf(&drs.visibility, saved_visibility);
    if (bp_xp_ver < 12000)
    {
        dr_seti(&drs.cloud_types[0], saved_cloud_types[0]);
        dr_seti(&drs.cloud_types[1], saved_cloud_types[1]);
        dr_seti(&drs.cloud_types[2], saved_cloud_types[2]);
    }
    if ((bp_xp_ver >= 12000) && (bp_xp_ver < 13000))
    {
        dr_setvf32(&drs.cloud_types[0], fsaved_cloud_types, 0, 3);
    }
    dr_seti(&drs.use_real_wx, saved_real_wx);
    cam_inited = B_FALSE;
    planner_open = B_FALSE; /* BP_DATAREF planner_open */

    pop_fov_values();
    eye_track_fini();
    clear_bottom_msg();

    if (!slave_mode)
        plan_complete = (list_head(&bp.segs) != NULL); /* BP_DATAREF plan_complete */

    return (B_TRUE);
}

bool_t
bp_cam_is_running(void)
{
    return (cam_inited);
}

void init_bottom_msg(char *msg)
{
    FT_Error err;
    char *filename;
    int tex_w, tex_h;
    uint8_t *tex_bytes;

    if ((err = FT_Init_FreeType(&ft)) != 0)
    {
        logMsg(BP_ERROR_LOG "Error initializing FreeType library: %s",
               ft_err2str(err));
        return;
    }

    filename = mkpathname(bp_plugindir, "data", "fonts", BOTTOM_MSG_FONT,
                          NULL);
    if ((err = FT_New_Face(ft, filename, 0, &face)) != 0)
    {
        logMsg(BP_ERROR_LOG "Error loading init_msg font %s: %s", filename,
               ft_err2str(err));
        VERIFY(FT_Done_FreeType(ft) == 0);
        free(filename);
        return;
    }
    free(filename);

    clear_bottom_msg();

    if (strlen(msg) == 0)
    {
        return;
    }
    if (!get_text_block_size(msg, face, BOTTOM_MSG_FONT_SIZE,
                             &bottom_msg.width, &bottom_msg.height))
    {
        return;
    }

    tex_w = bottom_msg.width + 2 * MARGIN_SIZE;
    tex_h = bottom_msg.height + 2 * MARGIN_SIZE;
    tex_bytes = calloc(tex_w * tex_h * 4, 1);

    /* fill with a black, semi-transparent background */
    for (int i = 0; i < tex_w * tex_h; i++)
        tex_bytes[i * 4 + 3] = (uint8_t)(255 * 0.67);

    if (!render_text_block(msg, face, BOTTOM_MSG_FONT_SIZE,
                           MARGIN_SIZE, MARGIN_SIZE + BOTTOM_MSG_FONT_SIZE,
                           255, 255, 255, tex_bytes, tex_w, tex_h))
    {
        free(tex_bytes);
        return;
    }

    bottom_msg.msg = msg;
    bottom_msg.bytes = tex_bytes;

    glGenTextures(1, &bottom_msg.texture);
    glBindTexture(GL_TEXTURE_2D, bottom_msg.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_w, tex_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, bottom_msg.bytes);
}

void clear_bottom_msg(void)
{
    if (bottom_msg.msg != NULL)
    {
        glDeleteTextures(1, &bottom_msg.texture);
        free(bottom_msg.bytes);
        memset(&bottom_msg, 0, sizeof(bottom_msg));
    }
}

void draw_bottom_msg(int screen_x, int screen_y)
{

    UNUSED(screen_y);

    glBindTexture(GL_TEXTURE_2D, bottom_msg.texture);
    glBegin(GL_QUADS);
    glTexCoord2f(monitor_def.x_origin, monitor_def.y_origin + 1.0);
    glVertex2f(monitor_def.x_origin + (screen_x - bottom_msg.width) / 2 - MARGIN_SIZE, monitor_def.y_origin);
    glTexCoord2f(monitor_def.x_origin, monitor_def.y_origin);
    glVertex2f(monitor_def.x_origin + (screen_x - bottom_msg.width) / 2 - MARGIN_SIZE,
               monitor_def.y_origin + bottom_msg.height + 2 * MARGIN_SIZE);
    glTexCoord2f(monitor_def.x_origin + 1, monitor_def.y_origin);
    glVertex2f(monitor_def.x_origin + (screen_x + bottom_msg.width) / 2 + MARGIN_SIZE,
               monitor_def.y_origin + bottom_msg.height + 2 * MARGIN_SIZE);
    glTexCoord2f(monitor_def.x_origin + 1, monitor_def.y_origin + 1);
    glVertex2f(monitor_def.x_origin + (screen_x + bottom_msg.width) / 2 + MARGIN_SIZE, monitor_def.y_origin);
    glEnd();
}

void eye_track_debut(void)
{

    const char *plg_to_exclude = NULL;

    eye_tracker_plg.plg_id = -1;
    if (conf_get_str(bp_conf, "plg_to_exclude", &plg_to_exclude))
    {
        eye_tracker_plg.plg_id = XPLMFindPluginBySignature(plg_to_exclude);
    }
    else
    {
        logMsg(BP_INFO_LOG "XPLMDisablePlugin not done, no plugin to exclude selected");
        return;
    }

    if (eye_tracker_plg.plg_id != -1)
    {
        eye_tracker_plg.plg_status = XPLMIsPluginEnabled(eye_tracker_plg.plg_id);
        if (eye_tracker_plg.plg_status)
        {
            XPLMDisablePlugin(eye_tracker_plg.plg_id);
            logMsg(BP_INFO_LOG "XPLMDisablePlugin on %s", plg_to_exclude);
        }
        else
        {
            logMsg(BP_INFO_LOG "XPLMDisablePlugin not done, was already disabled");
        }
    }
    else
    {
        logMsg(BP_INFO_LOG "XPLMDisablePlugin not done, plugin %s not found", plg_to_exclude);
    }
}

void eye_track_fini(void)
{
    if (eye_tracker_plg.plg_id != -1)
    {
        eye_tracker_plg.plg_status = XPLMIsPluginEnabled(eye_tracker_plg.plg_id);
        if (!eye_tracker_plg.plg_status)
        {
            int r = XPLMEnablePlugin(eye_tracker_plg.plg_id);
            logMsg(BP_INFO_LOG "XPLMEnablePlugin %d", r);
        }
        else
        {
            logMsg(BP_INFO_LOG "XPLMEnablePlugin not done, was already enabled");
        }
    }
}
