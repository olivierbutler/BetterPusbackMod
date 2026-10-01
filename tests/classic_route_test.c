#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef int bool_t;
enum { B_FALSE, B_TRUE };
typedef struct { int count; } list_t;
typedef struct { double lat, lon; } geo_pos2_t;
#define GEO_POS2(lat_, lon_) ((geo_pos2_t){ (lat_), (lon_) })
#define BP_INFO_LOG ""
#define _(text) (text)
static struct {
    list_t segs;
    int awaiting_plan;
    double step_start_t, cur_t;
} bp;
static struct { void *tug; } bp_ls;
static struct { double lat, lon, hdg; } drs;
static struct { int recognized; } planner_gate_context;
static struct {
    int new_route, suppress_save, loaded_slot, save_slot;
} planner_gate_routes;
static struct { int active; } push_manual;
static int slave_mode, late_plan_requested, plan_complete;
static int classic_enabled, persistent_allowed, cached_route;
static int load_calls, save_calls, default_planner_calls;
static geo_pos2_t loaded_position;
static double loaded_heading;
static int planner_running, bp_connected, replan_calls, emergency_notifications;
static const char *bp_hint_status_str;

static int bp_legacy_routes(void) { return classic_enabled; }
static int emergency_tow_allows_persistent_routes(void) {
    return persistent_allowed;
}
static void *list_head(const list_t *list) {
    return list->count ? (void *)list : NULL;
}
static double dr_getf(const double *dr) { return *dr; }
static void bp_delete_all_segs(void) { bp.segs.count = 0; }
static void planner_route_show_message(const char *message) { (void)message; }
static void logMsg(const char *message, ...) { (void)message; }
static void tug_set_lift_pos(double value) { (void)value; }
static void tug_set_lift_in_transit(int value) { (void)value; }
static void tug_set_cradle_beeper_on(void *tug, int value) {
    (void)tug;
    (void)value;
}
static void tug_set_TE_override(void *tug, int value) {
    (void)tug;
    (void)value;
}
static int emergency_tow_is_active(void) { return !persistent_allowed; }
static void bp_emergency_tow_planner_notify(void) { ++emergency_notifications; }
static int bp_cam_is_running(void) { return planner_running; }
static int bp_num_segs(void) { return bp.segs.count; }
static int bp_cam_stop(void) { planner_running = 0; return 1; }
static void enable_replanning(void) { ++replan_calls; }
static void route_load_legacy(geo_pos2_t position, double heading, list_t *segments) {
    assert(segments->count == 0);
    ++load_calls;
    loaded_position = position;
    loaded_heading = heading;
    segments->count = cached_route;
}
static void route_save_legacy(const list_t *segments) {
    assert(segments->count != 0);
    ++save_calls;
}

#include "classic_routes.inc"

static void reset(void) {
    memset(&bp, 0, sizeof(bp));
    memset(&planner_gate_context, 0, sizeof(planner_gate_context));
    memset(&planner_gate_routes, 0, sizeof(planner_gate_routes));
    planner_gate_routes.loaded_slot = -1;
    planner_gate_routes.save_slot = -1;
    classic_enabled = persistent_allowed = 1;
    cached_route = slave_mode = late_plan_requested = push_manual.active = 0;
    plan_complete = load_calls = save_calls = default_planner_calls = 0;
    planner_running = bp_connected = replan_calls = emergency_notifications = 0;
    bp_hint_status_str = NULL;
    drs.lat = 58.79;
    drs.lon = 16.91;
    drs.hdg = 123;
}

int main(void) {
    for (unsigned mask = 0; mask < 16; ++mask) {
        reset();
        classic_enabled = !!(mask & 1);
        int emergency = !!(mask & 2);
        planner_running = !!(mask & 4);
        bp.segs.count = (mask & 8) ? 3 : 0;
        int keep = classic_enabled && !emergency && !planner_running;
        call_tug(emergency);
        assert(bp.segs.count == (keep && (mask & 8) ? 3 : 0));
        assert(late_plan_requested && !planner_running);
    }
    reset();
    cached_route = 3;
    planner_open();
    assert(load_calls == 1 && bp.segs.count == 3);
    assert(loaded_position.lat == drs.lat && loaded_position.lon == drs.lon);
    assert(loaded_heading == drs.hdg);
    assert(!planner_gate_routes.new_route && planner_gate_routes.suppress_save);
    assert(!planner_gate_context.recognized && !default_planner_calls);
    assert(planner_gate_routes.loaded_slot == -1 && planner_gate_routes.save_slot == -1);
    planner_open();
    assert(load_calls == 1 && bp.segs.count == 3);
    start_push();
    assert(save_calls == 1);

    reset();
    planner_open();
    assert(load_calls == 1 && bp.segs.count == 0);
    assert(planner_gate_routes.new_route && planner_gate_routes.suppress_save);
    start_push();
    accept_late_plan();
    assert(save_calls == 0);

    reset();
    bp.segs.count = 2;
    cached_route = 5;
    planner_open();
    assert(load_calls == 0 && bp.segs.count == 2);

    /* Exhaust all save guards for both the pre-plan and late-plan branches. */
    for (unsigned mask = 0; mask < 64; ++mask) {
        reset();
        classic_enabled = !!(mask & 1);
        persistent_allowed = !!(mask & 2);
        slave_mode = !!(mask & 4);
        push_manual.active = !!(mask & 8);
        bp.segs.count = !!(mask & 16);
        late_plan_requested = !!(mask & 32);
        int eligible = classic_enabled && persistent_allowed && !slave_mode &&
            !push_manual.active && bp.segs.count;
        start_push();
        assert(save_calls == (eligible && !late_plan_requested));
        save_calls = 0;
        late_plan_requested = 1;
        plan_complete = slave_mode;
        accept_late_plan();
        assert(save_calls == eligible);
    }

    for (unsigned mask = 0; mask < 8; ++mask) {
        reset();
        classic_enabled = !!(mask & 1);
        persistent_allowed = !!(mask & 2);
        bp.segs.count = !!(mask & 4);
        cached_route = 4;
        planner_open();
        assert(load_calls == (classic_enabled && persistent_allowed && !(mask & 4)));
        assert(default_planner_calls == (!classic_enabled && persistent_allowed));
        if (!persistent_allowed)
            assert(bp.segs.count == 0 && planner_gate_routes.suppress_save);
    }

    reset();
    late_plan_requested = 1;
    for (int frame = 0; frame < 3; ++frame)
        accept_late_plan();
    assert(save_calls == 0 && late_plan_requested && bp.awaiting_plan);
    assert(replan_calls == 3 && bp_hint_status_str != NULL);
    bp.segs.count = 2;
    planner_running = 1;
    for (int frame = 0; frame < 3; ++frame)
        accept_late_plan();
    assert(save_calls == 0 && late_plan_requested && bp.awaiting_plan);
    planner_running = 0;
    bp.cur_t = 123;
    accept_late_plan();
    assert(save_calls == 1 && !late_plan_requested && !bp.awaiting_plan);
    assert(plan_complete && bp_connected && bp.step_start_t == 123);
    for (int frame = 0; frame < 20; ++frame)
        accept_late_plan();
    assert(save_calls == 1);

    reset();
    slave_mode = late_plan_requested = 1;
    accept_late_plan();
    assert(save_calls == 0 && late_plan_requested);
    plan_complete = 1;
    accept_late_plan();
    assert(save_calls == 0 && !late_plan_requested);

    puts("Optional legacy route recall/save regression tests passed.");
    return 0;
}
