#include <assert.h>
#include <math.h>
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
#define BRAKE_PEDAL_THRESH 0.03
#define STATE_TRANS_DELAY 2.0
#define PB_CONN_LIFT_DURATION 9.0
#define PB_CRADLE_DELAY 10.0
#define BP_INFO_LOG ""
#define MSG_OP_COMPLETE 0
#define MSG_DISCO 1
#define LIFT_GRAB 0
#define LIFT_WINCH 1

static struct {
    double cur_t, step_start_t, last_voice_t;
    pushback_step_t step;
    bool_t reconnect, ok2disco, fast_brakes_relinquished;
    bp_fast_brake_handoff_t fast_brake_handoff;
    struct { double nw_len; int nw_i; } acf;
    struct { double max_accel; } veh;
} bp;
static struct {
    test_dr_t pbrake, pbrake_rat, pbrake_valve, pbrake_trap;
    test_dr_t lbrake, rbrake, leg_len, override_steer;
    bool pbrake_is_custom;
} drs;
typedef struct { int lift_type; double lift_height; } test_tug_info_t;
typedef struct { test_tug_info_t *info; } test_tug_t;
static test_tug_info_t tug_info;
static test_tug_t tug;
static struct { test_tug_t *tug; void *wing_walker; } bp_ls;
static bool slave_mode, cfg_ignore_park_break, fast;
static int bp_xp_ver;
static bool pb_set_override, pb_set_remote, op_complete;
static bool bp_started, bp_connected, late_plan_requested, plan_complete;
static unsigned brake_writes, zero_writes, reconnect_notifications;
static unsigned geometry_writes, te_releases, done_notifications;
static double pilot_left, pilot_right;
static const char *bp_hint_status_str;

static double dr_getf(const test_dr_t *dr) { return dr->value; }
static int dr_geti(const test_dr_t *dr) { return (int)dr->value; }
static void dr_setf(test_dr_t *dr, double value)
{
    dr->value = value;
    if (dr == &drs.lbrake || dr == &drs.rbrake) {
        brake_writes++;
        if (value == 0) {
            zero_writes++;
            /* A synthetic release followed by still-held pilot input. */
            if (pilot_left > 0.1 && pilot_right > 0.1)
                drs.pbrake.value = drs.pbrake_rat.value = 0;
        }
    }
}
static void dr_seti(test_dr_t *dr, int value) { dr->value = value; }
static void logMsg(const char *format, ...) { UNUSED(format); }
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
static void tug_set_lift_pos(double value)
{ UNUSED(value); geometry_writes++; }
static void tug_set_cradle_air_on(void *value, bool_t on, double now)
{ UNUSED(value); UNUSED(on); UNUSED(now); }
static void tug_set_cradle_beeper_on(void *value, bool_t on)
{ UNUSED(value); UNUSED(on); }
static void tug_set_TE_override(void *value, bool_t on)
{ UNUSED(value); if (!on) te_releases++; }
static void tug_set_lift_arm_pos(void *value, double position, bool_t on)
{ UNUSED(value); UNUSED(position); UNUSED(on); geometry_writes++; }
static void tug_set_winch_on(void *value, bool_t on)
{ UNUSED(value); UNUSED(on); geometry_writes++; }
static void bp_reconnect_notify(void) { reconnect_notifications++; }
static bool emergency_tow_is_active(void) { return false; }
static void telemetry_stop(void) {}
static void bp_delete_all_segs(void) {}
static void bp_conf_set_save_enabled(bool enabled) { UNUSED(enabled); }
static void tug_free(void *value) { UNUSED(value); }
static void wing_walker_free(void *value) { UNUSED(value); }
static void disco_intf_hide(void) {}
static void bp_done_notify(void) { done_notifications++; }
static bool_t bp_state_init(void) { memset(&bp, 0, sizeof(bp)); return true; }
static void bp_emergency_tow_session_end_notify(bool value) { UNUSED(value); }

