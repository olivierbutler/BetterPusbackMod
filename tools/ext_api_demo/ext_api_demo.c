/*
 * BetterPushback external interface - example client and test tool.
 *
 * A small, separate plugin that uses the interface exactly as another plugin
 * would (README-EXTERNAL-API.md): it shows the published state in a window,
 * logs every change to X-Plane's Log.txt, and offers commands to load and
 * check sample routes, use the saved slots and stand in for an external
 * voice. Build with tools/ext_api_demo/build_demo.sh.
 *
 * Commands (bind them to keys in X-Plane's keyboard settings):
 *   BPDemo/toggle_window          show / hide the window
 *   BPDemo/load_straight_back     load a 40 m straight push
 *   BPDemo/load_push_turn_left    load a push ending 90 degrees left (tail right)
 *   BPDemo/load_tight_turn        load a push too tight to fit (to see a refusal)
 *   BPDemo/check_push_turn_left   check that push without loading it
 *   BPDemo/check_tight_turn       check the tight one
 *   BPDemo/clear_route            clear the route
 *   BPDemo/save_slot1, BPDemo/load_slot1
 *   BPDemo/voice_toggle           external voice on / off
 *   BPDemo/voice_stall            stop / restart the heartbeat (fallback test)
 */

#include <math.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <XPLMDataAccess.h>
#include <XPLMDisplay.h>
#include <XPLMGraphics.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>

#define LOG_PREFIX "BPDemo: "
#define EARTH_M_PER_DEG 111320.0
#define DEG2RAD(d) ((d) * M_PI / 180.0)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    const char *name;
    XPLMDataRef ref;
} dr_t;

/* Indexes into refs[]; keep in the same order. */
enum {
    R_API, R_SEQ, R_STEP_NAME, R_STAGE_NAME, R_ACTION_NAME, R_PAUSED,
    R_BLOCKER_NAME, R_BLOCKER_ITEM, R_STATUS, R_SPEED, R_DIST, R_VERSION,
    R_MSG_SEQ, R_MSG_KEY, R_MSG_TEXT, R_MSG_DUR, R_MSG_VOICE,
    R_VOICE_MODE, R_VOICE_HB, R_VOICE_DONE, R_VOICE_EXT,
    R_ROUTE_IN, R_ROUTE_SEQ, R_ROUTE_STATUS, R_ROUTE_REASON, R_ROUTE_CUR,
    R_ROUTE_SOURCE_NAME, R_ROUTE_SLOT, R_ROUTE_STAND, R_SLOT1, R_SLOT2,
    R_LAT, R_LON, R_PSI, R_COUNT
};

static dr_t refs[R_COUNT] = {
    {"bp/api_version", NULL}, {"bp/state_seq", NULL},
    {"bp/step_name", NULL}, {"bp/stage_name", NULL},
    {"bp/action_name", NULL}, {"bp/paused", NULL},
    {"bp/blocker_name", NULL}, {"bp/blocker_item", NULL},
    {"bp/status_text", NULL}, {"bp/push_speed", NULL},
    {"bp/push_distance_remaining", NULL}, {"bp/plugin_version", NULL},
    {"bp/msg_seq", NULL}, {"bp/msg_key", NULL}, {"bp/msg_text", NULL},
    {"bp/msg_duration", NULL}, {"bp/msg_voice", NULL},
    {"bp/voice_mode", NULL}, {"bp/voice_heartbeat", NULL},
    {"bp/voice_done_seq", NULL}, {"bp/voice_external", NULL},
    {"bp/route_in", NULL}, {"bp/route_seq", NULL},
    {"bp/route_status", NULL}, {"bp/route_reason", NULL},
    {"bp/route_current", NULL}, {"bp/route_source_name", NULL},
    {"bp/route_slot", NULL}, {"bp/route_stand", NULL},
    {"bp/route_slot1", NULL}, {"bp/route_slot2", NULL},
    {"sim/flightmodel/position/latitude", NULL},
    {"sim/flightmodel/position/longitude", NULL},
    {"sim/flightmodel/position/psi", NULL}
};

