/*
 * Shared lifetime for the plugin's XPImgWindow/ImGui font resources.
 */

#ifndef _UI_RUNTIME_H_
#define _UI_RUNTIME_H_

#include <acfutils/types.h>

#ifdef __cplusplus
extern "C" {
#endif

bool_t bp_ui_runtime_init(void);
void bp_ui_runtime_cleanup(void);
bool_t bp_ui_runtime_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* _UI_RUNTIME_H_ */
