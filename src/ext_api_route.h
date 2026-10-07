/*
 * External interface: push routes exchanged with other plugins as text
 * (README-EXTERNAL-API.md, "External routes"). Pure parsing and formatting,
 * no XPLM dependencies.
 *
 * A route is the list of positions and true headings the aircraft should
 * reach, one leg after another, starting from where it stands. The positions
 * are the aircraft's reference point (the latitude and longitude X-Plane
 * reports for the aircraft); BetterPushback fits each leg to the aircraft's
 * turning limits, pushing to a target behind it and towing to one ahead.
 *
 *     BPROUTE 1
 *     P 47.7931234 12.9975123 180.0
 *     P 47.7924000 12.9969000 270.0 push
 *
 * Lines starting with '#' and blank lines are ignored. A fifth word on a "P"
 * line (push or tow, written in published routes) is ignored when reading.
 */

#ifndef _EXT_API_ROUTE_H_
#define _EXT_API_ROUTE_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BP_EXT_ROUTE_FORMAT 1          /* the number after BPROUTE */
#define BP_EXT_ROUTE_MAX_POSES 16
#define BP_EXT_ROUTE_TEXT_LEN 2048
#define BP_EXT_ROUTE_REASON_LEN 128

typedef struct {
    double lat;
    double lon;
    double hdg;                        /* true heading, 0 <= hdg < 360 */
    bool backward;                     /* written routes only: a push leg */
} bp_ext_pose_t;

/*
 * Reads a route. Returns the number of poses (at least 1), or -1 with the
 * reason in `reason`.
 */
int bp_ext_route_parse(const char *text, bp_ext_pose_t *poses, int max,
    char *reason, size_t reason_len);

/* Writes a route (with push/tow words); returns false if it does not fit. */
bool bp_ext_route_format(const bp_ext_pose_t *poses, int n, char *text,
    size_t text_len);

/* What decides whether another plugin may change the route now. */
typedef struct {
    bool ready;             /* BetterPushback works with this aircraft */
    bool slave_mode;        /* shared cockpit: the other cockpit plans */
    bool planner_open;
    bool manual_push;
    bool started;           /* an operation is running */
    bool awaiting_plan;     /* the connected tug waits for a plan */
    bool can_replan;        /* the connected hold, parking brake set */
} bp_ext_route_gate_t;

/*
 * NULL when the route may be changed (loaded, cleared, a slot loaded): before
 * an operation, while the connected tug waits for a plan, and during the
 * connected hold. Otherwise why not, in words.
 */
const char *bp_ext_route_change_refused(const bp_ext_route_gate_t *gate);

#ifdef __cplusplus
}
#endif

#endif /* _EXT_API_ROUTE_H_ */
