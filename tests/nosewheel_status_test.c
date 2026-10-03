#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "handling_timing.h"
#include "pushback_step.h"

typedef bool bool_t;
typedef int XPLMCommandRef;
typedef int XPLMCommandPhase;
typedef struct { double x, y; } vect2_t;
typedef struct { double value; } test_dr_t;
#define B_TRUE true
#define B_FALSE false
#define IBM 0
#define APL 1
#define UNUSED(x) ((void)(x))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ASSERT(x) assert(x)
#define STATE_TRANS_DELAY 2.0
#define PB_CONN_LIFT_DELAY 13.0
#define PB_CONN_LIFT_DURATION 9.0
#define PB_CRADLE_DELAY 10.0
#define PB_LIFT_TE 0.075
#define MSG_WINCH 0
#define MSG_CONNECTED 1
#define MSG_OP_COMPLETE 2
#define LIFT_GRAB 0
#define LIFT_WINCH 1
#define LIFT_WALL_FRONT 0
#define LIFT_WALL_CENTER 1
#define LIFT_WALL_BACK 2
#define _(s) (s)
#define logMsg(...) ((void)0)

static struct {
    struct { int nw_i, n_gear; double nw_len, nw_z, tirrad; } acf;
    struct { vect2_t pos; double hdg, spd; } cur_pos;
    struct { vect2_t start_acf_pos; bool complete, pbrk_rele_called; } winching;
    struct { float nosewheel_rot_spd; } anim;
    struct { bool required; } fast_brake_handoff;
    double cur_t, last_t, d_t, step_start_t, last_voice_t;
    pushback_step_t step;
    bool awaiting_plan, reconnect, fast_brakes_relinquished;
    int segs;
} bp;
typedef struct {
    int lift_type, lift_wall_loc;
    double lift_height, lift_wall_z, plat_z, plat_h;
    bool anim_debug, quick_debug;
} test_tug_info_t;
typedef test_tug_info_t tug_info_t;
typedef struct {
    test_tug_info_t *info;
    struct { vect2_t pos; double hdg, spd; } pos;
    struct { double max_fwd_spd; } veh_slow;
    double tirrad;
} test_tug_t;
static test_tug_info_t info;
static test_tug_t tug;
static struct { test_tug_t *tug; } bp_ls;
static struct { test_dr_t leg_len, tire_rot_spd, tirrad; } drs;
static bool slave_mode, cfg_ignore_park_break, late_plan_requested;
static bool bp_started, bp_connected, plan_complete, op_complete;
static bool fast, parking_brake, handoff_blocked, rng_failure;
static int cycle, rate_count = 1;
static unsigned rng_calls, rate_reads, geometry_writes, brake_writes;
static unsigned reconnect_notifications;
static const char *bp_hint_status_str;
static int *registered_snapshot;
static int registered_count;
static bool registered_writable;
static int disco_cmd, recon_cmd, clear_ack_cmd;

static int XPLMGetCycleNumber(void) { return cycle; }
static int XPLMCreateCommand(const char *name, const char *description)
{ UNUSED(name); UNUSED(description); return 1; }
static void test_rng(void *bytes, size_t count)
{
    memset(bytes, rng_failure ? 0 : 0xa5, count);
    rng_calls++;
}
#define arc4random_buf test_rng
#define DCR_CREATE_F(dr, value, writable, name) \
    do { UNUSED(dr); UNUSED(value); assert(!(writable)); UNUSED(name); } while (0)
#define DCR_CREATE_VI(dr, value, count, writable, name) \
    do { UNUSED(dr); assert(strcmp(name, "bp/anim/nosewheel_rotation_status") == 0); \
        registered_snapshot = value; registered_count = count; \
        registered_writable = writable; } while (0)
