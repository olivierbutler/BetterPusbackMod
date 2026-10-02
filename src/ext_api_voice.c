/*
 * External interface: how long a crew line lasts when another plugin speaks
 * it. Unit tested on its own (tests/ext_api_voice_test.c).
 */

#include "ext_api_voice.h"

double
bp_ext_voice_effective_dur(double recorded, bool external_line, bool alive,
    bool done, double done_after, double so_far)
{
    double longest = recorded + BP_EXT_VOICE_GRACE_S;
    double still_speaking;

    if (!external_line)
        return (recorded);
    if (done)                       /* finished: from then on, as it was */
        return (done_after < longest ? done_after : longest);
    if (!alive)                     /* the external voice went away */
        return (recorded);
    /* Still speaking: always a little more than the time so far, capped. */
    still_speaking = so_far + BP_EXT_VOICE_LEAD_S;
    return (still_speaking < longest ? still_speaking : longest);
}
