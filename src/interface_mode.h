/*
 * Runtime selection between the fork's Ground Operations interface and the
 * original BetterPushback magic-squares interface.
 */

#ifndef _BP_INTERFACE_MODE_H_
#define _BP_INTERFACE_MODE_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BP_INTERFACE_MODE_CONFIG_KEY "pushback_interface_mode"

typedef enum {
    BP_INTERFACE_MODE_GROUND_OPS = 0,
    BP_INTERFACE_MODE_LEGACY_MAGIC_SQUARES = 1
} bp_interface_mode_t;

bool bp_interface_mode_valid(int mode);
bp_interface_mode_t bp_interface_mode_normalize(int mode);
bool bp_interface_mode_uses_ground_ops(bp_interface_mode_t mode);
bool bp_interface_mode_uses_legacy_magic_squares(bp_interface_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* _BP_INTERFACE_MODE_H_ */