static double vect2_dist(vect2_t a, vect2_t b)
{ return hypot(a.x - b.x, a.y - b.y); }
static double artificial_delay(double seconds)
{ return bp_handling_duration(seconds, fast); }
static double handling_fraction(double seconds, double duration)
{ return bp_handling_fraction(seconds, duration, fast); }
static bool_t pbrake_is_set(void) { return parking_brake; }
static void brakes_set(bool_t on) { UNUSED(on); brake_writes++; }
static int dr_getvf32(test_dr_t *dr, float *value, int offset, int count)
{
    UNUSED(offset); assert(count == 1); rate_reads++;
    if (rate_count == 1) *value = dr->value;
    return rate_count;
}
static int dr_getvf(test_dr_t *dr, double *value, int offset, int count)
{ UNUSED(offset); assert(count == 1); *value = dr->value; return 1; }
static void dr_setvf(test_dr_t *dr, double *value, int offset, int count)
{ UNUSED(offset); assert(count == 1); dr->value = *value; geometry_writes++; }
static void push_at_speed(double a, double b, bool_t c, bool_t d)
{ UNUSED(a); UNUSED(b); UNUSED(c); UNUSED(d); }
static void turn_nosewheel(double value) { UNUSED(value); }
static void tug_set_lift_pos(double value) { UNUSED(value); }
static void tug_set_lift_arm_pos(void *t, double value, bool_t on)
{ UNUSED(t); UNUSED(value); UNUSED(on); }
static void tug_set_winch_on(void *t, bool_t on) { UNUSED(t); UNUSED(on); }
static void tug_set_TE_override(void *t, bool_t on) { UNUSED(t); UNUSED(on); }
static void tug_set_TE_snd(void *t, double value, double elapsed)
{ UNUSED(t); UNUSED(value); UNUSED(elapsed); }
static void tug_set_lift_in_transit(bool_t on) { UNUSED(on); }
static void tug_set_cradle_beeper_on(void *t, bool_t on)
{ UNUSED(t); UNUSED(on); }
static void tug_set_cradle_air_on(void *t, bool_t on, double now)
{ UNUSED(t); UNUSED(on); UNUSED(now); }
static double tug_lift_wall_off(const test_tug_t *t)
{ return t->info->lift_wall_loc == LIFT_WALL_FRONT ? t->tirrad :
    (t->info->lift_wall_loc == LIFT_WALL_BACK ? -t->tirrad : 0); }
static double tug_plat_h(const test_tug_t *t)
{ return t->info->lift_type == LIFT_WINCH ?
    (1 - t->tirrad / (t->info->lift_wall_z - t->info->plat_z)) * t->info->plat_h : 0; }
static bool_t tug_is_stopped(const test_tug_t *t) { return t->pos.spd == 0; }
static double msg_dur(int message) { UNUSED(message); return 0; }
static void msg_play(int message) { UNUSED(message); }
static bool_t emergency_tow_is_active(void) { return false; }
static void bp_emergency_tow_planner_notify(void) {}
static bool_t late_plan_end_cond(void) { return plan_complete; }
static void enable_replanning(void) {}
static bool_t bp_legacy_routes(void) { return false; }
static bool_t emergency_tow_allows_persistent_routes(void) { return true; }
static struct { bool active; } push_manual;
static void *list_head(void *list) { UNUSED(list); return &bp; }
static void route_save_legacy(void *list) { UNUSED(list); }
static bool_t fast_brake_handoff_active(void) { return handoff_blocked; }
static bool_t fast_brake_handoff_ready(void) { return !handoff_blocked; }
static void bp_fast_brake_handoff_reset(void *state, bool_t required)
{ UNUSED(state); UNUSED(required); }
static void bp_reconnect_notify(void) { reconnect_notifications++; }

#include "nosewheel_reasons.inc"
#include "nosewheel_producers.inc"

static uint64_t counter(int offset)
{ return (uint64_t)(uint32_t)nw_status.snapshot[offset] |
    ((uint64_t)(uint32_t)nw_status.snapshot[offset + 1] << 32); }

static void expect(int mode, int phase, int flags, int reason)
{
    nw_publish();
    assert(nw_status.snapshot[0] == 1);
    assert(nw_status.snapshot[1] == mode);
    assert(nw_status.snapshot[2] == phase);
    assert(nw_status.snapshot[3] == flags);
    assert(nw_status.snapshot[14] == cycle);
    assert(nw_status.snapshot[16] == reason);
    if (mode != NW_RATE) assert(nw_status.snapshot[15] == -1);
}

