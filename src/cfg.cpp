/*
 * CDDL HEADER START
 *
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2022 Saso Kiselkov. All rights reserved.
 * Copyright 2024 Robert Wellinger. All rights reserved.
 */

#include <curl/curl.h>
#include <errno.h>
#include <string.h>

#include <XPLMGraphics.h>
#include <XPLMPlanes.h>
#include <XPLMPlugin.h>
#include <XPStandardWidgets.h>
#include <XPWidgets.h>

#include <acfutils/assert.h>
#include <acfutils/dr.h>
#include <acfutils/intl.h>

#include "bp.h"
#include "cfg.h"
#include "msg.h"
#include "xplane.h"

#include "xp_img_window.h"

#define GITURL                                                                 \
  "https://api.github.com/repos/olivierbutler/BetterPusbackMod/releases/"      \
  "latest"

#define DL_TIMEOUT 5L /* seconds */
#define MAX_VERSION_BF_SIZE 32000

#define CONF_FILENAME "BetterPushback.cfg"
#define CONF_DIRS bp_xpdir, "Output", "preferences"
#define MISC_FILENAME "Miscellaneous.prf"
#define XP_PREF_WINDOWS "X-Plane Window Positions.prf"
conf_t *bp_conf = NULL;

static bool_t inited = B_FALSE;
static bool_t gui_inited = B_FALSE;
bool_t setup_view_callback_is_alive = B_FALSE;

#define MAIN_WINDOW_W 800
#define MAIN_WINDOW_H 700

#define ROUNDED 8.0f
#define TOOLTIP_BG_COLOR ImVec4(0.2f, 0.3f, 0.8f, 1.0f)
#define LINE_COLOR IM_COL32(255, 255, 255, 255)
#define LINE_THICKNESS 1.0f
#define BUTTON_DISABLED 0.5f

#define MONITOR_AUTO -1

#define COPYRIGHT1                                                             \
  "BetterPushback " BP_PLUGIN_VERSION                                          \
  "       © 2017-2024 S.Kiselkov, Robwell, O.Butler. All rights reserved."
#define COPYRIGHT2                                                             \
  "BetterPushback is open-source software. See COPYING for more information."

/* Warning this is used on PO Translation as ID */
#define TOOLTIP_HINT                                                           \
  "Hint: hover your mouse cursor over any label to show a short description "  \
  "of what it does."

static fov_t fov_values = {0};
static struct {
  dr_t fov_h_deg;
  dr_t fov_h_ratio;
  dr_t fov_roll;
  dr_t fov_v_deg;
  dr_t fov_v_ratio;
  dr_t ui_scale;
} drs;

static struct {
  bool_t new_version_available;
  char version[MAX_VERSION_BF_SIZE];
} gitHubVersion;

struct curl_memory {
  char *response;
  size_t size;
};

monitors_t monitor_def = {0};
// by default we have only 1 monitor with org (0,0)
// later we will set it with the first monitor found or
// with the one selected in the preferences

void get_fov_values_impl(fov_t *values);

void set_fov_values_impl(fov_t *values);

void fetchGitVersion(void);

const char *crew_language_tooltip =
    "My language only at domestic airports:\n"
    "Ground crew speaks my language only if the country the airport is "
    "in speaks my language. Otherwise the ground crew speaks English "
    "with a local accent.\n\n"
    "My language at all airports:\n"
    "Ground crew speaks my language irrespective "
    "of what country the airport is in.\n\n"
    "English at all airports:\n"
    "Ground crew always speaks English "
    "with a local accent.";

const char *dev_menu_tooltip = "Show the developer menu options.";
const char *save_prefs_tooltip = "Save current preferences to disk.";
const char *disco_when_done_tooltip =
    "Never ask and always automatically disconnect "
    "the tug when the pushback operation is complete.";
const char *per_aircraft_is_global_tooltip =
    "When enabled, all per aircraft settings becomes global.";
const char *always_connect_tug_first_tooltip =
    "The push process is always halted when the tug is at the nose of the "
    "aircraft.\n"
    "The process will proceed by triggering again the 'start pushback' "
    "command.";
const char *tug_starts_next_plane_tooltip =
    "The tug appears next to the plane avoiding in certain case that he is "
    "travelling inside the buildings.";
const char *tug_auto_start_tooltip =
    "The tug will appear once the beacon light is switched from off to on "
    "then the process will proceed as 'connect the tug first'.";
const char *ignore_park_brake_tooltip =
    "Never check \"set parking brake\".\n"
    "Some aircraft stuck on this check.\n"
    "It's on the beginning and on the end.\n"
    "This should solve this problem for some aircrafts. (KA350 for instance).";
const char *hide_xp11_tug_tooltip =
    "Hides default X-Plane 11 pushback tug.\n"
    "Restart X-Plane for this change to take effect.";
const char *hide_magic_squares_tooltip =
    "Hides the shortcut buttons on the left side of the screen.\n"
    "The first button starts the planner and the second starts the push-back.";
const char *doors_check_tooltip =
    "Select the action after the doors/GPU/ASU status check is done before starting the push-back.\n"
    "Active with ground crew message: The ground crew will tell you if the doors are not closed.\n"
    "Active, but stay silent: The ground crew will wait, but not say anything if the doors are not closed.\n"
    "Ignore: The doors/GPU/ASU status check is ignored and the push-back starts immediately, even if the doors are not closed or the GPU/ASU is still connected.";

const char *monitor_tooltip =
    "In case of multiple monitors configuration, BpB need to use the primary "
    "monitor\n"
    "(the one with the x-plane menus), Bpb is able to select it "
    "automatically.\n"
    "If not select the one that works ! (the monitor numbers are arbitrary).";

const char *magic_squares_height_tooltip =
    "Slide this bar to move the magic squares up or down.";

