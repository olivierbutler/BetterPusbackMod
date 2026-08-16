/*
 * Ground Operations compact rail and interactive workflow panel.
 */

#ifndef _GROUND_OPS_UI_H_
#define _GROUND_OPS_UI_H_

#include <acfutils/types.h>

#ifdef __cplusplus
extern "C" {
#endif

bool_t ground_ops_ui_init(void);
void ground_ops_ui_fini(void);
void ground_ops_ui_reset_context(void);
bool_t ground_ops_ui_is_enabled(void);
bool_t ground_ops_ui_is_visible(void);
void ground_ops_ui_set_captions_enabled(bool_t enabled);
void ground_ops_ui_toggle_visible(void);
void ground_ops_ui_toggle_expanded(void);
void ground_ops_ui_suspend_for_planner(void);
void ground_ops_ui_resume_after_planner(void);

#ifdef __cplusplus
}
#endif

#endif /* _GROUND_OPS_UI_H_ */
