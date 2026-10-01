#ifndef BP_HANDLING_TIMING_H
#define BP_HANDLING_TIMING_H

#include <stdbool.h>

static inline double
bp_handling_duration(double seconds, bool fast)
{
    return fast ? 0.0 : seconds;
}

static inline double
bp_handling_fraction(double elapsed, double duration, bool fast)
{
    return fast ? 1.0 : elapsed / duration;
}

#endif