const char *eye_tracker_tooltip =
    "Some Eye tracker plugins use the X-plane camera system and therefore "
    "doesn't allow BpB to work properly "
    "while using the planner or the tug view.\n\n"
    "If it is the case, select the plugin that is in conflit with BpB.\n\n"
    "CAUTION: At this point, you know what you are doing, selecting the wrong "
    "plugin may cause X-plane to crash.\n"
    "Otherwise this setting must be set to : None.";

typedef struct {
  const char *string;
  bool use_chinese;
  const char *value;
} comboList_t_;

typedef struct {
  comboList_t_ *combo_list;
  int list_size;
  const char *name;
  int selected;
} comboList_t;

comboList_t_ language_list_[] = {{_("X-Plane's language"), B_FALSE, "xp_l"},
                                 {"Deutsch", B_FALSE, "de"},
                                 {"English", B_FALSE, "en"},
                                 {"Español", B_FALSE, "es"},
                                 {"Italiano", B_FALSE, "it"},
                                 {"Français", B_FALSE, "fr"},
                                 {"Português", B_FALSE, "pt"},
                                 {"Português do Brasil", B_FALSE, "pt_BR"},
                                 {"Русский", B_FALSE, "ru"},
                                 {"中文", B_TRUE, "cn"}};

comboList_t language_list = {language_list_, IM_ARRAYSIZE(language_list_),
                             "##lang_list", 0};

comboList_t_ crew_lang_list_[] = {
    {"My language only at domestic airports", B_FALSE, "0"},
    {"My language at all airports", B_FALSE, "1"},
    {"English at all airports", B_FALSE, "2"}};

comboList_t crew_lang_list = {crew_lang_list_, IM_ARRAYSIZE(crew_lang_list_),
                              "##crew_lang_list", 0};

#define MAX_MONITOR_COUNT 7
comboList_t_ monitor_list_[MAX_MONITOR_COUNT] = {
    {"Automatic", B_FALSE, "0"},  {"Monitor #0", B_FALSE, "1"},
    {"Monitor #1", B_FALSE, "1"}, {"Monitor #2", B_FALSE, "1"},
    {"Monitor #3", B_FALSE, "1"}, {"Monitor #4", B_FALSE, "1"},
    {"Monitor #5", B_FALSE, "2"}};

comboList_t monitor_list = {monitor_list_, 0, "##monitor_list", 0};

comboList_t_ *radio_device_list_ = nullptr;
comboList_t radio_device_list = {radio_device_list_, 0, "##radio_device_list",
                                 0};

comboList_t_ *sound_device_list_ = nullptr;
comboList_t sound_device_list = {sound_device_list_, 0, "##sound_device_list",
                                 0};

comboList_t_ *plg_list_ = nullptr;
comboList_t plg_list = {plg_list_, 0, "##plg_list", 0};

comboList_t_ *doors_check_list_ = nullptr;
comboList_t doors_check_list = {doors_check_list_, 0, "##doors_check", 0};

class SettingsWindow : public XPImgWindow {
public:
  SettingsWindow(WndMode _mode = WND_MODE_FLOAT_CENTERED);
  void CenterText(const char *text);
  bool CenterButton(const char *text);
  void Tooltip(const char *tip);
  bool_t comboList(comboList_t *list);
  bool_t save_disabled;

  ~SettingsWindow() {
    comboList_free(&radio_device_list);
    comboList_free(&sound_device_list);
    comboList_free(&plg_list);
    comboList_free(&doors_check_list);
  }

  bool_t getIsDestroy(void) { return is_destroy; }

private:
  const char *lang;
  bool_t is_chinese;
  lang_pref_t lang_pref;
  bool_t disco_when_done;
  bool_t ignore_park_brake;
  bool_t hide_magic_squares;
  bool_t dont_hide;
  bool_t always_connect_tug_first;
  bool_t per_aircraft_is_global;
  bool_t xp11_only;
  bool_t is_destroy;
  bool_t tug_starts_next_plane;
  bool_t tug_auto_start;
  int monitor_id;
  int for_credit;
  int magic_squares_height;
  int doors_check;
  const char *radio_dev, *sound_dev, *plg_to_exclude;
  void LoadConfig(void);
  void sound_comboList_init(comboList_t *list);
  void plugin_comboList_init(comboList_t *list);
  void doorscheck_comboList_init(comboList_t *list);
  void comboList_free(comboList_t *list);
  void initPerAircraftSettings(void);

protected:
  void buildInterface() override;
};

SettingsWindow::SettingsWindow(WndMode _mode)
    : XPImgWindow(_mode, WND_STYLE_SOLID,
                  WndRect(0, MAIN_WINDOW_W, MAIN_WINDOW_H, 0)),
      is_destroy(B_FALSE), for_credit(0), save_disabled(B_FALSE) {
  SetWindowTitle(_("BetterPushback Preferences"));
  SetWindowResizingLimits(MAIN_WINDOW_W, MAIN_WINDOW_H, MAIN_WINDOW_W,
                          MAIN_WINDOW_H);
  LoadConfig();
  setup_view_callback_is_alive = B_TRUE;
}

