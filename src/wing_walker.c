/*
 * CDDL HEADER START
 *
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source. A copy is also available in the repository's COPYING file.
 *
 * CDDL HEADER END
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <XPLMGraphics.h>
#include <XPLMInstance.h>
#include <XPLMScenery.h>

#include <acfutils/assert.h>
#include <acfutils/geom.h>
#include <acfutils/math.h>
#include <acfutils/safe_alloc.h>

#include "wing_walker.h"
#include "wing_walker_logic.h"
#include "xplane.h"

#define WING_WALKER_NOSE_CLEARANCE 27.432 /* 30 yards */
#define WING_WALKER_CAPTAIN_OFFSET 1.5
#define WING_WALKER_GROUND_CLEARANCE 0.02

struct wing_walker {
    XPLMObjectRef object;
    XPLMInstanceRef instance;
    XPLMProbeRef probe;
    char *object_path;
    bool_t load_in_progress;
    bool_t destroyed;
    bool_t terrain_warning_shown;
    bool_t signal_initialized;
    wing_walker_signal_t last_signal;
};

static const char *wing_walker_datarefs[] = {
    WING_WALKER_SIGNAL_DATAREF,
    NULL
};

static const char *
wing_walker_signal_name(wing_walker_signal_t signal)
{
    switch (signal) {
    case WING_WALKER_SIGNAL_HIDDEN:
        return ("hidden");
    case WING_WALKER_SIGNAL_STOP:
        return ("stop");
    case WING_WALKER_SIGNAL_STANDBY:
        return ("standby");
    case WING_WALKER_SIGNAL_CLEAR:
        return ("clear");
    }
    return ("unknown");
}

static void
wing_walker_destroy_storage(wing_walker_t *walker)
{
    ASSERT(walker != NULL);
    if (walker->probe != NULL)
        XPLMDestroyProbe(walker->probe);
    free(walker->object_path);
    free(walker);
}

static void
wing_walker_load_complete(XPLMObjectRef object, void *refcon)
{
    wing_walker_t *walker = refcon;

    ASSERT(walker != NULL);
    walker->load_in_progress = B_FALSE;
    walker->object = object;

    if (walker->destroyed) {
        if (object != NULL)
            XPLMUnloadObject(object);
        wing_walker_destroy_storage(walker);
        return;
    }

    if (object == NULL) {
        logMsg(BP_WARN_LOG "Wing walker object could not be loaded from %s; "
            "pushback will continue without the worker",
            walker->object_path);
        return;
    }

    walker->instance = XPLMCreateInstance(object, wing_walker_datarefs);
    if (walker->instance == NULL) {
        logMsg(BP_WARN_LOG "Wing walker instance could not be created; "
            "pushback will continue without the worker");
    }
}

wing_walker_t *
wing_walker_alloc(const char *object_path)
{
    wing_walker_t *walker;

    ASSERT(object_path != NULL);
    walker = safe_calloc(1, sizeof(*walker));
    walker->object_path = strdup(object_path);
    walker->probe = XPLMCreateProbe(xplm_ProbeY);
    if (walker->probe == NULL) {
        logMsg(BP_WARN_LOG "Wing walker terrain probe could not be created; "
            "pushback will continue without the worker");
        wing_walker_destroy_storage(walker);
        return (NULL);
    }

    walker->load_in_progress = B_TRUE;
    XPLMLoadObjectAsync(walker->object_path, wing_walker_load_complete,
        walker);
    return (walker);
}

void
wing_walker_free(wing_walker_t *walker)
{
    if (walker == NULL)
        return;

    if (walker->instance != NULL) {
        XPLMDestroyInstance(walker->instance);
        walker->instance = NULL;
    }
    if (walker->object != NULL) {
        XPLMUnloadObject(walker->object);
        walker->object = NULL;
    }
    if (walker->load_in_progress) {
        walker->destroyed = B_TRUE;
        return;
    }
    wing_walker_destroy_storage(walker);
}

void
wing_walker_update(wing_walker_t *walker, vect2_t aircraft_pos,
    double aircraft_heading, double aircraft_nose_forward,
    pushback_step_t step, bool_t reconnecting)
{
    XPLMDrawInfo_t draw_info = {.structSize = sizeof(draw_info)};
    XPLMProbeInfo_t probe_info = {.structSize = sizeof(probe_info)};
    wing_walker_signal_t signal;
    vect2_t aircraft_direction, captain_left, walker_pos;
    vect3_t normal, normal_heading, grounded_pos;
    float instance_data[1];
    double walker_heading;

    if (walker == NULL || walker->instance == NULL)
        return;

    signal = wing_walker_signal_for_step(step, reconnecting != B_FALSE);
    if (!walker->signal_initialized || signal != walker->last_signal) {
        logMsg(BP_INFO_LOG "Wing walker signal changed to %s (step %d)",
            wing_walker_signal_name(signal), step);
        walker->last_signal = signal;
        walker->signal_initialized = B_TRUE;
    }
    aircraft_direction = hdg2dir(aircraft_heading);
    captain_left = vect2_norm(aircraft_direction, B_FALSE);
    walker_pos = vect2_add(aircraft_pos, vect2_scmul(aircraft_direction,
        aircraft_nose_forward + WING_WALKER_NOSE_CLEARANCE));
    walker_pos = vect2_add(walker_pos, vect2_scmul(captain_left,
        WING_WALKER_CAPTAIN_OFFSET));

    if (XPLMProbeTerrainXYZ(walker->probe, walker_pos.x, 0,
        -walker_pos.y, &probe_info) != xplm_ProbeHitTerrain) {
        if (!walker->terrain_warning_shown) {
            logMsg(BP_WARN_LOG "Wing walker terrain probe failed; hiding "
                "the worker until terrain becomes available");
            walker->terrain_warning_shown = B_TRUE;
        }
        instance_data[0] = WING_WALKER_SIGNAL_HIDDEN;
        XPLMInstanceSetPosition(walker->instance, &draw_info, instance_data);
        return;
    }
    walker->terrain_warning_shown = B_FALSE;

    normal = VECT3(probe_info.normalX, probe_info.normalY,
        probe_info.normalZ);
    grounded_pos = VECT3(walker_pos.x, probe_info.locationY, -walker_pos.y);
    grounded_pos = vect3_add(grounded_pos, vect3_set_abs(normal,
        WING_WALKER_GROUND_CLEARANCE));
    walker_heading = normalize_hdg(aircraft_heading + 180);
    normal_heading = vect3_rot(VECT3(probe_info.normalX,
        probe_info.normalY, -probe_info.normalZ), walker_heading, 1);

    draw_info.x = grounded_pos.x;
    draw_info.y = grounded_pos.y;
    draw_info.z = grounded_pos.z;
    draw_info.heading = walker_heading;
    draw_info.pitch = -RAD2DEG(atan(normal_heading.z / normal_heading.y));
    draw_info.roll = RAD2DEG(atan(normal_heading.x / normal_heading.y));
    instance_data[0] = (float)signal;
    XPLMInstanceSetPosition(walker->instance, &draw_info, instance_data);
}