static XPLMWindowID window = NULL;
static XPLMFlightLoopID loop = NULL;
static int last_state_seq = -1, last_msg_seq = -1, last_route_seq = -1;
static int heartbeat = 0;
static int stalled = 0;
static int speaking_seq = 0;            /* the line the demo voice "speaks" */
static float speaking_until = 0;
static char last_line[192] = "";
static char last_answer[192] = "";

static void
logf_(const char *fmt, ...)
{
    char buf[512];
    va_list ap;

    va_start(ap, fmt);
    (void)vsnprintf(buf, sizeof (buf), fmt, ap);
    va_end(ap);
    XPLMDebugString(LOG_PREFIX);
    XPLMDebugString(buf);
    XPLMDebugString("\n");
}

static int
geti(int r)
{
    return (refs[r].ref != NULL ? XPLMGetDatai(refs[r].ref) : -1);
}

static float
getf(int r)
{
    return (refs[r].ref != NULL ? XPLMGetDataf(refs[r].ref) : -1);
}

static double
getd(int r)
{
    return (refs[r].ref != NULL ? XPLMGetDatad(refs[r].ref) : 0);
}

static void
seti(int r, int v)
{
    if (refs[r].ref != NULL)
        XPLMSetDatai(refs[r].ref, v);
}

static const char *
gets_(int r, char *buf, int len)
{
    int n;

    buf[0] = '\0';
    if (refs[r].ref == NULL)
        return (buf);
    n = XPLMGetDatab(refs[r].ref, buf, 0, len - 1);
    buf[n > 0 ? (n < len ? n : len - 1) : 0] = '\0';
    return (buf);
}

static int
interface_present(void)
{
    if (refs[R_API].ref == NULL)
        refs[R_API].ref = XPLMFindDataRef(refs[R_API].name);
    return (refs[R_API].ref != NULL && geti(R_API) >= 1);
}

static void
find_refs(void)
{
    for (int i = 0; i < R_COUNT; i++) {
        if (refs[i].ref == NULL)
            refs[i].ref = XPLMFindDataRef(refs[i].name);
    }
}

/* A pose `back_m` behind the aircraft and `side_m` to its right, heading +dh. */
static void
pose_from_here(double back_m, double side_m, double dh, char *line, int len)
{
    double lat = getd(R_LAT), lon = getd(R_LON), hdg = getf(R_PSI);
    double h = DEG2RAD(hdg);
    /* Unit vectors: forward (east, north) and right. */
    double fe = sin(h), fn = cos(h), re = cos(h), rn = -sin(h);
    double de = -back_m * fe + side_m * re;
    double dn = -back_m * fn + side_m * rn;
    double nlat = lat + dn / EARTH_M_PER_DEG;
    double nlon = lon + de / (EARTH_M_PER_DEG * cos(DEG2RAD(lat)));
    double nhdg = fmod(hdg + dh + 360.0, 360.0);

    (void)snprintf(line, len, "P %.8f %.8f %.1f\n", nlat, nlon, nhdg);
}

static void
send_route(const char *name, const char *command, double back, double side,
    double dh)
{
    char text[256], pose[96];
    XPLMCommandRef cmd;

    find_refs();
    if (!interface_present() || refs[R_ROUTE_IN].ref == NULL) {
        logf_("no BetterPushback external interface (bp/api_version)");
        return;
    }
    pose_from_here(back, side, dh, pose, sizeof (pose));
    (void)snprintf(text, sizeof (text), "BPROUTE 1\n# %s\n%s", name, pose);
    XPLMSetDatab(refs[R_ROUTE_IN].ref, text, 0, (int)strlen(text) + 1);
    cmd = XPLMFindCommand(command);
    if (cmd == NULL) {
        logf_("command %s not found", command);
        return;
    }
    logf_("%s %s: %s", command, name, pose);
    XPLMCommandOnce(cmd);
}