static void reset(int kind, bool quick)
{
    memset(&nw_status, 0, sizeof(nw_status));
    memset(&bp, 0, sizeof(bp));
    memset(&drs, 0, sizeof(drs));
    info = (test_tug_info_t){ .lift_type = kind, .lift_wall_loc = LIFT_WALL_CENTER,
        .lift_height = 0.3, .lift_wall_z = kind == LIFT_GRAB ? NAN : 2,
        .plat_z = kind == LIFT_GRAB ? NAN : 0,
        .plat_h = kind == LIFT_GRAB ? NAN : 0.2 };
    /* GRAB configs omit these fields; the real parser defaults them to NaN. */
    tug = (test_tug_t){ .info = &info, .tirrad = 0.3,
        .veh_slow.max_fwd_spd = 0.1 };
    bp_ls.tug = &tug;
    bp.acf.nw_i = 0; bp.acf.n_gear = 3;
    bp.acf.nw_len = 1; bp.acf.tirrad = 0.3; bp.acf.nw_z = -5;
    drs.tirrad.value = 0.3;
    nw_status.nosegear_index = nw_status.lift_kind = -1;
    nw_status.rate_sample_cycle = -1;
    cycle = 10; fast = quick;
    slave_mode = cfg_ignore_park_break = late_plan_requested = false;
    parking_brake = handoff_blocked = rng_failure = false;
    bp_started = true; plan_complete = op_complete = bp_connected = false;
    rate_count = 1;
    rng_calls = rate_reads = geometry_writes = brake_writes = reconnect_notifications = 0;
    bp_boot_init();
    bp_nosewheel_status_enable();
    nw_basis_ready();
    nw_new_operation(false);
    assert(registered_snapshot == nw_status.snapshot);
    assert(registered_count == 17 && !registered_writable && rng_calls == 1);
    assert(nw_status.snapshot[4] == 0);
}

static void grab(bool quick)
{
    reset(LIFT_GRAB, quick);
    bp.step = PB_STEP_GRABBING;
    nw_observe_step();
    expect(NW_XP, NW_CAPTURE_PREP, NW_READY | NW_MASTER, BP_NW_NONE);
    bp.cur_t = 20;
    pb_step_connect_grab();
    expect(NW_HOLD, NW_CAPTURED_PRE_LIFT,
        NW_READY | NW_MASTER | NW_CUSTODY, BP_NW_NONE);
    assert(!bp_connected && nw_status.snapshot[5] == 0);
    late_plan_requested = true;
    bp.step = PB_STEP_LIFTING;
    nw_observe_step();
    pb_step_lift();
    expect(NW_HOLD, NW_CAPTURED_PRE_LIFT,
        NW_READY | NW_MASTER | NW_CUSTODY, BP_NW_NONE);
    late_plan_requested = false;
    if (!quick) {
        bp.cur_t = 24;
        nw_observe_step();
        pb_step_lift();
        expect(NW_HOLD, NW_LIFTING,
            NW_READY | NW_MASTER | NW_CUSTODY, BP_NW_NONE);
        assert(!nw_status.fully_lifted);
    }
    bp.cur_t = 50;
    nw_observe_step();
    pb_step_lift();
    expect(NW_HOLD, NW_CARRIED_STILL,
        NW_READY | NW_MASTER | NW_CUSTODY | NW_FULLY_LIFTED, BP_NW_NONE);
    tug.pos.spd = -1;
    /* Even a held/MIN frame must publish the current private speed class. */
    expect(NW_HOLD, NW_CARRIED_MOVING,
        NW_READY | NW_MASTER | NW_CUSTODY | NW_FULLY_LIFTED, BP_NW_NONE);
    nw_status.phase = NW_CARRIED_MOVING;
    tug.pos.spd = NAN;
    expect(NW_INVALID, NW_INVALID_STATE, NW_READY | NW_MASTER,
        BP_NW_INVALID_GEOMETRY);
    tug.pos.spd = 0;
    bp.step = PB_STEP_LOWERING;
    bp.cur_t = 100; bp.step_start_t = 0;
    handoff_blocked = true;
    nw_observe_step(); pb_step_lowering();
    expect(NW_HOLD, NW_LOWERING,
        NW_READY | NW_MASTER | NW_CUSTODY | NW_FULLY_LIFTED, BP_NW_NONE);
    handoff_blocked = false;
    pb_step_lowering();
    expect(NW_HOLD, NW_UNGRABBING,
        NW_READY | NW_MASTER | NW_CUSTODY, BP_NW_NONE);
    bp.cur_t = 130;
    assert(pb_step_ungrabbing_grab());
    expect(NW_XP, NW_RELEASED_CLEARING, NW_READY | NW_MASTER, BP_NW_NONE);
    assert(bp_connected); /* Connected is deliberately not animation authority. */
    bp.step = PB_STEP_WAITING4OK2DISCO;
    nw_observe_step();
    expect(NW_XP, NW_WAIT_DISCONNECT, NW_READY | NW_MASTER, BP_NW_NONE);
    uint64_t epoch = counter(10);
    assert(recon_handler(0, 0, NULL) == 1);
    assert(counter(10) == epoch + 1 && reconnect_notifications == 1);
    expect(NW_XP, NW_CAPTURE_PREP, NW_READY | NW_MASTER | NW_RECONNECT, BP_NW_NONE);
    bp.cur_t += 20;
    pb_step_connect_grab();
    expect(NW_HOLD, NW_CAPTURED_PRE_LIFT,
        NW_READY | NW_MASTER | NW_RECONNECT | NW_CUSTODY, BP_NW_NONE);
}

