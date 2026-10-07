#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "ext_api_voice.h"

static bool
close_to(double a, double b)
{
    return (fabs(a - b) < 1e-9);
}

static void
test_own_voice_keeps_the_recording_length(void)
{
    /* Whatever the external voice reports, BetterPushback's own line lasts
     * as long as its recording. */
    assert(close_to(bp_ext_voice_effective_dur(3.0, false, true, false, 0, 1.0),
        3.0));
    assert(close_to(bp_ext_voice_effective_dur(3.0, false, true, true, 1.0, 5.0),
        3.0));
}

static void
test_external_line_lasts_until_it_is_reported_finished(void)
{
    /* Still speaking: always ahead of the time so far, so the wait goes on,
     * even past the recording's length. */
    double d = bp_ext_voice_effective_dur(3.0, true, true, false, 0, 1.0);
    assert(d > 1.0 && close_to(d, 1.0 + BP_EXT_VOICE_LEAD_S));
    d = bp_ext_voice_effective_dur(3.0, true, true, false, 0, 6.0);
    assert(d > 6.0);
    /* Finished: the line lasted exactly as long as it was spoken, shorter
     * or longer than the recording. */
    assert(close_to(bp_ext_voice_effective_dur(3.0, true, true, true, 2.2, 9.0),
        2.2));
    assert(close_to(bp_ext_voice_effective_dur(3.0, true, true, true, 4.5, 9.0),
        4.5));
}

static void
test_a_line_is_never_waited_for_beyond_the_grace(void)
{
    double longest = 3.0 + BP_EXT_VOICE_GRACE_S;

    assert(close_to(bp_ext_voice_effective_dur(3.0, true, true, false, 0, 50.0),
        longest));
    assert(close_to(bp_ext_voice_effective_dur(3.0, true, true, true, 60.0, 61.0),
        longest));
}

static void
test_a_silent_external_voice_falls_back_to_the_recording(void)
{
    /* The heartbeat stopped mid-line: wait no longer than the recording. */
    assert(close_to(bp_ext_voice_effective_dur(3.0, true, false, false, 0, 1.0),
        3.0));
    /* A line it did finish keeps its spoken length. */
    assert(close_to(bp_ext_voice_effective_dur(3.0, true, false, true, 2.0, 5.0),
        2.0));
}

int
main(void)
{
    test_own_voice_keeps_the_recording_length();
    test_external_line_lasts_until_it_is_reported_finished();
    test_a_line_is_never_waited_for_beyond_the_grace();
    test_a_silent_external_voice_falls_back_to_the_recording();
    printf("ext_api_voice tests passed\n");
    return (0);
}