static void
run(const char *command)
{
    XPLMCommandRef cmd = XPLMFindCommand(command);

    if (cmd == NULL) {
        logf_("command %s not found", command);
        return;
    }
    logf_("running %s", command);
    XPLMCommandOnce(cmd);
}

/* ---- commands ---------------------------------------------------------- */

enum {
    C_TOGGLE, C_LOAD_STRAIGHT, C_LOAD_TURN, C_LOAD_TIGHT, C_CHECK_TURN,
    C_CHECK_TIGHT, C_CLEAR, C_SAVE1, C_LOAD1, C_VOICE, C_STALL, C_COUNT
};

static const char *const cmd_names[C_COUNT][2] = {
    {"BPDemo/toggle_window", "Show or hide the BetterPushback API demo"},
    {"BPDemo/load_straight_back", "Load a 40 m straight push"},
    {"BPDemo/load_push_turn_left", "Load a push ending 90 degrees left"},
    {"BPDemo/load_tight_turn", "Load a push too tight to fit"},
    {"BPDemo/check_push_turn_left", "Check the 90 degree push"},
    {"BPDemo/check_tight_turn", "Check the tight push"},
    {"BPDemo/clear_route", "Clear the route"},
    {"BPDemo/save_slot1", "Save the route to slot 1"},
    {"BPDemo/load_slot1", "Load slot 1"},
    {"BPDemo/voice_toggle", "External voice on or off"},
    {"BPDemo/voice_stall", "Stop or restart the external voice heartbeat"}
};
static XPLMCommandRef cmds[C_COUNT];

static int
cmd_cb(XPLMCommandRef cmd, XPLMCommandPhase phase, void *refcon)
{
    int which = (int)(intptr_t)refcon;

    (void)cmd;
    if (phase != xplm_CommandBegin)
        return (1);
    find_refs();
    switch (which) {
    case C_TOGGLE:
        XPLMSetWindowIsVisible(window, !XPLMGetWindowIsVisible(window));
        break;
    case C_LOAD_STRAIGHT:
        send_route("straight back 40 m", "BetterPushback/load_route",
            40, 0, 0);
        break;
    case C_LOAD_TURN:
        send_route("push, end facing 90 degrees left",
            "BetterPushback/load_route", 45, 25, -90);
        break;
    case C_LOAD_TIGHT:
        send_route("too tight", "BetterPushback/load_route", 6, 6, -90);
        break;
    case C_CHECK_TURN:
        send_route("push, end facing 90 degrees left",
            "BetterPushback/check_route", 45, 25, -90);
        break;
    case C_CHECK_TIGHT:
        send_route("too tight", "BetterPushback/check_route", 6, 6, -90);
        break;
    case C_CLEAR:
        run("BetterPushback/clear_route");
        break;
    case C_SAVE1:
        seti(R_ROUTE_SLOT, 1);
        run("BetterPushback/save_route_slot");
        break;
    case C_LOAD1:
        seti(R_ROUTE_SLOT, 1);
        run("BetterPushback/load_route_slot");
        break;
    case C_VOICE:
        seti(R_VOICE_MODE, geti(R_VOICE_MODE) == 1 ? 0 : 1);
        logf_("external voice mode %d", geti(R_VOICE_MODE));
        break;
    case C_STALL:
        stalled = !stalled;
        logf_("heartbeat %s", stalled ? "stopped" : "running");
        break;
    }
    return (1);
}

/* ---- the loop: log changes, play the demo voice ------------------------- */

