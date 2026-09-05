/*
 * Pure read-only mapping from BetterPushback controller state to a fixed-size
 * Ground Operations UI snapshot.
 */

#ifndef _GROUND_OPS_STATE_H_
#define _GROUND_OPS_STATE_H_

#include <stdbool.h>
#include <stdint.h>

#include "pushback_step.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GROUND_OPS_STAGE_COUNT 5
#define GROUND_OPS_STATUS_LEN 64
#define GROUND_OPS_DETAIL_LEN 96
#define GROUND_OPS_TASK_LEN 64
#define GROUND_OPS_METRIC_LEN 32
#define GROUND_OPS_CAPTION_LEN 96
#define GROUND_OPS_SOURCE_LEN 32
#define GROUND_OPS_HOVER_LEN 160
#define GROUND_OPS_ACTION_LABEL_LEN 32
#define GROUND_OPS_AIRPORT_LEN 24
#define GROUND_OPS_FLIGHT_LEN 24
#define GROUND_OPS_SCHEDULE_LEN 20
#define GROUND_OPS_WEATHER_LEN 48
#define GROUND_OPS_PRESSURE_LEN 40
#define GROUND_OPS_ADVISORY_LEN 48

typedef enum {
    GROUND_OPS_STAGE_TUG,
    GROUND_OPS_STAGE_CONNECT,
    GROUND_OPS_STAGE_COMMS,
    GROUND_OPS_STAGE_PUSH,
    GROUND_OPS_STAGE_CLEAR
} ground_ops_stage_t;

typedef enum {
    GROUND_OPS_STAGE_FUTURE,
    GROUND_OPS_STAGE_CURRENT,
    GROUND_OPS_STAGE_COMPLETE
} ground_ops_stage_progress_t;

typedef enum {
    GROUND_OPS_PREP_AIRPORT_DATA,
    GROUND_OPS_PREP_PLANNER_REVIEW,
    GROUND_OPS_PREP_COMPLETE
} ground_ops_prep_state_t;

typedef enum {
    GROUND_OPS_CAPTION_NONE,
    GROUND_OPS_CAPTION_PLAN_START,
    GROUND_OPS_CAPTION_PLAN_END,
    GROUND_OPS_CAPTION_DRIVING_UP,
    GROUND_OPS_CAPTION_READY_TO_CONNECT,
    GROUND_OPS_CAPTION_READY_TO_CONNECT_NOPARK,
    GROUND_OPS_CAPTION_WINCH,
    GROUND_OPS_CAPTION_CONNECTED,
    GROUND_OPS_CAPTION_START_PUSHBACK,
    GROUND_OPS_CAPTION_START_TOW,
    GROUND_OPS_CAPTION_START_PUSHBACK_NOSTART,
    GROUND_OPS_CAPTION_START_TOW_NOSTART,
    GROUND_OPS_CAPTION_OPERATION_COMPLETE,
    GROUND_OPS_CAPTION_DISCONNECT,
    GROUND_OPS_CAPTION_DONE_RIGHT,
    GROUND_OPS_CAPTION_DONE_LEFT,
    GROUND_OPS_CAPTION_COUNT
} ground_ops_caption_t;

typedef enum {
    GROUND_OPS_ACTION_NONE,
    GROUND_OPS_ACTION_CALL_TUG,
    GROUND_OPS_ACTION_CALL_EMERGENCY_TOW,
    GROUND_OPS_ACTION_OPEN_PLANNER,
    GROUND_OPS_ACTION_CHANGE_PLAN,
    GROUND_OPS_ACTION_PAUSE,
    GROUND_OPS_ACTION_RESUME,
    GROUND_OPS_ACTION_END_DISCONNECT
} ground_ops_action_t;

typedef struct {
    bool operation_active;
    bool emergency_tow;
    pushback_step_t step;
    ground_ops_prep_state_t prep_state;
    bool prep_state_active;
    bool tug_staged;
    bool late_plan;
    bool plan_complete;
    bool planner_open;
    bool awaiting_plan;
    bool replan_available;
    bool pause_requested;
    bool pause_held;
    char airport_ident[8];
    char flight[GROUND_OPS_FLIGHT_LEN];
    char schedule[GROUND_OPS_SCHEDULE_LEN];
    char weather[GROUND_OPS_WEATHER_LEN];
    char pressure[GROUND_OPS_PRESSURE_LEN];
    char advisory[GROUND_OPS_ADVISORY_LEN];
    char data_source[GROUND_OPS_SOURCE_LEN];
    bool captions_enabled;
    bool caption_active;
    ground_ops_caption_t caption;
    uint64_t caption_sequence;
    bool speed_valid;
    int speed_tenths_mps;
    bool distance_valid;
    int distance_m;
} ground_ops_raw_state_t;

typedef struct {
    uint64_t revision;
    uint64_t transition_sequence;
    bool operation_active;
    bool emergency_tow;
    bool operation_complete;
    bool prep_state_active;
    bool action_required;
    bool caption_visible;
    ground_ops_action_t primary_action;
    ground_ops_action_t secondary_action;
    pushback_step_t controller_step;
    ground_ops_prep_state_t prep_state;
    ground_ops_stage_t stage;
    ground_ops_stage_progress_t stages[GROUND_OPS_STAGE_COUNT];
    char stage_name[16];
    char eyebrow[16];
    char status[GROUND_OPS_STATUS_LEN];
    char detail[GROUND_OPS_DETAIL_LEN];
    char current_task[GROUND_OPS_TASK_LEN];
    char speed[GROUND_OPS_METRIC_LEN];
    char distance[GROUND_OPS_METRIC_LEN];
    char caption[GROUND_OPS_CAPTION_LEN];
    char airport[GROUND_OPS_AIRPORT_LEN];
    char flight[GROUND_OPS_FLIGHT_LEN];
    char schedule[GROUND_OPS_SCHEDULE_LEN];
    char weather[GROUND_OPS_WEATHER_LEN];
    char pressure[GROUND_OPS_PRESSURE_LEN];
    char advisory[GROUND_OPS_ADVISORY_LEN];
    char source[GROUND_OPS_SOURCE_LEN];
    char hover[GROUND_OPS_HOVER_LEN];
    char primary_action_label[GROUND_OPS_ACTION_LABEL_LEN];
    char secondary_action_label[GROUND_OPS_ACTION_LABEL_LEN];
} ground_ops_snapshot_t;

typedef struct {
    bool initialized;
    ground_ops_raw_state_t previous_raw;
    ground_ops_snapshot_t snapshot;
} ground_ops_state_t;

void ground_ops_state_init(ground_ops_state_t *state);
bool ground_ops_state_update(ground_ops_state_t *state,
    const ground_ops_raw_state_t *raw);
const ground_ops_snapshot_t *ground_ops_state_get(
    const ground_ops_state_t *state);
const char *ground_ops_stage_name(ground_ops_stage_t stage);
const char *ground_ops_step_name(pushback_step_t step);
const char *ground_ops_prep_name(ground_ops_prep_state_t prep);

#ifdef __cplusplus
}
#endif

#endif /* _GROUND_OPS_STATE_H_ */
