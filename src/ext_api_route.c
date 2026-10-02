/*
 * External interface: reading and writing push routes as text. Unit tested on
 * its own (tests/ext_api_route_test.c).
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ext_api_route.h"

static void
set_reason(char *reason, size_t len, const char *what, int line)
{
    if (reason == NULL || len == 0)
        return;
    if (line > 0)
        (void)snprintf(reason, len, "line %d: %s", line, what);
    else
        (void)snprintf(reason, len, "%s", what);
}

/* Reads one number; true when the whole word is a finite number. */
static bool
read_number(const char *word, double *out)
{
    char *end;
    double v;

    if (word == NULL)
        return (false);
    v = strtod(word, &end);
    if (end == word || *end != '\0' || !isfinite(v))
        return (false);
    *out = v;
    return (true);
}

int
bp_ext_route_parse(const char *text, bp_ext_pose_t *poses, int max,
    char *reason, size_t reason_len)
{
    char line[256];
    const char *p = text;
    int n = 0, lineno = 0;
    bool header = false;

    if (text == NULL) {
        set_reason(reason, reason_len, "no route text", 0);
        return (-1);
    }
    while (*p != '\0') {
        size_t len = strcspn(p, "\r\n");
        char *words[6] = { NULL };
        int nwords = 0;

        lineno++;
        if (len >= sizeof (line)) {
            set_reason(reason, reason_len, "line too long", lineno);
            return (-1);
        }
        memcpy(line, p, len);
        line[len] = '\0';
        p += len;
        while (*p == '\r' || *p == '\n')
            p++;

        for (char *w = strtok(line, " \t"); w != NULL && nwords < 6;
            w = strtok(NULL, " \t"))
            words[nwords++] = w;
        if (nwords == 0 || words[0][0] == '#')
            continue;

        if (!header) {
            double version;
            if (strcmp(words[0], "BPROUTE") != 0 || nwords != 2 ||
                !read_number(words[1], &version)) {
                set_reason(reason, reason_len,
                    "the route must start with \"BPROUTE 1\"", lineno);
                return (-1);
            }
            if (version != BP_EXT_ROUTE_FORMAT) {
                set_reason(reason, reason_len,
                    "unsupported route format version", lineno);
                return (-1);
            }
            header = true;
            continue;
        }
        if (strcmp(words[0], "P") != 0 || nwords < 4 || nwords > 5) {
            set_reason(reason, reason_len,
                "expected \"P <latitude> <longitude> <heading>\"", lineno);
            return (-1);
        }
        if (n >= max) {
            set_reason(reason, reason_len, "too many positions", lineno);
            return (-1);
        }
        if (!read_number(words[1], &poses[n].lat) ||
            !read_number(words[2], &poses[n].lon) ||
            !read_number(words[3], &poses[n].hdg)) {
            set_reason(reason, reason_len, "not a number", lineno);
            return (-1);
        }
        if (fabs(poses[n].lat) > 90 || fabs(poses[n].lon) > 180) {
            set_reason(reason, reason_len,
                "latitude or longitude out of range", lineno);
            return (-1);
        }
        if (poses[n].hdg < 0 || poses[n].hdg > 360) {
            set_reason(reason, reason_len,
                "heading must be 0 to 360 degrees", lineno);
            return (-1);
        }
        if (poses[n].hdg == 360)
            poses[n].hdg = 0;
        poses[n].backward = false;
        n++;
    }
    if (!header) {
        set_reason(reason, reason_len,
            "the route must start with \"BPROUTE 1\"", 0);
        return (-1);
    }
    if (n == 0) {
        set_reason(reason, reason_len, "the route has no positions", 0);
        return (-1);
    }
    return (n);
}

bool
bp_ext_route_format(const bp_ext_pose_t *poses, int n, char *text,
    size_t text_len)
{
    size_t used;
    int w;

    if (text == NULL || text_len == 0)
        return (false);
    w = snprintf(text, text_len, "BPROUTE %d\n", BP_EXT_ROUTE_FORMAT);
    if (w < 0 || (size_t)w >= text_len)
        return (false);
    used = (size_t)w;
    for (int i = 0; i < n; i++) {
        w = snprintf(text + used, text_len - used, "P %.8f %.8f %.2f %s\n",
            poses[i].lat, poses[i].lon, poses[i].hdg,
            poses[i].backward ? "push" : "tow");
        if (w < 0 || (size_t)w >= text_len - used) {
            text[0] = '\0';
            return (false);
        }
        used += (size_t)w;
    }
    return (true);
}

const char *
bp_ext_route_change_refused(const bp_ext_route_gate_t *gate)
{
    if (!gate->ready)
        return ("BetterPushback cannot work with this aircraft");
    if (gate->slave_mode)
        return ("the route comes from the other cockpit (shared cockpit)");
    if (gate->planner_open)
        return ("the planner is open");
    if (gate->manual_push)
        return ("a manual push is in progress");
    if (!gate->started || gate->awaiting_plan || gate->can_replan)
        return (NULL);
    return ("the route cannot change now: the tug is connecting or the push "
        "is under way");
}
