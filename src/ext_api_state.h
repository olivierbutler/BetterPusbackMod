/*
 * External interface: the operation state published to other plugins through
 * datarefs (README-EXTERNAL-API.md). Pure mapping from the Ground Operations
 * snapshot to stable public numbers and names; no XPLM dependencies.
 */

#ifndef _EXT_API_STATE_H_
#define _EXT_API_STATE_H_

#include <stdbool.h>
#include <stdint.h>

#include "ground_ops_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Raised whenever the published interface gains something (never lowered). */
#define BP_EXT_API_VERSION 3

#define BP_EXT_NAME_LEN 32

/*
 * Public step numbers. They are part of the interface: a value never changes
 * meaning, and new steps get new numbers at the end. Today they equal the
 * controller's pushback_step_t order; the mapping table in ext_api_state.c
 * keeps them stable if that enum is ever reordered.
 */
typedef enum {
    BP_EXT_STEP_OFF = 0,
    BP_EXT_STEP_TUG_LOAD = 1,
    BP_EXT_STEP_START = 2,
    BP_EXT_STEP_DRIVING_UP_CLOSE = 3,
    BP_EXT_STEP_WAITING_FOR_DOORS = 4,
    BP_EXT_STEP_OPENING_CRADLE = 5,
    BP_EXT_STEP_WAITING_FOR_PBRAKE = 6,
    BP_EXT_STEP_DRIVING_UP_CONNECT = 7,
    BP_EXT_STEP_GRABBING = 8,
    BP_EXT_STEP_LIFTING = 9,
    BP_EXT_STEP_CONNECTED = 10,
    BP_EXT_STEP_STARTING = 11,
    BP_EXT_STEP_PUSHING = 12,
    BP_EXT_STEP_STOPPING = 13,
    BP_EXT_STEP_STOPPED = 14,
    BP_EXT_STEP_LOWERING = 15,
    BP_EXT_STEP_UNGRABBING = 16,
    BP_EXT_STEP_WAITING_TO_DISCONNECT = 17,
    BP_EXT_STEP_MOVING_AWAY = 18,
    BP_EXT_STEP_CLOSING_CRADLE = 19,
    BP_EXT_STEP_STARTING_TO_CLEAR = 20,
    BP_EXT_STEP_MOVING_TO_CLEAR = 21,
    BP_EXT_STEP_CLEAR_SIGNAL = 22,
    BP_EXT_STEP_DRIVING_AWAY = 23
} bp_ext_step_t;

/* Public action numbers: what the pilot must do next (0 = nothing). */
typedef enum {
    BP_EXT_ACTION_NONE = 0,
    BP_EXT_ACTION_CALL_TUG = 1,
    BP_EXT_ACTION_CALL_EMERGENCY_TOW = 2,
    BP_EXT_ACTION_OPEN_PLANNER = 3,
    BP_EXT_ACTION_CHANGE_PLAN = 4,
    BP_EXT_ACTION_PAUSE = 5,
    BP_EXT_ACTION_RESUME = 6,
    BP_EXT_ACTION_DISCONNECT_TUG = 7,
    BP_EXT_ACTION_RECONNECT_TUG = 8,
    BP_EXT_ACTION_END_DISCONNECT = 9,
    BP_EXT_ACTION_ACKNOWLEDGE_CLEAR = 10
} bp_ext_action_t;

/* Public blocker numbers: what the operation waits for (0 = nothing). */
typedef enum {
    BP_EXT_BLOCKER_NONE = 0,
    BP_EXT_BLOCKER_AIRCRAFT_NOT_READY = 1,      /* door open, GPU/ASU on */
    BP_EXT_BLOCKER_SET_PARKING_BRAKE = 2,
    BP_EXT_BLOCKER_RELEASE_PARKING_BRAKE = 3,
    BP_EXT_BLOCKER_PLAN_REQUIRED = 4
} bp_ext_blocker_t;

#define BP_EXT_ITEM_LEN 64
#define BP_EXT_STATUS_LEN 96

/* The controller's blocker, as the glue code reads it from bp.c. */
typedef struct {
    int blocker;                    /* bp_ext_blocker_t */
    const char *item;               /* the dataref still open, or NULL */
    const char *status;             /* the status line, or NULL */
} bp_ext_blocker_in_t;

/* Everything published by this part of the interface. */
typedef struct {
    int active;                     /* an operation is running */
    int emergency_tow;              /* ... and it is the Emergency Tow */
    int step;                       /* bp_ext_step_t */
    char step_name[BP_EXT_NAME_LEN];
    int stage;                      /* 0..4 Tug..Clear, -1 when idle */
    char stage_name[BP_EXT_NAME_LEN];
    int action;                     /* bp_ext_action_t */
    char action_name[BP_EXT_NAME_LEN];
    int paused;                     /* a pause is requested or held */
    int blocker;                    /* bp_ext_blocker_t */
    char blocker_name[BP_EXT_NAME_LEN];
    char blocker_item[BP_EXT_ITEM_LEN];
    char blocker_item_kind[BP_EXT_NAME_LEN];
    char status[BP_EXT_STATUS_LEN];
    int state_seq;                  /* changes whenever any field above does */
} bp_ext_state_t;

int bp_ext_step_public(pushback_step_t step);
const char *bp_ext_step_name(int public_step);
int bp_ext_action_public(ground_ops_action_t action);
const char *bp_ext_action_name(int public_action);
const char *bp_ext_stage_name(int stage);
const char *bp_ext_blocker_name(int public_blocker);
/*
 * What a door-check dataref stands for, from its name: "gpu", "asu",
 * "cargo_door", "door", or "other" ("" for none).
 */
const char *bp_ext_item_kind(const char *dataref);

/*
 * Fills `out` from the controller's raw state, the Ground Operations snapshot
 * made from it, and its blocker (NULL: none). `previous` is the state
 * published before (or NULL): state_seq is carried over, plus one when
 * anything changed.
 */
void bp_ext_state_from(const ground_ops_raw_state_t *raw,
    const ground_ops_snapshot_t *snapshot, const bp_ext_blocker_in_t *blocker,
    const bp_ext_state_t *previous, bp_ext_state_t *out);

#ifdef __cplusplus
}
#endif

#endif /* _EXT_API_STATE_H_ */
