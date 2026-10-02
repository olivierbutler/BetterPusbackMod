/*
 * External interface for other plugins (ATC, crew and copilot add-ons):
 * datarefs and commands described in README-EXTERNAL-API.md.
 */

#ifndef _EXT_API_H_
#define _EXT_API_H_

#include <acfutils/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Creates the datarefs and starts publishing; called from XPluginStart. */
void ext_api_init(void);
/* Stops publishing and removes the datarefs; called from XPluginStop. */
void ext_api_fini(void);

#ifdef __cplusplus
}
#endif

#endif /* _EXT_API_H_ */