void SettingsWindow::initPerAircraftSettings(void) {
  disco_when_done = B_FALSE;
  (void)conf_get_b_per_acf((char *)"disco_when_done", &disco_when_done);

  ignore_park_brake = B_FALSE;
  (void)conf_get_b_per_acf((char *)"ignore_park_brake", &ignore_park_brake);

  doors_check = DOOR_CHECK_ActiveWithMessage;
  (void)conf_get_i_per_acf((char *)"doors_check", &doors_check);

  hide_magic_squares = B_FALSE;
  (void)conf_get_b_per_acf((char *)"hide_magic_squares", &hide_magic_squares);

  magic_squares_height = 50;
  (void)conf_get_i_per_acf((char *)"magic_squares_height",
                           &magic_squares_height);

}
void SettingsWindow::LoadConfig(void) {

  lang = NULL;
  language_list.selected = 0;
  is_chinese = B_FALSE;

  if (conf_get_str(bp_conf, "lang", &lang)) {
    for (int i = 0; i < language_list.list_size; i++) {
      if ((strcmp(lang, language_list.combo_list[i].value) == 0)) {
        language_list.selected = i;
        break;
      }
    }
    is_chinese = (strcmp(lang, "cn") == 0);
  }

  lang_pref = LANG_PREF_MATCH_REAL;
  conf_get_i(bp_conf, "lang_pref", (int *)&lang_pref);
  crew_lang_list.selected = lang_pref;

  initPerAircraftSettings();

  per_aircraft_is_global = B_FALSE;
  (void)conf_get_b(bp_conf, "per_aircraft_is_global", &per_aircraft_is_global);

  xp11_only = (bp_xp_ver >= 11000 && bp_xp_ver < 12000);
  dont_hide = B_FALSE;
  (void)conf_get_b(bp_conf, "dont_hide_xp11_tug", &dont_hide);

  always_connect_tug_first = B_FALSE;
  (void)conf_get_b(bp_conf, "always_connect_tug_first",
                   &always_connect_tug_first);

  tug_starts_next_plane = B_FALSE;
  (void)conf_get_b(bp_conf, "tug_starts_next_plane", &tug_starts_next_plane);

// feature disabled for now
//  tug_auto_start = B_FALSE;
//  (void)conf_get_b(bp_conf, "tug_auto_start", &tug_auto_start);

  initMonitorOrigin();

  monitor_list.list_size =
      monitor_def.monitor_count <= 1 ? 0 : monitor_def.monitor_count + 1;
  monitor_list.selected = 0;
  monitor_id = MONITOR_AUTO;
  (void)conf_get_i(bp_conf, "monitor_id", &monitor_id);
  if (monitor_id > MONITOR_AUTO) {
    monitor_list.selected = monitor_id + 1;
  }
  if (monitor_list.selected > MAX_MONITOR_COUNT) {
    monitor_list.selected = 0;
  }

  radio_dev = NULL;
  sound_comboList_init(&radio_device_list);
  radio_device_list.selected = 0;
  if (conf_get_str(bp_conf, "radio_device", &radio_dev)) {
    for (int i = 0; i < radio_device_list.list_size; i++) {
      if ((strcmp(radio_dev, radio_device_list.combo_list[i].value) == 0)) {
        radio_device_list.selected = i;
        break;
      }
    }
  }

  sound_dev = NULL;
  sound_comboList_init(&sound_device_list);
  sound_device_list.selected = 0;
  if (conf_get_str(bp_conf, "sound_device", &sound_dev)) {
    for (int i = 0; i < sound_device_list.list_size; i++) {
      if ((strcmp(sound_dev, sound_device_list.combo_list[i].value) == 0)) {
        sound_device_list.selected = i;
        break;
      }
    }
  }

  plg_to_exclude = NULL;
  plugin_comboList_init(&plg_list);
  plg_list.selected = 0;
  if (conf_get_str(bp_conf, "plg_to_exclude", &plg_to_exclude)) {
    for (int i = 0; i < plg_list.list_size; i++) {
      if ((strcmp(plg_to_exclude, plg_list.combo_list[i].value) == 0)) {
        plg_list.selected = i;
        break;
      }
    }
  }

  doorscheck_comboList_init(&doors_check_list);
  doors_check_list.selected = doors_check;
}

void SettingsWindow::plugin_comboList_init(comboList_t *list) {
  int num_plg = XPLMCountPlugins();
  XPLMPluginID plg_id;
  char plg_name[256] = {0};
  char plg_signature[256] = {0};
  char path[1024] = {0};
  int num_resource_plg = 1;

  list->combo_list =
      (comboList_t_ *)safe_calloc(num_plg + 1, sizeof(comboList_t_));
  list->list_size = num_plg + 1;
  list->combo_list[0].string = strdup(_("None"));
  list->combo_list[0].use_chinese = B_FALSE;
  list->combo_list[0].value = strdup(list->combo_list[0].string);

  for (int i = 0; i < num_plg; i++) {
    plg_id = XPLMGetNthPlugin(i);
    XPLMGetPluginInfo(plg_id, plg_name, path, plg_signature, NULL);
    // excluding pluginadmin and Navigraph and Bpb !!!!!
    if ((strstr(plg_signature, "pluginadmin") != NULL) ||
        (strstr(plg_signature, BP_PLUGIN_SIG) != NULL) ||
        (strstr(plg_signature, "skiselkov.xraas2") != NULL) ||
        (strstr(plg_signature, "Navigraph") != NULL)) {
      continue;
    }
    if (strstr(path, "Resources/plugins/") != NULL) {
      list->combo_list[num_resource_plg].string = strdup(plg_name);
      list->combo_list[num_resource_plg].use_chinese = B_FALSE;
      list->combo_list[num_resource_plg].value = strdup(plg_signature);
      num_resource_plg++;
    }
  }
  list->list_size = num_resource_plg;
}

void SettingsWindow::sound_comboList_init(comboList_t *list) {
  size_t num_devs;
  char **devs = openal_list_output_devs(&num_devs);

  list->combo_list =
      (comboList_t_ *)safe_calloc(num_devs + 1, sizeof(comboList_t_));
  list->list_size = num_devs + 1;
  list->combo_list[0].string = strdup(_("Default output device"));
  list->combo_list[0].use_chinese = B_FALSE;
  list->combo_list[0].value = strdup(list->combo_list[0].string);

  for (size_t i = 1; i < (num_devs + 1); i++) {
    list->combo_list[i].string = strdup(devs[i - 1]);
    list->combo_list[i].use_chinese = B_FALSE;
    list->combo_list[i].value = strdup(list->combo_list[i].string);
  }
}

