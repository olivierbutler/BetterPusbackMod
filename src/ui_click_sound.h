#ifndef BP_UI_CLICK_SOUND_H
#define BP_UI_CLICK_SOUND_H
void bp_ui_click_init();
void bp_ui_click_play();
double bp_ui_click_get_volume();
/* Main-thread only. Applies immediately; saved by Save preferences. */
void bp_ui_click_set_volume(double volume);
void bp_ui_click_fini();
#endif