static float
loop_cb(float since, float since_loop, int counter, void *refcon)
{
    char a[192], b[192], c[192], d[192], e[192];
    float now = XPLMGetElapsedTime();
    int seq;

    (void)since;
    (void)since_loop;
    (void)counter;
    (void)refcon;
    find_refs();
    if (!interface_present())
        return (1.0f);

    seq = geti(R_SEQ);
    if (seq != last_state_seq) {
        last_state_seq = seq;
        logf_("state %d: step %s, stage %s, action %s, blocker %s %s, "
            "paused %d", seq, gets_(R_STEP_NAME, a, sizeof (a)),
            gets_(R_STAGE_NAME, b, sizeof (b)),
            gets_(R_ACTION_NAME, c, sizeof (c)),
            gets_(R_BLOCKER_NAME, d, sizeof (d)),
            gets_(R_BLOCKER_ITEM, e, sizeof (e)), geti(R_PAUSED));
    }
    seq = geti(R_MSG_SEQ);
    if (seq != last_msg_seq) {
        last_msg_seq = seq;
        gets_(R_MSG_TEXT, last_line, sizeof (last_line));
        logf_("line %d (%s, %s, %.1f s, external %d): %s", seq,
            gets_(R_MSG_KEY, a, sizeof (a)), gets_(R_MSG_VOICE, b,
            sizeof (b)), getf(R_MSG_DUR), geti(R_VOICE_EXT), last_line);
        if (geti(R_VOICE_EXT) == 1) {
            /* Speak it (here: pretend) 1.5 times as long as the recording. */
            speaking_seq = seq;
            speaking_until = now + (getf(R_MSG_DUR) > 0 ?
                getf(R_MSG_DUR) * 1.5f : 2.0f);
        }
    }
    seq = geti(R_ROUTE_SEQ);
    if (seq != last_route_seq) {
        last_route_seq = seq;
        (void)snprintf(last_answer, sizeof (last_answer), "%s %s",
            geti(R_ROUTE_STATUS) == 1 ? "accepted" :
            geti(R_ROUTE_STATUS) == 2 ? "REJECTED" : "-",
            gets_(R_ROUTE_REASON, a, sizeof (a)));
        if (seq > 0)
            logf_("route answer %d: %s", seq, last_answer);
    }
    if (geti(R_VOICE_MODE) == 1 && !stalled)
        seti(R_VOICE_HB, ++heartbeat);
    if (speaking_seq != 0 && now >= speaking_until) {
        seti(R_VOICE_DONE, speaking_seq);
        logf_("demo voice finished line %d", speaking_seq);
        speaking_seq = 0;
    }
    return (-1.0f);
}

/* ---- the window --------------------------------------------------------- */

static void
draw_cb(XPLMWindowID id, void *refcon)
{
    float white[] = {1, 1, 1}, green[] = {0.5f, 1, 0.5f};
    char a[192], b[192], c[192], line[256];
    int l, t, r, bo, y;

    (void)refcon;
    XPLMGetWindowGeometry(id, &l, &t, &r, &bo);
    XPLMDrawTranslucentDarkBox(l, t, r, bo);
    y = t - 16;
#define LINE(col, ...) do { (void)snprintf(line, sizeof (line), __VA_ARGS__); \
    XPLMDrawString(col, l + 8, y, line, NULL, xplmFont_Proportional); \
    y -= 14; } while (0)
    find_refs();
    if (!interface_present()) {
        LINE(white, "BetterPushback external interface not found");
        return;
    }
    LINE(green, "BetterPushback %s, interface %d",
        gets_(R_VERSION, a, sizeof (a)), geti(R_API));
    LINE(white, "step %s  stage %s  next %s  paused %d",
        gets_(R_STEP_NAME, a, sizeof (a)), gets_(R_STAGE_NAME, b,
        sizeof (b)), gets_(R_ACTION_NAME, c, sizeof (c)), geti(R_PAUSED));
    LINE(white, "blocker %s %s", gets_(R_BLOCKER_NAME, a, sizeof (a)),
        gets_(R_BLOCKER_ITEM, b, sizeof (b)));
    LINE(white, "status: %s", gets_(R_STATUS, a, sizeof (a)));
    LINE(white, "speed %.1f m/s  distance left %.0f m", getf(R_SPEED),
        getf(R_DIST));
    LINE(green, "line %d: %s", geti(R_MSG_SEQ), last_line);
    LINE(white, "voice: mode %d external %d heartbeat %s done %d",
        geti(R_VOICE_MODE), geti(R_VOICE_EXT), stalled ? "STOPPED" : "ok",
        geti(R_VOICE_DONE));
    LINE(white, "route: source %s  last answer %s",
        gets_(R_ROUTE_SOURCE_NAME, a, sizeof (a)), last_answer);
    LINE(white, "stand %s  slot1 %d  slot2 %d",
        gets_(R_ROUTE_STAND, b, sizeof (b)), geti(R_SLOT1), geti(R_SLOT2));
