#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "interface_mode.h"
#include "clear_signal_gate.h"
#include "handling_timing.h"
#include "post_push_automation.h"

typedef bool bool_t;
#define B_TRUE true
#define B_FALSE false
#define BP_INFO_LOG ""
#define STATE_TRANS_DELAY 2.0
typedef struct { double x, y; } vect2_t;
typedef struct { vect2_t pos; double hdg; } position_t;
typedef struct {
    int step;
    double cur_t, step_start_t, start_hdg;
    bool_t ok2disco;
    position_t cur_pos;
    vect2_t start_pos;
    struct { double nw_z; } acf;
    struct { double wheelbase; } veh;
    bp_clear_signal_gate_t clear_signal_gate;
} state_t;
static state_t bp;
typedef struct { double apch_dist; } tug_info_t;
typedef struct { tug_info_t *info; position_t pos;
    struct { double wheelbase; } veh; } tug_t;
static tug_t tug;
static struct { void *disco_win; tug_t *tug; } bp_ls;
static bool_t cfg_disco_when_done, slave_mode, fast;
static bp_interface_mode_t interface_mode;
static unsigned window_show, window_hide, drives, signals, fallbacks;
static double artificial_delay(double seconds) {
    return bp_handling_duration(seconds, fast);
}
static bool_t bp_fast_ground_handling(void) { return fast; }
static bp_interface_mode_t bp_get_interface_mode(void) { return interface_mode; }
static void logMsg(const char *text) { (void)text; }
static void disco_intf_show(void) { ++window_show; bp_ls.disco_win = &bp; }
static void disco_intf_hide(void) { ++window_hide; bp_ls.disco_win = NULL; }
static bool_t tug_clear_is_right(void) { return true; }
static void tug_set_clear_signal(bool_t show, bool_t right) {
    (void)right; signals += show;
}
static vect2_t hdg2dir(double hdg) { (void)hdg; return (vect2_t){0, 1}; }
static vect2_t vect2_add(vect2_t a, vect2_t b) {
    return (vect2_t){a.x + b.x, a.y + b.y};
}
static vect2_t vect2_sub(vect2_t a, vect2_t b) {
    return (vect2_t){a.x - b.x, a.y - b.y};
}
static vect2_t vect2_scmul(vect2_t a, double factor) {
    return (vect2_t){a.x * factor, a.y * factor};
}
static vect2_t vect2_norm(vect2_t a, bool_t right) {
    (void)right; return (vect2_t){a.y, -a.x};
}
static double vect2_dotprod(vect2_t a, vect2_t b) { return a.x*b.x + a.y*b.y; }
static double dir2hdg(vect2_t dir) { (void)dir; return 0; }
static double rel_hdg(double a, double b) { return b - a; }
static double normalize_hdg(double hdg) { return hdg; }
static bool_t tug_drive2point(void *which, vect2_t pos, double hdg) {
    (void)which; (void)pos; (void)hdg; ++drives; return true;
}
static void drive_away_fallback(void) { ++fallbacks; }

#include "post_push_controller.inc"

static void reset(unsigned mask, double elapsed) {
    memset(&bp, 0, sizeof(bp));
    bp.step = 10;
    bp.cur_t = elapsed;
    bp.veh.wheelbase = 10;
    interface_mode = (mask & 1) ? BP_INTERFACE_MODE_LEGACY_MAGIC_SQUARES
        : BP_INTERFACE_MODE_GROUND_OPS;
    cfg_disco_when_done = !!(mask & 2);
    slave_mode = !!(mask & 4);
    fast = !!(mask & 8);
    bp.ok2disco = !!(mask & 16);
    bp.clear_signal_gate.displayed = !!(mask & 32);
    bp.clear_signal_gate.acknowledged = !!(mask & 64);
    bp_ls.tug = &tug;
    bp_ls.disco_win = NULL;
    window_show = window_hide = drives = signals = fallbacks = 0;
}

int main(void) {
    static tug_info_t info;
    tug.info = &info;
    const double times[] = {0, 0.1, 1.9, 2, 14.9, 15, 20};
    for (unsigned mask = 0; mask < 128; ++mask) {
        for (unsigned t = 0; t < sizeof(times)/sizeof(times[0]); ++t) {
            reset(mask, times[t]);
            bool permitted = bp.ok2disco || (cfg_disco_when_done && !slave_mode);
            pb_step_waiting4ok2disco();
            assert((bp.step == 11) == (permitted && (fast || times[t] >= 2)));
            state_t actual = bp;
            unsigned actual_show = window_show, actual_hide = window_hide;
            unsigned actual_drives = drives;
            if (!(mask & 8)) {
                reset(mask, times[t]);
                reference_pb_step_waiting4ok2disco();
                assert(memcmp(&bp, &actual, sizeof(bp)) == 0);
                assert(window_show == actual_show && window_hide == actual_hide);
                assert(drives == actual_drives);
            }

            reset(mask, times[t]);
            bool acknowledged = bp.clear_signal_gate.acknowledged ||
                interface_mode == BP_INTERFACE_MODE_LEGACY_MAGIC_SQUARES ||
                (cfg_disco_when_done && !slave_mode);
            bool had_display = bp.clear_signal_gate.displayed;
            pb_step_clear_signal();
            bool expected = acknowledged &&
                (fast ? had_display : times[t] >= BP_CLEAR_SIGNAL_MIN_SECONDS);
            assert((bp.step == 11) == expected);
            actual = bp;
            unsigned actual_signals = signals, actual_fallbacks = fallbacks;
            actual_drives = drives;
            if (!(mask & 8)) {
                reset(mask, times[t]);
                reference_pb_step_clear_signal();
                assert(memcmp(&bp, &actual, sizeof(bp)) == 0);
                assert(signals == actual_signals && fallbacks == actual_fallbacks);
                assert(drives == actual_drives);
            }
        }
    }
    /* Repeated Fast calls still cannot approve either manual pilot gate. */
    reset(8, 0);
    for (unsigned frame = 0; frame < 20; ++frame) {
        bp.cur_t = frame;
        pb_step_waiting4ok2disco();
        assert(bp.step == 10 && !bp.ok2disco);
    }
    reset(8, 0);
    for (unsigned frame = 0; frame < 20; ++frame) {
        bp.cur_t = frame;
        pb_step_clear_signal();
        assert(bp.step == 10 && !bp.clear_signal_gate.acknowledged);
    }
    assert(bp_clear_signal_acknowledge(&bp.clear_signal_gate, true));
    pb_step_clear_signal();
    assert(bp.step == 11);
    puts("Post-push interface/automation/Fast gates and upstream parity passed.");
    return 0;
}
