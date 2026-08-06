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

#ifndef _WING_WALKER_LOGIC_H_
#define _WING_WALKER_LOGIC_H_

#include <stdbool.h>

#include "pushback_step.h"

typedef enum {
    WING_WALKER_SIGNAL_HIDDEN = 0,
    WING_WALKER_SIGNAL_STOP = 1,
    WING_WALKER_SIGNAL_STANDBY = 2,
    WING_WALKER_SIGNAL_CLEAR = 3
} wing_walker_signal_t;

wing_walker_signal_t wing_walker_signal_for_step(pushback_step_t step,
    bool reconnecting);

#endif /* _WING_WALKER_LOGIC_H_ */
