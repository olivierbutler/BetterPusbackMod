#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "fast_brake_handoff.h"
#include "handling_timing.h"
#include "pushback_step.h"

typedef bool bool_t;
typedef int XPLMCommandRef;
typedef int XPLMCommandPhase;
typedef struct { double value; } test_dr_t;

#define B_TRUE true
#define B_FALSE false
#define ASSERT(x) assert(x)
#define VERIFY_FAIL() assert(false)
#define UNUSED(x) ((void)(x))
#define _(x) (x)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define STATE_TRANS_DELAY 2.0
#define PB_CONN_LIFT_DURATION 9.0
#define PB_CRADLE_DELAY 10.0
#define MSG_OP_COMPLETE 0
#define MSG_DISCO 1
#define LIFT_GRAB 0
#define LIFT_WINCH 1

static struct {
    double cur_t, step_start_t, last_voice_t;
    pushback_step_t step;
    bool_t reconnect, ok2disco;
    bp_fast_brake_handoff_t fast_brake_handoff;
    struct { double nw_len; int nw_i; } acf;
    struct { double max_accel; } veh;
} bp;
static struct {
    test_dr_t pbrake, pbrake_rat, lbrake, rbrake, leg_len;
    bool pbrake_is_custom;
} drs;
typedef struct { int lift_type; double lift_height; } test_tug_info_t;
typedef struct { test_tug_info_t *info; } test_tug_t;
static test_tug_info_t tug_info;
static test_tug_t tug;
static struct { test_tug_t *tug; } bp_ls = { &tug };

static bool slave_mode, cfg_ignore_park_break, fast;
static bool pb_set_override, pb_set_remote, op_complete;
static bool coupled_wheel_brake;
static unsigned brake_writes, reconnect_notifications;
static const char *bp_hint_status_str;

static double
dr_getf(const test_dr_t *dr)
{
    if (coupled_wheel_brake && dr == &drs.pbrake_rat)
        return MAX(dr->value, drs.lbrake.value);
    return dr->value;
}

static void
dr_setf(test_dr_t *dr, double value)
{
    dr->value = value;
    if (dr == &drs.lbrake || dr == &drs.rbrake)
        brake_writes++;
}

static void dr_setvf(test_dr_t *dr, double *value, int offset, int count)
{ UNUSED(dr); UNUSED(value); UNUSED(offset); UNUSED(count); }
static bool_t bp_fast_ground_handling(void) { return fast; }
static double artificial_delay(double value)
{ return bp_handling_duration(value, fast); }
static double handling_fraction(double elapsed, double duration)
{ return bp_handling_fraction(elapsed, duration, fast); }
static double msg_dur(int msg) { UNUSED(msg); return 4; }
static void msg_play(int msg) { UNUSED(msg); }
static void turn_nosewheel(double value) { UNUSED(value); }
static void push_at_speed(double speed, double accel, bool_t a, bool_t b)
{ UNUSED(speed); UNUSED(accel); UNUSED(a); UNUSED(b); }
static double tug_plat_h(void *value) { UNUSED(value); return 0; }
static void tug_set_lift_in_transit(bool_t value) { UNUSED(value); }
static void tug_set_lift_pos(double value) { UNUSED(value); }
static void tug_set_cradle_air_on(void *value, bool_t on, double now)
{ UNUSED(value); UNUSED(on); UNUSED(now); }
static void tug_set_cradle_beeper_on(void *value, bool_t on)
{ UNUSED(value); UNUSED(on); }
static void tug_set_TE_override(void *value, bool_t on)
{ UNUSED(value); UNUSED(on); }
static void tug_set_lift_arm_pos(void *value, double position, bool_t on)
{ UNUSED(value); UNUSED(position); UNUSED(on); }
static void tug_set_winch_on(void *value, bool_t on)
{ UNUSED(value); UNUSED(on); }
static void bp_reconnect_notify(void) { reconnect_notifications++; }

/* The runner supplies the actual controller functions, without substitutions. */
#include "fast_brake_handoff_controller.inc"

static void
reset_test(bool fast_mode, bool slave, bool ignore, int lift_type)
{
    memset(&bp, 0, sizeof(bp));
    memset(&drs, 0, sizeof(drs));
    tug.info = &tug_info;
    tug_info.lift_type = lift_type;
    fast = fast_mode;
    slave_mode = slave;
    cfg_ignore_park_break = ignore;
    pb_set_override = false;
    pb_set_remote = false;
    op_complete = false;
    coupled_wheel_brake = false;
    brake_writes = reconnect_notifications = 0;
    bp_hint_status_str = NULL;
    bp.cur_t = 10;
}

static void
test_mode_matrix_and_pre_lift_abort(void)
{
    for (int lift_type = LIFT_GRAB; lift_type <= LIFT_WINCH; lift_type++) {
        for (int fast_mode = 0; fast_mode <= 1; fast_mode++) {
            for (int slave = 0; slave <= 1; slave++) {
                for (int ignore = 0; ignore <= 1; ignore++) {
                    for (int required = 0; required <= 1; required++) {
                        reset_test(fast_mode, slave, ignore, lift_type);
                        pb_enter_ungrabbing(required);
                        bp.cur_t = 23;
                        pb_step_ungrabbing();
                        bool guarded = fast_mode && !slave && !ignore && required;
                        assert(bp.step == (guarded ? PB_STEP_UNGRABBING :
                            PB_STEP_WAITING4OK2DISCO));
                        assert(drs.lbrake.value == (guarded ? 0.9 : 0));
                        assert(brake_writes == (slave ? 0 : 2));
                    }
                }
            }
        }
    }
}