/* Observational hooks are exercised by the separate nosewheel contract test. */
#define NW_UNGRABBING 10
static void nw_phase(int phase) { UNUSED(phase); }
static void nw_completed(void) {}
static void nw_support_written(double lift, double fraction, bool_t lowering)
{ UNUSED(lift); UNUSED(fraction); UNUSED(lowering); }
static void nw_released(void) {}
static void nw_new_operation(bool_t reconnect) { UNUSED(reconnect); }

/* Compile the actual controller and completion functions, not copies. */
#include "fast_brake_handoff_controller.inc"

static void reset_test(bool fast_mode, bool slave, bool ignore, int lift_type)
{
    memset(&bp, 0, sizeof(bp));
    memset(&drs, 0, sizeof(drs));
    drs.pbrake_trap.value = 1;
    tug.info = &tug_info;
    bp_ls.tug = &tug;
    bp_ls.wing_walker = NULL;
    tug_info.lift_type = lift_type;
    fast = fast_mode;
    bp_xp_ver = 12200;
    slave_mode = slave;
    cfg_ignore_park_break = ignore;
    pb_set_override = pb_set_remote = op_complete = false;
    bp_started = bp_connected = true;
    late_plan_requested = plan_complete = true;
    brake_writes = zero_writes = reconnect_notifications = 0;
    geometry_writes = te_releases = done_notifications = 0;
    pilot_left = pilot_right = 0;
    bp_hint_status_str = NULL;
    bp.cur_t = 10;
    bp_fast_brake_handoff_reset(&bp.fast_brake_handoff, true);
}

static void pilot_frame(double left, double right)
{
    pilot_left = drs.lbrake.value = left;
    pilot_right = drs.rbrake.value = right;
    bp.cur_t += 0.05;
}

static void test_held_pedals_then_release_successful_cleanup(void)
{
    for (int type = LIFT_GRAB; type <= LIFT_WINCH; type++) {
        reset_test(true, false, false, type);
        pilot_left = pilot_right = 0.632;
        drs.pbrake.value = drs.pbrake_rat.value = 1;
        brakes_set(B_TRUE);
        unsigned held_writes = brake_writes;
        bp.step = PB_STEP_STOPPED;
        pb_step_stopped();
        assert(bp.fast_brakes_relinquished && brake_writes == held_writes);
        for (int frame = 0; frame < 100; frame++) {
            pilot_frame(0.632, 0.632);
            pb_step_stopped();
            assert(bp.step == PB_STEP_STOPPED);
            assert(brake_writes == held_writes && zero_writes == 0);
            assert(drs.pbrake.value == 1 && geometry_writes == 0);
            assert(strstr(bp_hint_status_str, "brake pedals") != NULL);
        }
        pilot_frame(0, 0);
        pb_step_stopped();
        assert(bp.step == PB_STEP_LOWERING);
        pilot_frame(0, 0);
        pb_step_lowering();
        assert(bp.step == PB_STEP_UNGRABBING);
        assert(bp.fast_brake_handoff.phase == BP_FAST_BRAKE_VERIFIED);
        /* A later pedal press must not require another handoff. */
        pilot_frame(0.632, 0.632);
        pb_step_ungrabbing();
        assert(bp.step == PB_STEP_WAITING4OK2DISCO && te_releases == 1);
        assert(brake_writes == held_writes && zero_writes == 0);
        pilot_frame(0.632, 0.632);
        bp_complete();
        assert(brake_writes == held_writes && drs.pbrake.value == 1);
        assert(!bp_started && !bp_connected && !late_plan_requested);
        assert(!plan_complete && bp_ls.tug == NULL && done_notifications == 1);
        assert(!bp.fast_brakes_relinquished);
    }
}

static void test_parking_loss_restores_hold_and_retries(void)
{
    reset_test(true, false, false, LIFT_GRAB);
    bp.step = PB_STEP_STOPPED;
    drs.pbrake.value = 1;
    pb_step_stopped();
    pilot_frame(0.632, 0.632);
    drs.pbrake.value = 0;
    pb_step_stopped();
    assert(bp.step == PB_STEP_STOPPED && !bp.fast_brakes_relinquished);
    assert(drs.lbrake.value == 0.9 && drs.rbrake.value == 0.9);
    assert(zero_writes == 0 && geometry_writes == 0);
    drs.pbrake.value = 1;
    pilot_frame(0, 0);
    pb_step_stopped();
    assert(bp.step == PB_STEP_STOPPED);
    pilot_frame(0, 0);
    pb_step_stopped();
    assert(bp.step == PB_STEP_LOWERING && zero_writes == 0);
}

