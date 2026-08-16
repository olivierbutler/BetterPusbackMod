/*
 * CDDL HEADER START
 *
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source. A copy is also available in the repository's COPYING file.
 *
 * CDDL HEADER END
 */

#include "wing_walker_logic.h"

wing_walker_signal_t
wing_walker_signal_for_step(pushback_step_t step, bool reconnecting)
{
    /*
     * A reconnect from the final disconnect gate returns directly to the
     * grabbing sequence. Keep the STOP signal displayed while equipment is
     * being reattached, but remove the worker before any resumed push begins.
     */
    if (reconnecting && step >= PB_STEP_GRABBING &&
        step <= PB_STEP_CONNECTED) {
        return (WING_WALKER_SIGNAL_STOP);
    }

    if (step >= PB_STEP_STOPPED && step <= PB_STEP_UNGRABBING)
        return (WING_WALKER_SIGNAL_STOP);

    if (step >= PB_STEP_WAITING4OK2DISCO &&
        step <= PB_STEP_MOVING2CLEAR)
        return (WING_WALKER_SIGNAL_STANDBY);

    if (step == PB_STEP_CLEAR_SIGNAL)
        return (WING_WALKER_SIGNAL_CLEAR);

    return (WING_WALKER_SIGNAL_HIDDEN);
}