static void
test_masked_signal_can_retry_and_complete(void)
{
    reset_test(true, false, false, LIFT_GRAB);
    coupled_wheel_brake = true;
    brakes_set(B_TRUE);
    pb_enter_ungrabbing(B_TRUE);
    pb_step_ungrabbing();
    assert(bp.step == PB_STEP_UNGRABBING);
    bp.cur_t = 11.5;
    pb_step_ungrabbing();
    assert(drs.lbrake.value == 0);
    bp.cur_t = 11.6;
    pb_step_ungrabbing();
    assert(drs.lbrake.value == 0.9);

    /* BPB pressure masks the false state after restore; retry must still run. */
    bp.cur_t = 11.7;
    pb_step_ungrabbing();
    bp.cur_t = 13.21;
    pb_step_ungrabbing();
    assert(drs.lbrake.value == 0);
    assert(bp.step == PB_STEP_UNGRABBING);
    drs.pbrake.value = 1;
    unsigned writes_after_release = brake_writes;
    bp.cur_t = 14.2;
    pb_step_ungrabbing();
    assert(bp.step == PB_STEP_UNGRABBING);
    assert(brake_writes == writes_after_release);
    bp.cur_t = 14.22;
    pb_step_ungrabbing();
    assert(bp.step == PB_STEP_WAITING4OK2DISCO);
    assert(brake_writes == writes_after_release);
}

static void
test_delayed_drop_restores_without_disconnect(void)
{
    reset_test(true, false, false, LIFT_WINCH);
    drs.pbrake.value = 1;
    pb_enter_ungrabbing(B_TRUE);
    pb_step_ungrabbing();
    bp.cur_t = 11.5;
    pb_step_ungrabbing();
    unsigned writes_after_release = brake_writes;
    bp.cur_t = 12.1;
    pb_step_ungrabbing();
    assert(bp.step == PB_STEP_UNGRABBING);
    assert(brake_writes == writes_after_release);
    drs.pbrake.value = 0;
    bp.cur_t = 12.2;
    pb_step_ungrabbing();
    assert(drs.lbrake.value == 0.9);
    assert(bp.step == PB_STEP_UNGRABBING);
}

static void
test_reconnect_lowering_resets_verified_handoff(void)
{
    reset_test(true, false, false, LIFT_GRAB);
    drs.pbrake.value = 1;
    pb_enter_ungrabbing(B_TRUE);
    pb_step_ungrabbing();
    bp.cur_t = 11.5;
    pb_step_ungrabbing();
    bp.cur_t = 12.5;
    pb_step_ungrabbing();
    assert(bp.fast_brake_handoff.phase == BP_FAST_BRAKE_VERIFIED);
    assert(recon_handler(0, 0, NULL) == 1);
    assert(bp.step == PB_STEP_GRABBING);
    assert(reconnect_notifications == 1);

    bp.step = PB_STEP_STOPPED;
    bp.cur_t = 20;
    pb_step_stopped();
    assert(bp.step == PB_STEP_LOWERING);
    bp.cur_t = 20.01;
    pb_step_lowering();
    assert(bp.step == PB_STEP_UNGRABBING);
    assert(bp.fast_brake_handoff.phase == BP_FAST_BRAKE_HOLDING);
    assert(bp.fast_brake_handoff.required);
    pb_step_ungrabbing();
    assert(bp.step == PB_STEP_UNGRABBING);
    assert(drs.lbrake.value == 0.9);
    bp.cur_t = 21.52;
    pb_step_ungrabbing();
    assert(drs.lbrake.value == 0);
    bp.cur_t = 22.53;
    pb_step_ungrabbing();
    assert(bp.step == PB_STEP_WAITING4OK2DISCO);
}

static void
test_cradle_finishes_before_release(void)
{
    for (int lift_type = LIFT_GRAB; lift_type <= LIFT_WINCH; lift_type++) {
        reset_test(false, false, false, lift_type);
        drs.pbrake.value = 1;
        brakes_set(B_TRUE);
        pb_enter_ungrabbing(B_TRUE);
        unsigned writes_while_held = brake_writes;
        double duration = lift_type == LIFT_GRAB ? 12 : 4;
        bp.cur_t = 10 + duration - 0.01;
        pb_step_ungrabbing();
        assert(bp.step == PB_STEP_UNGRABBING);
        assert(drs.lbrake.value == 0.9);
        assert(brake_writes == writes_while_held);
        bp.cur_t = 10 + duration;
        pb_step_ungrabbing();
        assert(bp.step == PB_STEP_WAITING4OK2DISCO);
        assert(drs.lbrake.value == 0);
    }
}

int
main(void)
{
    test_mode_matrix_and_pre_lift_abort();
    test_masked_signal_can_retry_and_complete();
    test_delayed_drop_restores_without_disconnect();
    test_reconnect_lowering_resets_verified_handoff();
    test_cradle_finishes_before_release();
    puts("Fast brake controller integration tests passed");
    return 0;
}
