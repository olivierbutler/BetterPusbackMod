#include <assert.h>
#include <stdio.h>

#include "interface_mode.h"

static void
test_valid_modes_are_mutually_exclusive(void)
{
    for (int mode = BP_INTERFACE_MODE_GROUND_OPS;
        mode <= BP_INTERFACE_MODE_LEGACY_MAGIC_SQUARES; mode++) {
        bp_interface_mode_t normalized = bp_interface_mode_normalize(mode);
        bool ground_ops = bp_interface_mode_uses_ground_ops(normalized);
        bool legacy = bp_interface_mode_uses_legacy_magic_squares(normalized);

        assert(bp_interface_mode_valid(mode));
        assert(ground_ops != legacy);
    }
}

static void
test_missing_or_invalid_values_default_to_ground_ops(void)
{
    const int invalid_values[] = {-1, 2, 99};

    for (size_t index = 0;
        index < sizeof (invalid_values) / sizeof (invalid_values[0]); index++) {
        bp_interface_mode_t mode = bp_interface_mode_normalize(
            invalid_values[index]);

        assert(mode == BP_INTERFACE_MODE_GROUND_OPS);
        assert(bp_interface_mode_uses_ground_ops(mode));
        assert(!bp_interface_mode_uses_legacy_magic_squares(mode));
    }
}

int
main(void)
{
    test_valid_modes_are_mutually_exclusive();
    test_missing_or_invalid_values_default_to_ground_ops();
    puts("pushback interface mode tests passed");
    return (0);
}
