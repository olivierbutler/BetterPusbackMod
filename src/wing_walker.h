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

#ifndef _WING_WALKER_H_
#define _WING_WALKER_H_

#include <acfutils/geom.h>
#include <acfutils/types.h>

#include "pushback_step.h"

typedef struct wing_walker wing_walker_t;

#define WING_WALKER_SIGNAL_DATAREF "bp/anim/wing_walker_signal"

wing_walker_t *wing_walker_alloc(const char *object_path);
void wing_walker_free(wing_walker_t *walker);

void wing_walker_update(wing_walker_t *walker, vect2_t aircraft_pos,
    double aircraft_heading, double aircraft_nose_forward,
    pushback_step_t step, bool_t reconnecting);

#endif /* _WING_WALKER_H_ */