static void winch(bool quick)
{
    reset(LIFT_WINCH, quick);
    bp.step = PB_STEP_GRABBING;
    bp.anim.nosewheel_rot_spd = 99; /* stale rate has no admission */
    bp.cur_t = 1;
    nw_observe_step(); pb_step_connect_winch();
    expect(NW_XP, NW_CAPTURE_PREP, NW_READY | NW_MASTER, BP_NW_NONE);
    bp.cur_t = 5; drs.tire_rot_spd.value = 2;
    parking_brake = true;
    nw_observe_step(); pb_step_connect_winch();
    expect(NW_XP, NW_CAPTURE_PREP, NW_READY | NW_MASTER, BP_NW_NONE);
    assert(rate_reads == 0);
    parking_brake = false;
    nw_observe_step(); pb_step_connect_winch();
    expect(NW_RATE, NW_WINCH_LOADING,
        NW_READY | NW_MASTER | NW_CUSTODY, BP_NW_NONE);
    assert(nw_status.snapshot[15] == cycle && rate_reads == 1);
    cycle++;
    expect(NW_RATE, NW_WINCH_LOADING,
        NW_READY | NW_MASTER | NW_CUSTODY | NW_RATE_HELD_SAMPLE, BP_NW_NONE);
    assert(nw_status.snapshot[15] == cycle - 1);
    nw_observe_step(); pb_step_connect_winch();
    expect(NW_RATE, NW_WINCH_LOADING,
        NW_READY | NW_MASTER | NW_CUSTODY, BP_NW_NONE);
    assert(rate_reads == 2);
    nw_end_requested();
    expect(NW_HOLD, NW_PARTIAL_HELD,
        NW_READY | NW_MASTER | NW_CUSTODY | NW_END_REQUESTED, BP_NW_SOFT_END);
    assert(bp.anim.nosewheel_rot_spd == 2);
    nw_fault(BP_NW_TIME_RESET);
    expect(NW_INVALID, NW_INVALID_STATE,
        NW_READY | NW_MASTER | NW_END_REQUESTED, BP_NW_TIME_RESET);
    nw_observe_step(); pb_step_connect_winch();
    expect(NW_RATE, NW_WINCH_LOADING,
        NW_READY | NW_MASTER | NW_CUSTODY | NW_END_REQUESTED, BP_NW_SOFT_END);
    bp.cur_pos.pos.x = 2;
    nw_observe_step(); pb_step_connect_winch();
    expect(NW_HOLD, NW_CAPTURED_PRE_LIFT,
        NW_READY | NW_MASTER | NW_CUSTODY | NW_END_REQUESTED, BP_NW_SOFT_END);
    assert(bp.anim.nosewheel_rot_spd == 0);
    bp.step = PB_STEP_UNGRABBING; bp.cur_t = 100; bp.step_start_t = 0;
    nw_observe_step(); assert(pb_step_ungrabbing_winch());
    expect(NW_HOLD, NW_UNGRABBING,
        NW_READY | NW_MASTER | NW_CUSTODY | NW_END_REQUESTED, BP_NW_SOFT_END);
    bp.step = PB_STEP_WAITING4OK2DISCO;
    nw_observe_step();
    uint64_t epoch = counter(10);
    recon_handler(0, 0, NULL);
    assert(counter(10) == epoch + 1);
    assert(bp.winching.complete);
    bp.anim.nosewheel_rot_spd = 99;
    bp.cur_t = 110;
    nw_observe_step(); pb_step_connect_winch();
    expect(NW_HOLD, NW_CAPTURED_PRE_LIFT,
        NW_READY | NW_MASTER | NW_CUSTODY | NW_RECONNECT, BP_NW_NONE);
    assert(rate_reads == 3); /* no invented second winch copy */
    bp.step = PB_STEP_MOVING_AWAY;
    bp.cur_pos.pos = (vect2_t){0, 0};
    tug.pos.pos = (vect2_t){4, 0}; tug.pos.spd = 0.1;
    run_roll_off();
    expect(NW_RATE, NW_WINCH_ROLL_OFF,
        NW_READY | NW_MASTER | NW_CUSTODY | NW_RECONNECT, BP_NW_NONE);
    assert(bp.anim.nosewheel_rot_spd < 0);
    tug.pos.pos.x = 5;
    run_roll_off();
    expect(NW_XP, NW_RELEASED_CLEARING,
        NW_READY | NW_MASTER | NW_RECONNECT, BP_NW_NONE);
    assert(bp.anim.nosewheel_rot_spd == 0);
}