#undef LINE
}

static int
mouse_cb(XPLMWindowID id, int x, int y, XPLMMouseStatus s, void *refcon)
{
    (void)id; (void)x; (void)y; (void)s; (void)refcon;
    return (1);
}

static XPLMCursorStatus
cursor_cb(XPLMWindowID id, int x, int y, void *refcon)
{
    (void)id; (void)x; (void)y; (void)refcon;
    return (xplm_CursorDefault);
}

static int
wheel_cb(XPLMWindowID id, int x, int y, int wheel, int clicks, void *refcon)
{
    (void)id; (void)x; (void)y; (void)wheel; (void)clicks; (void)refcon;
    return (0);
}

static void
key_cb(XPLMWindowID id, char key, XPLMKeyFlags flags, char vkey, void *refcon,
    int losing)
{
    (void)id; (void)key; (void)flags; (void)vkey; (void)refcon; (void)losing;
}

/* ---- plugin entry points ------------------------------------------------ */

PLUGIN_API int
XPluginStart(char *name, char *sig, char *desc)
{
    XPLMCreateWindow_t w;
    XPLMCreateFlightLoop_t fl = {
        sizeof (fl), xplm_FlightLoop_Phase_AfterFlightModel, loop_cb, NULL
    };

    strcpy(name, "BetterPushback API demo");
    strcpy(sig, "betterpushback.ext_api_demo");
    strcpy(desc, "Example client and test tool for the BetterPushback "
        "external interface");

    memset(&w, 0, sizeof (w));
    w.structSize = sizeof (w);
    w.left = 50;
    w.top = 500;
    w.right = 600;
    w.bottom = 350;
    w.visible = 1;
    w.drawWindowFunc = draw_cb;
    w.handleMouseClickFunc = mouse_cb;
    w.handleKeyFunc = key_cb;
    w.handleCursorFunc = cursor_cb;
    w.handleMouseWheelFunc = wheel_cb;
    w.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;
    w.layer = xplm_WindowLayerFloatingWindows;
    window = XPLMCreateWindowEx(&w);
    XPLMSetWindowTitle(window, "BetterPushback API demo");

    for (int i = 0; i < C_COUNT; i++) {
        cmds[i] = XPLMCreateCommand(cmd_names[i][0], cmd_names[i][1]);
        XPLMRegisterCommandHandler(cmds[i], cmd_cb, 1, (void *)(intptr_t)i);
    }
    loop = XPLMCreateFlightLoop(&fl);
    XPLMScheduleFlightLoop(loop, 1.0f, 1);
    return (1);
}

PLUGIN_API void
XPluginStop(void)
{
    for (int i = 0; i < C_COUNT; i++)
        XPLMUnregisterCommandHandler(cmds[i], cmd_cb, 1, (void *)(intptr_t)i);
    if (loop != NULL)
        XPLMDestroyFlightLoop(loop);
    if (window != NULL)
        XPLMDestroyWindow(window);
}

PLUGIN_API int
XPluginEnable(void)
{
    return (1);
}

PLUGIN_API void
XPluginDisable(void)
{
    /* Hand the voice back when the demo goes away. */
    if (geti(R_VOICE_MODE) == 1)
        seti(R_VOICE_MODE, 0);
}

PLUGIN_API void
XPluginReceiveMessage(XPLMPluginID from, int msg, void *param)
{
    (void)from;
    (void)msg;
    (void)param;
}