void SettingsWindow::doorscheck_comboList_init(comboList_t *list) {
  // Options for doors/GPU/ASU check
  list->combo_list = (comboList_t_ *)safe_calloc(3, sizeof(comboList_t_));
  list->list_size = 3;

  // These should follow the order of the DoorCheck enum
  static const char* doors_check_strings[] = {
    _("Active with ground crew message"),
    _("Active, but stay silent"),
    _("Ignore")
  };

  for (int i = 0; i < 3; ++i) {
    list->combo_list[i].string = strdup(doors_check_strings[i]);
    list->combo_list[i].use_chinese = B_FALSE;
    list->combo_list[i].value = strdup("");
  }
}

void SettingsWindow::comboList_free(comboList_t *list) {
  for (size_t i = 0; i < list->list_size; i++) {
    free((void *)list->combo_list[i].string);
    free((void *)list->combo_list[i].value);
  }
  free((void *)list->combo_list);
  list->list_size = 0;
}

bool_t SettingsWindow::comboList(comboList_t *list) {
  bool_t is_changed = B_FALSE;
  comboList_t_ combo_previous = list->combo_list[list->selected];
  if (combo_previous.use_chinese) {
    ImGui::PushFont(ImgWindow::fontChinese.get());
  }

  //  ImGui::SetNextItemWidth(360.0f); // Width in pixels

  bool_t lang_combo =
      ImGui::BeginCombo(list->name, _(combo_previous.string), 0);
  if (combo_previous.use_chinese) {
    ImGui::PopFont();
  }
  if (lang_combo) {
    for (int i = 0; i < list->list_size; i++) {
      int is_selected = (i == list->selected);
      if (list->combo_list[i].use_chinese) {
        ImGui::PushFont(ImgWindow::fontChinese.get());
      }
      if (ImGui::Selectable(_(list->combo_list[i].string), is_selected)) {
        is_changed = (list->selected != i);
        list->selected = i;
      }
      if (list->combo_list[i].use_chinese) {
        ImGui::PopFont();
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  return is_changed;
}

void SettingsWindow::CenterText(const char *text) {
  ImVec2 windowSize = ImGui::GetWindowSize();
  ImVec2 textSize = ImGui::CalcTextSize(text);

  // Calculate horizontal position to center the text
  float textPosX = (windowSize.x - textSize.x) * 0.5f;

  // Make sure it doesn't go out of bounds
  if (textPosX > 0.0f)
    ImGui::SetCursorPosX(textPosX);

  ImGui::Text("%s", text);
}

bool SettingsWindow::CenterButton(const char *text) {
  ImVec2 windowSize = ImGui::GetWindowSize();
  ImVec2 textSize = ImGui::CalcTextSize(text);

  // Calculate horizontal position to center the text
  float textPosX = (windowSize.x - textSize.x) * 0.5f;

  // Make sure it doesn't go out of bounds
  if (textPosX > 0.0f)
    ImGui::SetCursorPosX(textPosX);

  return ImGui::Button(text);
}

void SettingsWindow::Tooltip(const char *tip) {
  // do the tooltip
  if (tip && ImGui::IsItemHovered()) {
    ImGui::PushStyleColor(ImGuiCol_PopupBg, TOOLTIP_BG_COLOR);
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(300);
    ImGui::TextUnformatted(tip);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
    ImGui::PopStyleColor();
  }
}
void SettingsWindow::buildInterface() {
  if (is_chinese) {
    ImGui::PushFont(ImgWindow::fontChinese.get());
  }

  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ROUNDED);
  float tableWidth = ImGui::GetContentRegionAvail().x;
  float combowithWidth = tableWidth * 0.45f;

  if (ImGui::BeginTable("##main_table", 2, ImGuiTableFlags_SizingStretchSame,
                        ImVec2(tableWidth, 0))) {

    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    ImGui::Text("%s", _("User interface"));

    if (is_chinese) {
      ImGui::PopFont();
    }
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(combowithWidth);
    if (comboList(&language_list)) {
      conf_set_str(bp_conf, "lang",
                   language_list.combo_list[language_list.selected].value);
    }
    if (is_chinese) {
      ImGui::PushFont(ImgWindow::fontChinese.get());
    }
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    ImGui::Text("%s", _("Ground crew audio"));
    Tooltip(_(crew_language_tooltip));

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(combowithWidth);
    if (comboList(&crew_lang_list)) {
      conf_set_i(bp_conf, "lang_pref", crew_lang_list.selected);
    }

    if (monitor_list.list_size) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%s", _("User interface on monitor #"));
      Tooltip(_(monitor_tooltip));

      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(combowithWidth);
      if (comboList(&monitor_list)) {
        conf_set_i(bp_conf, "monitor_id", monitor_list.selected - 1);
      }
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text(" ");

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImVec2 rowMin = ImGui::GetItemRectMin();
    ImGui::Text("%s", _("Settings related to the current aircraft"));
    ImGui::TableNextColumn();
    ImGui::Text(" ");
    // Draw bottom border for the first row
    // ImVec2 rowMin = ImGui::GetItemRectMin();
    ImVec2 rowMax = ImGui::GetItemRectMax();
    ImVec2 rowBottomStart = ImVec2(
        rowMin.x, rowMax.y); // Start point of the border (bottom of the row)
    ImVec2 rowBottomEnd = ImVec2(rowMax.x, rowMax.y); // End point of the border

    ImGui::GetWindowDrawList()->AddLine(
        rowBottomStart, rowBottomEnd, LINE_COLOR,
        LINE_THICKNESS); // White line, 1.0f thickness

    /*  Disabling for now, feature not enough mature  
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", _("Per aircraft settings are global"));
    Tooltip(_(per_aircraft_is_global_tooltip));

    ImGui::TableNextColumn();
    if (ImGui::Checkbox("##per_aircraft_is_global", (bool *)&per_aircraft_is_global)) {
      conf_set_b(bp_conf, (char *)"per_aircraft_is_global", per_aircraft_is_global);
      initPerAircraftSettings();
    } */
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", _("Auto disconnect when done"));
    Tooltip(_(disco_when_done_tooltip));

    ImGui::TableNextColumn();
    if (ImGui::Checkbox("##disco_when_done_cbox", (bool *)&disco_when_done)) {
      conf_set_b_per_acf((char *)"disco_when_done", disco_when_done);
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", _("Ignore check parking brake is set"));
    Tooltip(_(ignore_park_brake_tooltip));

    ImGui::TableNextColumn();
    if (ImGui::Checkbox("##ignore_check_park_cbox",
                        (bool *)&ignore_park_brake)) {
      conf_set_b_per_acf((char *)"ignore_park_brake", ignore_park_brake);
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", _("Doors/GPU/ASU check"));
    Tooltip(_(doors_check_tooltip));

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(combowithWidth);
    if (comboList(&doors_check_list)) {
      conf_set_i_per_acf((char *)"doors_check", doors_check_list.selected);
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", _("Hide the magic squares"));
    Tooltip(_(hide_magic_squares_tooltip));

    ImGui::TableNextColumn();
    if (ImGui::Checkbox("##hide_magic_squares_cbox",
                        (bool *)&hide_magic_squares)) {
      conf_set_b_per_acf((char *)"hide_magic_squares", hide_magic_squares);
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", _("Magic squares position"));
    Tooltip(_(magic_squares_height_tooltip));

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(combowithWidth);
    if (ImGui::SliderInt("##magic_position", &magic_squares_height, 20, 80,
                         "%d %%")) {
      conf_set_i_per_acf((char *)"magic_squares_height",
                         (int)magic_squares_height);
      main_intf_hide();
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text(" ");
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    rowMin = ImGui::GetItemRectMin();
    ImGui::Text("%s", _("Miscellaneous"));
    ImGui::TableNextColumn();

    ImGui::Text(" ");
    rowMax = ImGui::GetItemRectMax();
    // Draw bottom border for the first row
    //   rowMin = ImGui::GetItemRectMin();
    //   rowMax = ImGui::GetItemRectMax();
    rowBottomStart = ImVec2(
        rowMin.x, rowMax.y); // Start point of the border (bottom of the row)
    rowBottomEnd = ImVec2(rowMax.x, rowMax.y); // End point of the border

    ImGui::GetWindowDrawList()->AddLine(rowBottomStart, rowBottomEnd,
                                        LINE_COLOR, LINE_THICKNESS);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", _("Always connect the tug first"));
    Tooltip(_(always_connect_tug_first_tooltip));

    ImGui::TableNextColumn();
    if (ImGui::Checkbox("##always_connect_cbox",
                        (bool *)&always_connect_tug_first)) {
      (void)conf_set_b(bp_conf, "always_connect_tug_first",
                       always_connect_tug_first);
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", _("Tug starts near the aircraft"));
    Tooltip(_(tug_starts_next_plane_tooltip));

    ImGui::TableNextColumn();
    if (ImGui::Checkbox("##tug_starts_next_plane",
                        (bool *)&tug_starts_next_plane)) {
      (void)conf_set_b(bp_conf, "tug_starts_next_plane", tug_starts_next_plane);
    }

/*
    if (!tug_starts_next_plane) {
      ImGui::BeginDisabled();
      tug_auto_start = B_FALSE;
      (void)conf_set_b(bp_conf, "tug_auto_start", tug_auto_start);
    }
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", _("Tug called by activating the beacon"));
    Tooltip(_(tug_auto_start_tooltip));

    ImGui::TableNextColumn();
    if (ImGui::Checkbox("##tug_auto_start", (bool *)&tug_auto_start)) {
      (void)conf_set_b(bp_conf, "tug_auto_start", tug_auto_start);
    }
    if (!tug_starts_next_plane) {
      ImGui::EndDisabled();
    }
*/
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", _("Eye Tracker Plugin Exclusion"));
    Tooltip(_(eye_tracker_tooltip));

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(combowithWidth);
    if (comboList(&plg_list)) {
      if (plg_list.selected == 0) {
        conf_set_str(bp_conf, "plg_to_exclude", NULL);
      } else {
        conf_set_str(bp_conf, "plg_to_exclude",
                     plg_list.combo_list[plg_list.selected].value);
      }
    }

    if (xp11_only) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%s", _("Hide default X-Plane 11 tug"));
      Tooltip(_(hide_xp11_tug_tooltip));

      ImGui::TableNextColumn();
      bool hide_ = !dont_hide;
      if (ImGui::Checkbox("##hide_xp11_tug_cbox", (bool *)&hide_)) {
        hide_ = !hide_;
        conf_set_b(bp_conf, "dont_hide_xp11_tug", hide_);
      }
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text(" ");
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    rowMin = ImGui::GetItemRectMin();
    ImGui::Text("%s", _("Audio settings"));
    ImGui::TableNextColumn();

    ImGui::Text(" ");
    rowMax = ImGui::GetItemRectMax();
    // Draw bottom border for the first row
    //   rowMin = ImGui::GetItemRectMin();
    //   rowMax = ImGui::GetItemRectMax();
    rowBottomStart = ImVec2(
        rowMin.x, rowMax.y); // Start point of the border (bottom of the row)
    rowBottomEnd = ImVec2(rowMax.x, rowMax.y); // End point of the border

    ImGui::GetWindowDrawList()->AddLine(rowBottomStart, rowBottomEnd,
                                        LINE_COLOR, LINE_THICKNESS);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", _("Radio output device"));
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(combowithWidth);
    if (comboList(&radio_device_list)) {
      if (radio_device_list.selected == 0) {
        conf_set_str(bp_conf, "radio_device", NULL);
      } else {
        conf_set_str(
            bp_conf, "radio_device",
            radio_device_list.combo_list[radio_device_list.selected].value);
      }
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text(" ");
    ImGui::TableNextRow();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", _("Sound output device"));
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(combowithWidth);
    if (comboList(&sound_device_list)) {
      if (sound_device_list.selected == 0) {
        conf_set_str(bp_conf, "sound_device", NULL);
      } else {
        conf_set_str(
            bp_conf, "sound_device",
            sound_device_list.combo_list[sound_device_list.selected].value);
      }
    }

    ImGui::EndTable();
  }

  ImGui::Text(" ");
  if (save_disabled) {
    ImGui::BeginDisabled();
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                        ImGui::GetStyle().Alpha *
                            BUTTON_DISABLED); // Reduce button opacity
  }
  bool save_button = CenterButton(_("Save preferences"));
  if (save_disabled) {
    ImGui::PopStyleVar();
    ImGui::EndDisabled();
  }
  Tooltip(_(save_prefs_tooltip));
  if (save_button) {
    SetVisible(B_FALSE);
    (void)bp_conf_save();
    bp_sched_reload();
    set_pref_widget_status(B_FALSE);
  }

  ImGui::Text(" ");

  CenterText(_(COPYRIGHT1));
  if (for_credit > 4) {
    CenterText("Thanks and Credit to slgoldberg for better pushing me :)");
  } else {
    CenterText(_(COPYRIGHT2));
    if (ImGui::IsItemHovered()) {
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        for_credit++;
      }
    }
  }

  CenterText("");
  CenterText(_(TOOLTIP_HINT));

  ImGui::PopStyleVar();

  if (is_chinese) {
    ImGui::PopFont();
  }

  setup_view_callback_is_alive = B_TRUE;
}

SettingsWindow *setup_window = nullptr;

bool_t bp_conf_init(void) {
  char *path;
  FILE *fp;

  ASSERT(!inited);

  path = mkpathname(CONF_DIRS, CONF_FILENAME, NULL);
  fp = fopen(path, "rb");
  if (fp != NULL) {
    int errline;

    bp_conf = conf_read(fp, &errline);
    if (bp_conf == NULL) {
      logMsg(BP_ERROR_LOG "error parsing configuration %s: syntax error "
                          "on line %d.",
             path, errline);
      fclose(fp);
      free(path);
      return (B_FALSE);
    }
    fclose(fp);
  } else {
    bp_conf = conf_create_empty();
  }
  free(path);

  inited = B_TRUE;

  fdr_find(&drs.fov_h_deg, "sim/graphics/view/field_of_view_horizontal_deg");
  fdr_find(&drs.fov_h_ratio,
           "sim/graphics/view/field_of_view_horizontal_ratio");
  fdr_find(&drs.fov_roll, "sim/graphics/view/field_of_view_roll_deg");
  fdr_find(&drs.fov_v_deg, "sim/graphics/view/field_of_view_vertical_deg");
  if (bp_xp_ver >= 12000) { // these one only exists in Xp12
    fdr_find(&drs.fov_v_ratio,
             "sim/graphics/view/field_of_view_vertical_ratio");
    fdr_find(&drs.ui_scale, "sim/graphics/misc/user_interface_scale");
  }

  fetchGitVersion();
  return (B_TRUE);
}

bool_t bp_conf_save(void) {
  char *path = mkpathname(CONF_DIRS, NULL);
  bool_t res = B_FALSE;
  FILE *fp;
  bool_t isdir;

  if ((!file_exists(path, &isdir) || !isdir) &&
      !create_directory_recursive(path)) {
    logMsg(BP_ERROR_LOG "error writing configuration: "
                        "can't create parent directory %s",
           path);
    free(path);
    return (B_FALSE);
  }
  free(path);

  path = mkpathname(CONF_DIRS, CONF_FILENAME, NULL);
  fp = fopen(path, "wb");

  if (fp != NULL) {
    if (conf_write(bp_conf, fp)) {
      logMsg(BP_INFO_LOG "Write config file %s", path);
      res = B_TRUE;
    } else {
      logMsg(BP_ERROR_LOG "Error writing configuration %s: %s", path,
             strerror(errno));
    }
    fclose(fp);
  }

  free(path);

  return (res);
}

void destroy_setup_window(void) {

  if (setup_window != nullptr) {
    delete setup_window;
    setup_window = nullptr;
  }
}

void bp_conf_fini(void) {
  if (!inited)
    return;

  destroy_setup_window();
  conf_free(bp_conf);
  bp_conf = NULL;

  pop_fov_values();

  inited = B_FALSE;
}

void bp_conf_set_save_enabled(bool_t flag) {
  ASSERT(inited);
  if (setup_window) {
    setup_window->save_disabled = flag;
  }
}

void key_sanity(char *key) {
  int i;
  for (i = 0; key[i] != '\0'; i++) {
    if ((key[i] == ' ') || (key[i] == '.')) {
      key[i] = '_';
    }
  }
}

bool_t conf_get_b_per_acf(char *my_key, bool_t *value) {
  char my_acf[512], my_path[512];
  bool_t per_aircraft_is_global = B_FALSE;
  (void) conf_get_b(bp_conf, "per_aircraft_is_global", &per_aircraft_is_global);
  XPLMGetNthAircraftModel(0, my_acf, my_path);
  if (per_aircraft_is_global || (strlen(my_acf) == 0)) {
    // if per_aircraft_is_global , the setting is read as global setting
    // if not aircraft found (should never happened), try the generic key
    return conf_get_b(bp_conf, my_key, value);
  } else {
    int l;
    bool_t result;
    char *key;
    l = snprintf(NULL, 0, "%s_%s", my_key, my_acf);
    key = (char *)safe_malloc(l + 1);
    snprintf(key, l + 1, "%s_%s", my_key, my_acf);
    key_sanity(key);
    result = conf_get_b(bp_conf, key, value);
    if (!result) {
      // if not found per aircraft, try the generic key
      result = conf_get_b(bp_conf, my_key, value);
    }
    free(key);
    return result;
  }
}

void conf_set_b_per_acf(char *my_key, bool_t value) {
  char my_acf[512], my_path[512];
  bool_t per_aircraft_is_global = B_FALSE;
  (void) conf_get_b(bp_conf, "per_aircraft_is_global", &per_aircraft_is_global);
  XPLMGetNthAircraftModel(0, my_acf, my_path);
  if (per_aircraft_is_global || (strlen(my_acf) == 0)) {
    // if per_aircraft_is_global , the setting is written as global setting
    // if not aircraft found (should never happened), try the generic key
    (void)conf_set_b(bp_conf, my_key, value);
  } else {
    int l;
    char *key;
    l = snprintf(NULL, 0, "%s_%s", my_key, my_acf);
    key = (char *)safe_malloc(l + 1);
    snprintf(key, l + 1, "%s_%s", my_key, my_acf);
    key_sanity(key);
    (void)conf_set_b(bp_conf, key, value);
    free(key);
  }
}

bool_t conf_get_i_per_acf(char *my_key, int *value) {
  char my_acf[512], my_path[512];
  bool_t per_aircraft_is_global = B_FALSE;
  (void) conf_get_b(bp_conf, "per_aircraft_is_global", &per_aircraft_is_global);
  XPLMGetNthAircraftModel(0, my_acf, my_path);
  if (per_aircraft_is_global || (strlen(my_acf) == 0)) {
    // if per_aircraft_is_global , the setting is written as global setting
    // if not aircraft found (should never happened), try the generic key
    return conf_get_i(bp_conf, my_key, value);
  } else {
    int l;
    bool_t result;
    char *key;
    l = snprintf(NULL, 0, "%s_%s", my_key, my_acf);
    key = (char *)safe_malloc(l + 1);
    snprintf(key, l + 1, "%s_%s", my_key, my_acf);
    key_sanity(key);
    result = conf_get_i(bp_conf, key, value);
    if (!result) {
      // if not found per aircraft, try the generic key
      result = conf_get_i(bp_conf, my_key, value);
    }
    free(key);
    return result;
  }
}

void conf_set_i_per_acf(char *my_key, int value) {
  char my_acf[512], my_path[512];
  bool_t per_aircraft_is_global = B_FALSE;
  (void) conf_get_b(bp_conf, "per_aircraft_is_global", &per_aircraft_is_global);
  XPLMGetNthAircraftModel(0, my_acf, my_path);
  if (per_aircraft_is_global || (strlen(my_acf) == 0)) {
    // if per_aircraft_is_global , the setting is written as global setting
    // if not aircraft found (should never happened), try the generic key
    (void)conf_set_i(bp_conf, my_key, value);
  } else {
    int l;
    char *key;
    l = snprintf(NULL, 0, "%s_%s", my_key, my_acf);
    key = (char *)safe_malloc(l + 1);
    snprintf(key, l + 1, "%s_%s", my_key, my_acf);
    key_sanity(key);
    (void)conf_set_i(bp_conf, key, value);
    free(key);
  }
}

// Save the fov values ratio,angle
// they need to be changed during the planner
void push_reset_fov_values(void) {
  if (!fov_values.planner_running) {
    fov_t new_values = {0};
    get_fov_values_impl(&fov_values);
    fov_values.planner_running = B_TRUE;
    set_fov_values_impl(&new_values);
  }
}

void get_fov_values_impl(fov_t *values) {
  values->fov_h_deg = dr_getf(&drs.fov_h_deg);
  values->fov_h_ratio = dr_getf(&drs.fov_h_ratio);
  values->fov_roll = dr_getf(&drs.fov_roll);
  values->fov_v_deg = dr_getf(&drs.fov_v_deg);
  if (bp_xp_ver >= 12000) // this one only exists in Xp12
    values->fov_v_ratio = dr_getf(&drs.fov_v_ratio);
}

void pop_fov_values(void) {
  if (fov_values.planner_running) {
    set_fov_values_impl(&fov_values);
    fov_values.planner_running = B_FALSE;
  }
}

void set_fov_values_impl(fov_t *values) {
  dr_setf(&drs.fov_h_deg, values->fov_h_deg);
  dr_setf(&drs.fov_h_ratio, values->fov_h_ratio);
  dr_setf(&drs.fov_roll, values->fov_roll);
  dr_setf(&drs.fov_v_deg, values->fov_v_deg);
  if (bp_xp_ver >= 12000) // this one only exists in Xp12
    dr_setf(&drs.fov_v_ratio, values->fov_v_ratio);
}

#define BUFFER_SIZE 1024

int get_ui_monitor_from_pref(void) {
  char *path = mkpathname(CONF_DIRS, XP_PREF_WINDOWS, NULL);
  const char *key = "monitor/0/m_monitor";
  int monit_id = 0;
  FILE *fp = fopen(path, "rb");
  char line[BUFFER_SIZE];
  int line_num;

  UNUSED(line_num);

  if (fp != NULL) {
    while (fgets(line, BUFFER_SIZE, fp) != NULL) { // fgets reads a line
      char *search = strstr(line, key);
      if (search != NULL) {
        monit_id = atoi(search + strlen(key));
        logMsg("monit id %d found in the prf file", monit_id);
        break;
      }
    }
    fclose(fp);
  }

  free(path);
  return monit_id;
}

static size_t wrCallback(void *data, size_t size, size_t nmemb, void *clientp) {
  size_t realsize = size * nmemb;
  struct curl_memory *mem = (struct curl_memory *)clientp;

  char *ptr = (char *)realloc(mem->response, mem->size + realsize + 1);
  if (!ptr)
    return 0; /* out of memory! */

  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), data, realsize);
  mem->size += realsize;
  mem->response[mem->size] = 0;

  return realsize;
}

void parse_response(char *response, char *parsed) {
  char *firstpos = NULL;
  char *lastpos = NULL;
  size_t tag_length;

  memset(parsed, '\0', MAX_VERSION_BF_SIZE);

  firstpos = strstr(response, "tag_name");
  if (firstpos == NULL) {
    goto in_error;
  }
  firstpos = strstr(firstpos, "\"") + 1;
  if (firstpos == NULL) {
    goto in_error;
  }
  firstpos = strstr(firstpos, "\"") + 1;
  if (firstpos == NULL) {
    goto in_error;
  }
  lastpos = strstr(firstpos, "\"");
  if (lastpos == NULL) {
    goto in_error;
  }

  tag_length = lastpos - firstpos;

  if ((tag_length > (MAX_VERSION_BF_SIZE - 1)) || (tag_length <= 0)) {
    logMsg("Response len %d over buffer len size %d.. skipping",
           (int)tag_length, MAX_VERSION_BF_SIZE - 1);
    goto in_error;
  }

  memcpy(parsed, firstpos, tag_length);
  return;

in_error:
  logMsg("Unable to parse git json response;");
  return;
}

void fetchGitVersion(void) {
  CURL *curl_handle;
  CURLcode res;
  gitHubVersion.new_version_available = B_FALSE;
  struct curl_memory response = {0};

  curl_global_init(CURL_GLOBAL_ALL);

  /* init the curl session */
  curl_handle = curl_easy_init();

  if (curl_handle) {
    struct curl_slist *chunk = NULL;

    chunk = curl_slist_append(chunk, "Accept: application/vnd.github+json");
    chunk = curl_slist_append(chunk, "X-GitHub-Api-Version: 2022-11-28");
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, chunk);

    curl_easy_setopt(curl_handle, CURLOPT_URL, GITURL);
    curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, wrCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&response);

    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, DL_TIMEOUT);

    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER,
                     0L); // avoid SSL issue on Windows

    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "curl/8.3.0");

    res = curl_easy_perform(curl_handle);

    /* check for errors */
    if (res != CURLE_OK) {
      logMsg("curl_easy_perform() failed: %s.. skipping",
             curl_easy_strerror(res));
    } else {
      parse_response(response.response, gitHubVersion.version);
      gitHubVersion.new_version_available =
          (strcmp(gitHubVersion.version, BP_PLUGIN_VERSION) != 0);
      logMsg(
          "current version %s / new available version %s / update available %s",
          BP_PLUGIN_VERSION, gitHubVersion.version,
          gitHubVersion.new_version_available ? "true" : "false");
    }

    free(response.response);

    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);
    curl_slist_free_all(chunk);

    /* we are done with libcurl, so clean it up */
    curl_global_cleanup();
  }
}