static void exceptional_paths(void)
{
    reset(LIFT_GRAB, false);
    nw_end_requested();
    expect(NW_XP, NW_APPROACH, NW_READY | NW_MASTER | NW_END_REQUESTED, BP_NW_SOFT_END);
    bp.step = PB_STEP_LOWERING; bp.cur_t = 3;
    nw_observe_step(); pb_step_lowering();
    expect(NW_HOLD, NW_LOWERING,
        NW_READY | NW_MASTER | NW_CUSTODY | NW_END_REQUESTED, BP_NW_SOFT_END);
    assert(!nw_status.fully_lifted); /* actual support after abort before capture */
    reset(LIFT_GRAB, false);
    bp.cur_t = 20; pb_step_connect_grab();
    cfg_ignore_park_break = true;
    bp.step = PB_STEP_LIFTING; bp.cur_t = 100;
    nw_observe_step(); pb_step_lift();
    assert(!nw_status.fully_lifted && geometry_writes == 0);
    nw_observe_step();
    expect(NW_HOLD, NW_PARTIAL_HELD, NW_READY | NW_MASTER | NW_CUSTODY, BP_NW_NONE);
    reset(LIFT_WINCH, true);
    bp.cur_t = 5;
    drs.tire_rot_spd.value = -2;
    pb_step_connect_winch();
    expect(NW_INVALID, NW_INVALID_STATE, NW_READY | NW_MASTER, BP_NW_INVALID_RATE);
    assert(bp.anim.nosewheel_rot_spd == -2);
    drs.tire_rot_spd.value = NAN;
    pb_step_connect_winch();
    expect(NW_INVALID, NW_INVALID_STATE, NW_READY | NW_MASTER, BP_NW_INVALID_RATE);
    rate_count = 0;
    pb_step_connect_winch();
    expect(NW_INVALID, NW_INVALID_STATE, NW_READY | NW_MASTER, BP_NW_INVALID_RATE);
    rate_count = 1; drs.tire_rot_spd.value = 2;
    info.plat_z = info.lift_wall_z;
    pb_step_connect_winch();
    expect(NW_INVALID, NW_INVALID_STATE, NW_READY | NW_MASTER, BP_NW_INVALID_GEOMETRY);
    reset(LIFT_WINCH, false);
    bp.cur_t = 5; drs.tire_rot_spd.value = 2;
    pb_step_connect_winch();
    bp.step = PB_STEP_MOVING_AWAY; tug.pos.pos.x = 4; tug.pos.spd = 0;
    run_roll_off();
    expect(NW_INVALID, NW_INVALID_STATE, NW_READY | NW_MASTER, BP_NW_INVALID_GEOMETRY);
    reset(LIFT_GRAB, false);
    slave_mode = true;
    expect(NW_INVALID, NW_INVALID_STATE, NW_READY, BP_NW_UNSUPPORTED_SLAVE);
    slave_mode = false; info.quick_debug = true;
    expect(NW_INVALID, NW_INVALID_STATE, NW_READY | NW_MASTER, BP_NW_DEBUG_MODE);
    info.quick_debug = false; info.anim_debug = true;
    expect(NW_INVALID, NW_INVALID_STATE, NW_READY | NW_MASTER, BP_NW_DEBUG_MODE);
}