static void test_xp122_common_brake_ignores_bpb_wheel_pressure(void)
{
    for (int type = LIFT_GRAB; type <= LIFT_WINCH; type++) {
        reset_test(true, false, false, type);
        drs.pbrake_trap.value = 0;
        drs.pbrake.value = 0;
        drs.pbrake_rat.value = 0.9;
        drs.lbrake.value = drs.rbrake.value = 0.9;
        bp.step = PB_STEP_STOPPED;

        /* BPB's own master-cylinder pressure is not a parking-brake signal. */
        pb_step_stopped();
        assert(bp.step == PB_STEP_STOPPED && brake_writes == 2);
        assert(!bp.fast_brakes_relinquished && geometry_writes == 0);

        /*
         * Keep both physical pedals held while selecting a mechanically
         * locked/common parking brake.  BPB must withdraw its synthetic hold
         * before geometry advances and must never write a synthetic zero.
         */
        pilot_left = pilot_right = 0.9;
        drs.pbrake.value = 1;
        pb_step_stopped();
        assert(bp.step == PB_STEP_STOPPED);
        assert(bp.fast_brakes_relinquished && brake_writes == 2);
        assert(bp.fast_brake_handoff.phase == BP_FAST_BRAKE_VERIFIED);
        assert(drs.pbrake.value == 1 && zero_writes == 0);

        pilot_frame(0.9, 0.9);
        pb_step_stopped();
        assert(bp.step == PB_STEP_LOWERING);
        assert(bp.fast_brakes_relinquished && zero_writes == 0);
        assert(drs.pbrake.value == 1);

        pilot_frame(0.9, 0.9);
        pb_step_lowering();
        assert(bp.step == PB_STEP_UNGRABBING && geometry_writes != 0);
        pb_step_ungrabbing();
        assert(bp.step == PB_STEP_WAITING4OK2DISCO);
        assert(bp.fast_brakes_relinquished && zero_writes == 0);
        assert(drs.pbrake.value == 1);

        pilot_frame(0.9, 0.9);
        unsigned held_writes = brake_writes;
        bp_complete();
        assert(brake_writes == held_writes && zero_writes == 0);
        assert(drs.pbrake.value == 1);
    }
}

static void test_fast_slave_uses_remote_parking_brake_override(void)
{
    reset_test(true, true, false, LIFT_GRAB);
    bp.step = PB_STEP_STOPPED;
    pb_set_override = true;
    pb_set_remote = true;
    drs.pbrake.value = 0;
    drs.pbrake_rat.value = 0;
    pb_step_stopped();
    assert(bp.step == PB_STEP_LOWERING);
    assert(brake_writes == 0);

    reset_test(true, true, false, LIFT_GRAB);
    bp.step = PB_STEP_STOPPED;
    pb_set_override = true;
    pb_set_remote = false;
    drs.pbrake.value = 1;
    drs.pbrake_rat.value = 1;
    pb_step_stopped();
    assert(bp.step == PB_STEP_STOPPED);
    assert(brake_writes == 0);
    assert(strstr(bp_hint_status_str, "parking brakes") != NULL);
}

static void test_xp122_valve_brake_retains_pedal_handoff(void)
{
    reset_test(true, false, false, LIFT_GRAB);
    drs.pbrake.value = 0;
    drs.pbrake_trap.value = 1;
    drs.pbrake_valve.value = 1;
    drs.lbrake.value = drs.rbrake.value = 0.9;
    bp.step = PB_STEP_STOPPED;

    pb_step_stopped();
    assert(bp.step == PB_STEP_STOPPED && bp.fast_brakes_relinquished);
    pilot_frame(0, 0);
    pb_step_stopped();
    assert(bp.step == PB_STEP_LOWERING);
}