char *getPluginUpdateStatus(void) {
  return (gitHubVersion.new_version_available ? gitHubVersion.version : NULL);
}

void inMonitorBoundsCallback(int inMonitorIndex, int inLeftBx, int inTopBx,
                             int inRightBx, int inBottomBx, void *inRefcon) {

  UNUSED(inRefcon);

  monitor_def.monitor_count++;
  if (!monitor_def.monitor_found &&
      ((monitor_def.monitor_count == 1) ||
       (inMonitorIndex == monitor_def.monitor_requested))) {
    // we are taking the first one and then if found the one requested
    monitor_def.x_origin = inLeftBx;
    monitor_def.y_origin = inBottomBx;
    monitor_def.h = inTopBx - inBottomBx;
    monitor_def.w = inRightBx - inLeftBx;
    monitor_def.monitor_id = inMonitorIndex;
    if (inMonitorIndex == monitor_def.monitor_requested) {
      monitor_def.monitor_found = B_TRUE;
    }
  }
}

void initMonitorOrigin(void) {
  int monitor_id = -1;
  int magic_squares_height = 50;

  (void)conf_get_i(bp_conf, "monitor_id", &monitor_id);
  (void)conf_get_i_per_acf((char *)"magic_squares_height",
                           &magic_squares_height);

  if (monitor_id == -1) {
    monitor_id = get_ui_monitor_from_pref();
    logMsg("Automatic UI monitor search: id %d found as UI monitor",
           monitor_id);
  } else {
    logMsg("From pref file, id %d is the UI monitor", monitor_id);
  }

  monitor_def.monitor_found = B_FALSE; // 'clear' the found flag
  monitor_def.monitor_count = 0;
  monitor_def.monitor_requested = monitor_id; // We are looking for this id

  monitor_def.x_origin = 0;
  monitor_def.y_origin = 0;
  XPLMGetScreenSize(&monitor_def.w, &monitor_def.h);
  logMsg("%d monitor(s) found. id %d requested / id %d found",
         monitor_def.monitor_count, monitor_def.monitor_requested,
         monitor_def.monitor_id);
  XPLMGetAllMonitorBoundsGlobal(inMonitorBoundsCallback, NULL);
  logMsg("%d monitor(s) found. id %d requested / id %d found",
         monitor_def.monitor_count, monitor_def.monitor_requested,
         monitor_def.monitor_id);
  logMsg("id %d found with  x %d y %d h %d w %d", monitor_def.monitor_id,
         monitor_def.x_origin, monitor_def.y_origin, monitor_def.h,
         monitor_def.w);

  monitor_def.magic_squares_height =
      (int)(monitor_def.h * magic_squares_height / 100);
}

void bp_conf_open() {

  if (!gui_inited) {
    XPImgWindowInit();
    logMsg(BP_INFO_LOG "XPImgWindowInit");
    gui_inited = B_TRUE;
  }

  destroy_setup_window();
  setup_window = new SettingsWindow();
  setup_window->SetVisible(B_TRUE);
  set_pref_widget_status(B_TRUE);
}

void cfg_cleanup() {
  XPImgWindowCleanup();
}