static void geometry_validation(void)
{
    const double invalid_platform_heights[] = { NAN, INFINITY, -0.1 };
    const double invalid_lift_heights[] = { NAN, INFINITY, 0, -0.1 };

    for (unsigned i = 0; i < sizeof(invalid_platform_heights) /
        sizeof(invalid_platform_heights[0]); i++) {
        reset(LIFT_WINCH, false);
        info.plat_h = invalid_platform_heights[i];
        assert(!nw_tug_basis());
        expect(NW_INVALID, NW_INVALID_STATE, NW_READY | NW_MASTER,
            BP_NW_INVALID_GEOMETRY);
        assert(!nw_status.custody && !nw_status.rate_window);
    }
    reset(LIFT_WINCH, false);
    info.plat_h = 0;
    assert(nw_tug_basis()); /* Zero remains a valid WINCH platform height. */
    for (int kind = LIFT_GRAB; kind <= LIFT_WINCH; kind++) {
        for (unsigned i = 0; i < sizeof(invalid_lift_heights) /
            sizeof(invalid_lift_heights[0]); i++) {
            reset(kind, false);
            info.lift_height = invalid_lift_heights[i];
            assert(!nw_tug_basis());
            expect(NW_INVALID, NW_INVALID_STATE, NW_READY | NW_MASTER,
                BP_NW_INVALID_GEOMETRY);
        }
        reset(kind, false);
        bp.acf.tirrad = 0;
        nw_basis_ready();
        assert(!nw_tug_basis());
        expect(NW_INVALID, NW_INVALID_STATE, NW_MASTER,
            BP_NW_INVALID_GEOMETRY);
    }
    reset(LIFT_GRAB, false);
    info.lift_type = -1;
    assert(!nw_tug_basis());
    expect(NW_INVALID, NW_INVALID_STATE, NW_READY | NW_MASTER,
        BP_NW_INVALID_GEOMETRY);
    reset(LIFT_WINCH, false);
    info.plat_z = NAN;
    assert(!nw_tug_basis());
    expect(NW_INVALID, NW_INVALID_STATE, NW_READY | NW_MASTER,
        BP_NW_INVALID_GEOMETRY);
    reset(LIFT_WINCH, false);
    info.lift_wall_z = info.plat_z;
    assert(!nw_tug_basis());
    expect(NW_INVALID, NW_INVALID_STATE, NW_READY | NW_MASTER,
        BP_NW_INVALID_GEOMETRY);
    reset(LIFT_WINCH, false);
    info.lift_wall_loc = -1;
    assert(!nw_tug_basis());
    expect(NW_INVALID, NW_INVALID_STATE, NW_READY | NW_MASTER,
        BP_NW_INVALID_GEOMETRY);
    reset(LIFT_WINCH, false);
    tug.tirrad = NAN;
    assert(!nw_tug_basis());
    expect(NW_INVALID, NW_INVALID_STATE, NW_READY | NW_MASTER,
        BP_NW_INVALID_GEOMETRY);
}