static void test_stuck_single_invalid_pedal_and_abort(void)
{
    const double readings[][2] = {{0.05, 0}, {0, 0.05}, {NAN, 0},
        {0, INFINITY}, {-0.1, 0}, {0.03, 0.03}, {0.9, 0.9}};
    for (unsigned i = 0; i < sizeof(readings) / sizeof(readings[0]); i++) {
        reset_test(true, false, false, LIFT_WINCH);
        bp.step = PB_STEP_STOPPED;
        drs.pbrake.value = 1;
        pb_step_stopped();
        for (int frame = 0; frame < 100; frame++) {
            pilot_frame(readings[i][0], readings[i][1]);
            pb_step_stopped();
            assert(bp.step == PB_STEP_STOPPED && brake_writes == 0);
            assert(strstr(bp_hint_status_str, "abort pushback") != NULL);
        }
        bp_complete();
        assert(!bp_started && !bp_connected && bp_ls.tug == NULL);
        assert(brake_writes == 0 && done_notifications == 1);
    }
    reset_test(true, false, false, LIFT_GRAB);
    bp.step = PB_STEP_STOPPED;
    pb_step_stopped();
    assert(drs.lbrake.value == 0.9 && !bp.fast_brakes_relinquished);
    bp_complete();
    assert(drs.lbrake.value == 0 && zero_writes == 2);
}

static void test_reconnect_and_loss_before_geometry(void)
{
    reset_test(true, false, false, LIFT_GRAB);
    drs.pbrake.value = 1;
    bp.step = PB_STEP_STOPPED;
    pb_step_stopped();
    pilot_frame(0, 0);
    pb_step_stopped();
    pilot_frame(0.05, 0);
    pb_step_lowering();
    assert(bp.step == PB_STEP_UNGRABBING && geometry_writes != 0);
    drs.pbrake.value = 0;
    unsigned previous_geometry = geometry_writes;
    pb_step_ungrabbing();
    assert(bp.step == PB_STEP_UNGRABBING && geometry_writes == previous_geometry);
    assert(!bp.fast_brakes_relinquished && drs.lbrake.value == 0.9);
    bp.step = PB_STEP_WAITING4OK2DISCO;
    bp.fast_brakes_relinquished = true;
    bp.fast_brake_handoff.phase = BP_FAST_BRAKE_VERIFIED;
    assert(recon_handler(0, 0, NULL) == 1);
    assert(bp.step == PB_STEP_GRABBING && !bp.fast_brakes_relinquished);
    assert(bp.fast_brake_handoff.phase == BP_FAST_BRAKE_HOLDING);
    assert(reconnect_notifications == 1);
}

static void test_mode_matrix_and_original_cradle_timing(void)
{
    for (int type = LIFT_GRAB; type <= LIFT_WINCH; type++) {
        for (int fast_mode = 0; fast_mode <= 1; fast_mode++) {
            for (int slave = 0; slave <= 1; slave++) {
                for (int ignore = 0; ignore <= 1; ignore++) {
                    for (int required = 0; required <= 1; required++) {
                        reset_test(fast_mode, slave, ignore, type);
                        pb_enter_ungrabbing(required);
                        bp.cur_t = 23;
                        pb_step_ungrabbing();
                        bool guarded = fast_mode && !slave && !ignore && required;
                        assert(bp.step == (guarded ? PB_STEP_UNGRABBING :
                            PB_STEP_WAITING4OK2DISCO));
                        assert(brake_writes == (slave ? 0 : 2));
                        assert(drs.lbrake.value == (guarded ? 0.9 : 0));
                    }
                }
            }
        }
        reset_test(false, false, false, type);
        drs.pbrake.value = 1;
        brakes_set(B_TRUE);
        pb_enter_ungrabbing(true);
        double duration = type == LIFT_GRAB ? 12 : 4;
        bp.cur_t = 10 + duration - 0.01;
        pb_step_ungrabbing();
        assert(bp.step == PB_STEP_UNGRABBING && brake_writes == 2);
        bp.cur_t = 10 + duration;
        pb_step_ungrabbing();
        assert(bp.step == PB_STEP_WAITING4OK2DISCO && zero_writes == 2);
        bp_complete();
        assert(zero_writes == 4);
    }
}

