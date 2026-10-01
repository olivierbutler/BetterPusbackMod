/*
 * Runtime pushback-interface selection helpers. This file deliberately has
 * no X-Plane dependencies so its defaulting and exclusivity rules can be
 * covered by the offline regression suite.
 */

#include "interface_mode.h"

bool
bp_interface_mode_valid(int mode)
{
    return (mode == BP_INTERFACE_MODE_GROUND_OPS ||
        mode == BP_INTERFACE_MODE_LEGACY_MAGIC_SQUARES);
}

bp_interface_mode_t
bp_interface_mode_normalize(int mode)
{
    if (!bp_interface_mode_valid(mode))
        return (BP_INTERFACE_MODE_GROUND_OPS);
    return ((bp_interface_mode_t)mode);
}

bool
bp_interface_mode_uses_ground_ops(bp_interface_mode_t mode)
{
    return (bp_interface_mode_normalize(mode) ==
        BP_INTERFACE_MODE_GROUND_OPS);
}

bool
bp_interface_mode_uses_legacy_magic_squares(bp_interface_mode_t mode)
{
    return (bp_interface_mode_normalize(mode) ==
        BP_INTERFACE_MODE_LEGACY_MAGIC_SQUARES);
}