static void lifecycle(void)
{
    const bp_nosewheel_status_reason_t reasons[] = {
        BP_NW_HARD_ABORT, BP_NW_AIRCRAFT_RESET, BP_NW_PROVIDER_DISABLED,
        BP_NW_CORE_RELOAD, BP_NW_INITIALIZATION_FAILED
    };
    for (unsigned i = 0; i < sizeof(reasons) / sizeof(reasons[0]); i++) {
        reset(LIFT_WINCH, true);
        bp.cur_t = 5; drs.tire_rot_spd.value = 2; pb_step_connect_winch();
        uint64_t epoch = counter(10);
        uint32_t token[4]; memcpy(token, nw_status.boot_token, sizeof(token));
        bp_nosewheel_status_invalidate(reasons[i]);
        assert(counter(10) == epoch + 1);
        assert(nw_status.snapshot[4] == -1 && nw_status.snapshot[5] == -1);
        expect(NW_INVALID, NW_INVALID_STATE, NW_MASTER, reasons[i]);
        assert(bp.anim.nosewheel_rot_spd == 2);
        assert(memcmp(token, nw_status.boot_token, sizeof(token)) == 0);
    }
    reset(LIFT_GRAB, false);
    bp.cur_t = 20; pb_step_connect_grab();
    nw_end_window(); /* normal route exhaustion is not a soft-end request */
    assert(!nw_status.end_requested && nw_status.reason == BP_NW_NONE);
    nw_completed(); nw_reset_basis(); nw_basis_ready();
    expect(NW_XP, NW_COMPLETED, NW_READY | NW_MASTER, BP_NW_NORMAL_COMPLETE);
    uint64_t epoch = counter(10);
    nw_completed();
    assert(counter(10) == epoch && nw_status.reason == BP_NW_NORMAL_COMPLETE);
    nw_new_operation(false);
    assert(counter(10) == epoch + 1 && nw_status.reason == BP_NW_NONE);
    bp_nosewheel_status_invalidate(BP_NW_CORE_RELOAD);
    bp_nosewheel_status_invalidate(BP_NW_PROVIDER_DISABLED);
    assert(nw_status.reason == BP_NW_CORE_RELOAD);
    bp_nosewheel_status_enable(); nw_basis_ready();
    expect(NW_XP, NW_IDLE, NW_READY | NW_MASTER, BP_NW_NONE);
    assert(rng_calls == 1);
    nw_status.epoch = UINT64_MAX;
    nw_new_operation(false);
    expect(NW_INVALID, NW_INVALID_STATE, NW_MASTER, BP_NW_CONTRACT_NOT_READY);
    assert(counter(10) == UINT64_MAX);
    reset(LIFT_GRAB, false);
    nw_status.revision = UINT64_MAX;
    expect(NW_INVALID, NW_INVALID_STATE, NW_MASTER, BP_NW_CONTRACT_NOT_READY);
    assert(counter(12) == UINT64_MAX);
    reset(LIFT_GRAB, false);
    assert((uint32_t)nw_word(UINT32_MAX) == UINT32_MAX);
    assert((uint32_t)nw_word(UINT32_C(0x80000000)) == UINT32_C(0x80000000));
    bp_shut_fini();
    assert(!nw_status.booted && !nw_status.rate_window && !nw_status.custody);
    reset(LIFT_GRAB, false);
    rng_failure = true;
    bp_boot_init();
    expect(NW_INVALID, NW_INVALID_STATE, NW_MASTER, BP_NW_CONTRACT_NOT_READY);
    assert(bp_started); /* metadata failure cannot inhibit the existing engine */
}

int main(void)
{
    grab(false); grab(true);
    winch(false); winch(true);
    exceptional_paths(); geometry_validation(); lifecycle();
    printf("Nosewheel status: GRAB/WINCH, normal/Fast, gates, rates, reconnect, lifecycle passed.\n");
    return 0;
}