static void test_non_fast_matches_v114_controller(void)
{
    typedef void (*step_fn)(void);
    const step_fn actual[] = {pb_step_stopped, pb_step_lowering,
        pb_step_ungrabbing};
    const step_fn original[] = {reference_pb_step_stopped,
        reference_pb_step_lowering, reference_pb_step_ungrabbing};
    const pushback_step_t steps[] = {PB_STEP_STOPPED, PB_STEP_LOWERING,
        PB_STEP_UNGRABBING};
    const double times[] = {10, 10.01, 11.99, 12, 14, 19, 22, 24};
    for (unsigned step = 0; step < 3; step++) {
        for (int type = LIFT_GRAB; type <= LIFT_WINCH; type++) {
            for (int slave = 0; slave <= 1; slave++) {
                for (int ignore = 0; ignore <= 1; ignore++) {
                    for (int parking = 0; parking <= 1; parking++) {
                        for (unsigned t = 0; t < sizeof(times) / sizeof(times[0]); t++) {
                            reset_test(false, slave, ignore, type);
                            bp.step = steps[step];
                            bp.step_start_t = bp.last_voice_t = 10;
                            bp.cur_t = times[t];
                            drs.pbrake.value = parking;
                            drs.lbrake.value = drs.rbrake.value = 0.9;
                            original[step]();
                            pushback_step_t expected_step = bp.step;
                            double expected_start = bp.step_start_t;
                            double expected_voice = bp.last_voice_t;
                            double expected_brake = drs.lbrake.value;
                            unsigned expected_writes = brake_writes;
                            unsigned expected_geometry = geometry_writes;
                            unsigned expected_te = te_releases;
                            const char *expected_hint = bp_hint_status_str;
                            reset_test(false, slave, ignore, type);
                            bp.step = steps[step];
                            bp.step_start_t = bp.last_voice_t = 10;
                            bp.cur_t = times[t];
                            drs.pbrake.value = parking;
                            drs.lbrake.value = drs.rbrake.value = 0.9;
                            actual[step]();
                            assert(bp.step == expected_step);
                            assert(bp.step_start_t == expected_start);
                            assert(bp.last_voice_t == expected_voice);
                            assert(drs.lbrake.value == expected_brake);
                            assert(drs.rbrake.value == expected_brake);
                            assert(brake_writes == expected_writes);
                            assert(geometry_writes == expected_geometry);
                            assert(te_releases == expected_te);
                            assert(!bp.fast_brakes_relinquished);
                            assert((bp_hint_status_str == NULL) == (expected_hint == NULL));
                            if (expected_hint)
                                assert(strcmp(bp_hint_status_str, expected_hint) == 0);
                        }
                    }
                }
            }
        }
    }
}

static void test_ungrabbing_preserves_verified_and_abort_exception(void)
{
    reset_test(true, false, false, LIFT_GRAB);
    bp.fast_brake_handoff.phase = BP_FAST_BRAKE_VERIFIED;
    bp.fast_brakes_relinquished = true;
    pb_enter_ungrabbing(true);
    assert(bp.fast_brake_handoff.phase == BP_FAST_BRAKE_VERIFIED);
    assert(bp.fast_brake_handoff.required);
    assert(bp.fast_brakes_relinquished);
    pb_enter_ungrabbing(false);
    assert(!bp.fast_brake_handoff.required);
    assert(!fast_brake_handoff_active());
}

int main(void)
{
    test_held_pedals_then_release_successful_cleanup();
    test_parking_loss_restores_hold_and_retries();
    test_xp122_common_brake_ignores_bpb_wheel_pressure();
    test_fast_slave_uses_remote_parking_brake_override();
    test_xp122_valve_brake_retains_pedal_handoff();
    test_stuck_single_invalid_pedal_and_abort();
    test_reconnect_and_loss_before_geometry();
    test_mode_matrix_and_original_cradle_timing();
    test_non_fast_matches_v114_controller();
    test_ungrabbing_preserves_verified_and_abort_exception();
    puts("Fast passive brake controller integration tests passed");
    return 0;
}
