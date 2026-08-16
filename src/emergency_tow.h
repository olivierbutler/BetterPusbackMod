/*
 * Emergency Tow is a small session coordinator. It never owns tug motion,
 * planner geometry, or persistent route data; those remain with the existing
 * BetterPushback controllers.
 */

#ifndef _EMERGENCY_TOW_H_
#define _EMERGENCY_TOW_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void emergency_tow_reset(void);
bool emergency_tow_begin(void);
bool emergency_tow_is_active(void);
bool emergency_tow_claim_planner_launch(void);
bool emergency_tow_finish(void);

bool emergency_tow_allows_persistent_routes(void);
bool emergency_tow_allows_wing_walker(void);

#ifdef __cplusplus
}
#endif

#endif /* _EMERGENCY_TOW_H_ */
