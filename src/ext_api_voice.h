/*
 * External interface: another plugin speaking the ground-crew lines
 * (README-EXTERNAL-API.md, "External voice"). Pure timing rules, no XPLM or
 * audio dependencies.
 */

#ifndef _EXT_API_VOICE_H_
#define _EXT_API_VOICE_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The external voice counts as alive while its heartbeat moved this recently. */
#define BP_EXT_VOICE_ALIVE_S 3.0
/* An external line is waited for at most its recording's length plus this. */
#define BP_EXT_VOICE_GRACE_S 8.0
/* While an external line is still being spoken, its effective length runs this
 * far ahead of the time spoken so far. */
#define BP_EXT_VOICE_LEAD_S 1.0

/*
 * How long the operation treats a crew line as lasting, in seconds. The
 * controller waits for this where it used to wait for the recording.
 *   recorded       length of BetterPushback's own recording
 *   external_line  the line was handed to an external voice
 *   alive          the external voice's heartbeat is current
 *   done           the external voice reported the line finished ...
 *   done_after     ... this many seconds after it started
 *   so_far         seconds since the line started
 */
double bp_ext_voice_effective_dur(double recorded, bool external_line,
    bool alive, bool done, double done_after, double so_far);

#ifdef __cplusplus
}
#endif

#endif /* _EXT_API_VOICE_H_ */
