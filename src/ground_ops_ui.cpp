/*
 * CDDL HEADER START
 *
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * CDDL HEADER END
 */

#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstdint>
#include <new>

#include <XPLMDataAccess.h>
#include <XPLMDisplay.h>
#include <XPLMProcessing.h>

#include <acfutils/conf.h>
#include <acfutils/log.h>
#include <acfutils/time.h>

#include "ImgWindow/xp_img_window.h"
#include "bp.h"
#include "cfg.h"
#include "emergency_tow.h"
#include "ground_ops_data.h"
#include "ground_ops_state.h"
#include "ground_ops_ui.h"
#include "ground_ops_window_state.h"
#include "msg.h"
#include "ui_runtime.h"
#include "xplane.h"

namespace {

constexpr size_t GROUND_OPS_MAX_MONITORS = 16;
constexpr float GEOMETRY_POLL_SECONDS = 1.0f;
constexpr float AIRPORT_CONTEXT_POLL_SECONDS = 1.0f;
constexpr float LOCAL_DATA_POLL_SECONDS = 1.0f;
constexpr double LOCAL_DATA_TTL_SECONDS = 2.5;
constexpr float STATE_POLL_SECONDS = 0.1f;

enum class UiAction {
    None,
    ShowOrb,
    ShowPanel,
    Hide,
    ToggleVisible,
    ToggleExpanded,
    ToggleWindowMode,
    CallTug,
    CallEmergencyTow,
    OpenPlanner,
    ChangePlan,
    PausePush,
    ResumePush,
    ArmEndOperation,
    CancelEndOperation,
    ConfirmEndOperation
};

struct DrawPerformance {
    uint64_t draws = 0;
    uint64_t total_us = 0;
    uint64_t maximum_us = 0;
};

struct MonitorCollection {
    ground_ops_monitor_t monitors[GROUND_OPS_MAX_MONITORS];
    size_t count = 0;
};

class GroundOpsWindow;

static GroundOpsWindow *ground_window = nullptr;
static XPLMFlightLoopID manager_loop = nullptr;
static XPLMFlightLoopID airport_context_loop = nullptr;
static XPLMDataRef latitude_ref = nullptr;
static XPLMDataRef longitude_ref = nullptr;
static XPLMDataRef flight_id_ref = nullptr;
static XPLMDataRef aircraft_icao_ref = nullptr;
static XPLMDataRef wind_direction_ref = nullptr;
static XPLMDataRef wind_speed_ref = nullptr;
static XPLMDataRef temperature_ref = nullptr;
static XPLMDataRef qnh_ref = nullptr;
static bool_t initialized = B_FALSE;
static bool_t ui_enabled = B_TRUE;
static bool_t captions_enabled = B_TRUE;
static bool_t planner_suspended = B_FALSE;
static bool_t have_float_rect = B_FALSE;
static bool_t have_os_rect = B_FALSE;
static ground_ops_rect_t float_rect = {};
static ground_ops_rect_t os_rect = {};
static ground_ops_presentation_t presentation =
    GROUND_OPS_PRESENTATION_HIDDEN;
static ground_ops_window_mode_t window_mode = GROUND_OPS_WINDOW_FLOAT;
static int preferred_monitor = -1;
static UiAction pending_action = UiAction::None;
static DrawPerformance orb_performance;
static DrawPerformance panel_performance;
static uint64_t geometry_recoveries = 0;
static uint64_t click_expansions = 0;
static uint64_t drag_suppressions = 0;
static float geometry_poll_elapsed = 0;
static float local_data_poll_elapsed = LOCAL_DATA_POLL_SECONDS;
static ground_ops_state_t state_cache = {};
static ground_ops_data_snapshot_t data_context = {};
static bool local_data_logged = false;
static bool end_confirmation_armed = false;
static char airport_ident[8] = {};

static void queue_action(UiAction action);
static void note_draw(ground_ops_presentation_t mode, uint64_t elapsed_us);

static double
context_now_s(void)
{
    return (static_cast<double>(microclock()) / 1000000.0);
}

static bool
refresh_local_data_context(void)
{
    char flight_id[8] = {};
    char aircraft_icao[40] = {};
    double now_s = context_now_s();
    uint64_t previous_revision = data_context.revision;

    if (flight_id_ref == nullptr) {
        flight_id_ref = XPLMFindDataRef(
            "sim/cockpit2/radios/actuators/flight_id");
    }
    if (aircraft_icao_ref == nullptr) {
        aircraft_icao_ref = XPLMFindDataRef("sim/aircraft/view/acf_ICAO");
    }
    if (wind_direction_ref == nullptr) {
        wind_direction_ref = XPLMFindDataRef(
            "sim/weather/aircraft/wind_now_direction_degt");
    }
    if (wind_speed_ref == nullptr) {
        wind_speed_ref = XPLMFindDataRef(
            "sim/weather/aircraft/wind_now_speed_msc");
    }
    if (temperature_ref == nullptr) {
        temperature_ref = XPLMFindDataRef(
            "sim/weather/aircraft/temperature_ambient_deg_c");
    }
    if (qnh_ref == nullptr) {
        qnh_ref = XPLMFindDataRef("sim/weather/aircraft/qnh_pas");
    }

    if (flight_id_ref != nullptr) {
        int count = XPLMGetDatab(flight_id_ref, flight_id, 0,
            static_cast<int>(sizeof(flight_id)));
        int icao_count = aircraft_icao_ref != nullptr ?
            XPLMGetDatab(aircraft_icao_ref, aircraft_icao, 0,
            static_cast<int>(sizeof(aircraft_icao))) : 0;

        if (!ground_ops_data_publish_simulator_flight_identity(&data_context,
            flight_id,
            count > 0 ? static_cast<size_t>(count) : 0, aircraft_icao,
            icao_count > 0 ? static_cast<size_t>(icao_count) : 0, now_s,
            now_s + LOCAL_DATA_TTL_SECONDS) &&
            data_context.flight.meta.source ==
            GROUND_OPS_DATA_SOURCE_SIMULATOR) {
            ground_ops_data_clear(&data_context,
                GROUND_OPS_DATA_PROVIDER_FLIGHT_IDENTITY);
        }
    } else {
        ground_ops_data_clear(&data_context,
            GROUND_OPS_DATA_PROVIDER_FLIGHT_IDENTITY);
    }

    if (wind_direction_ref != nullptr && wind_speed_ref != nullptr &&
        temperature_ref != nullptr && qnh_ref != nullptr) {
        if (!ground_ops_data_publish_weather(&data_context,
            GROUND_OPS_DATA_SOURCE_SIMULATOR,
            XPLMGetDataf(wind_direction_ref),
            XPLMGetDataf(wind_speed_ref), XPLMGetDataf(temperature_ref),
            XPLMGetDataf(qnh_ref), now_s,
            now_s + LOCAL_DATA_TTL_SECONDS) &&
            data_context.weather.meta.source ==
            GROUND_OPS_DATA_SOURCE_SIMULATOR) {
            ground_ops_data_clear(&data_context,
                GROUND_OPS_DATA_PROVIDER_WEATHER);
        }
    } else {
        ground_ops_data_clear(&data_context,
            GROUND_OPS_DATA_PROVIDER_WEATHER);
    }

    if (!local_data_logged && (data_context.flight.meta.source !=
        GROUND_OPS_DATA_SOURCE_UNAVAILABLE || data_context.weather.meta.source !=
        GROUND_OPS_DATA_SOURCE_UNAVAILABLE)) {
        ground_ops_data_presentation_t presentation = {};

        ground_ops_data_format(&data_context, now_s, &presentation);
        logMsg(BP_INFO_LOG "Ground Ops simulator context loaded: %s; %s",
            presentation.flight, presentation.weather);
        local_data_logged = true;
    }
    return (data_context.revision != previous_revision);
}

static bool
refresh_airport_context(void)
{
    if (latitude_ref == nullptr) {
        latitude_ref = XPLMFindDataRef(
            "sim/flightmodel/position/latitude");
    }
    if (longitude_ref == nullptr) {
        longitude_ref = XPLMFindDataRef(
            "sim/flightmodel/position/longitude");
    }
    if (latitude_ref == nullptr || longitude_ref == nullptr)
        return (false);

    const double latitude = XPLMGetDatad(latitude_ref);
    const double longitude = XPLMGetDatad(longitude_ref);
    airport_ident[0] = '\0';
    if (find_nearest_airport_at(latitude, longitude, airport_ident)) {
        logMsg(BP_INFO_LOG "Ground Ops airport context loaded: %s",
            airport_ident);
        return (true);
    }
    return (false);
}

static float
airport_context_callback(float elapsed, float elapsed_flight, int counter,
    void *refcon)
{
    (void)elapsed;
    (void)elapsed_flight;
    (void)counter;
    (void)refcon;

    if (!initialized)
        return (0);
    if (!refresh_airport_context())
        return (AIRPORT_CONTEXT_POLL_SECONDS);
    if (manager_loop != nullptr && ground_window != nullptr &&
        !planner_suspended) {
        XPLMScheduleFlightLoop(manager_loop, -1.0f, 1);
    }
    return (0);
}

static ground_ops_caption_t
caption_from_message(message_t message)
{
    switch (message) {
    case MSG_PLAN_START:
        return (GROUND_OPS_CAPTION_PLAN_START);
    case MSG_PLAN_END:
        return (GROUND_OPS_CAPTION_PLAN_END);
    case MSG_DRIVING_UP:
        return (GROUND_OPS_CAPTION_DRIVING_UP);
    case MSG_RDY2CONN:
        return (GROUND_OPS_CAPTION_READY_TO_CONNECT);
    case MSG_RDY2CONN_NOPARK:
        return (GROUND_OPS_CAPTION_READY_TO_CONNECT_NOPARK);
    case MSG_WINCH:
        return (GROUND_OPS_CAPTION_WINCH);
    case MSG_CONNECTED:
        return (GROUND_OPS_CAPTION_CONNECTED);
    case MSG_START_PB:
        return (GROUND_OPS_CAPTION_START_PUSHBACK);
    case MSG_START_TOW:
        return (GROUND_OPS_CAPTION_START_TOW);
    case MSG_START_PB_NOSTART:
        return (GROUND_OPS_CAPTION_START_PUSHBACK_NOSTART);
    case MSG_START_TOW_NOSTART:
        return (GROUND_OPS_CAPTION_START_TOW_NOSTART);
    case MSG_OP_COMPLETE:
        return (GROUND_OPS_CAPTION_OPERATION_COMPLETE);
    case MSG_DISCO:
        return (GROUND_OPS_CAPTION_DISCONNECT);
    case MSG_DONE_RIGHT:
        return (GROUND_OPS_CAPTION_DONE_RIGHT);
    case MSG_DONE_LEFT:
        return (GROUND_OPS_CAPTION_DONE_LEFT);
    case MSG_NUM_MSGS:
        return (GROUND_OPS_CAPTION_NONE);
    }
    return (GROUND_OPS_CAPTION_NONE);
}

static ground_ops_raw_state_t
collect_raw_state(void)
{
    ground_ops_raw_state_t raw = {};
    ground_ops_data_presentation_t data = {};
    msg_caption_state_t message = {};
    double speed_mps = 0, distance_m = 0;
    bool_t speed_valid = B_FALSE, distance_valid = B_FALSE;

    raw.operation_active = (bp_started != B_FALSE);
    raw.emergency_tow = emergency_tow_is_active();
    raw.step = raw.operation_active ? bp.step : PB_STEP_OFF;
    raw.tug_staged = (tug_pending_mode != B_FALSE);
    raw.late_plan = (late_plan_requested != B_FALSE);
    raw.plan_complete = (plan_complete != B_FALSE);
    raw.planner_open = (planner_open != B_FALSE);
    raw.awaiting_plan = (bp_is_awaiting_plan() != B_FALSE);
    raw.replan_available = (bp_can_replan() != B_FALSE);
    raw.pause_requested = (bp_pause_is_requested() != B_FALSE);
    raw.pause_held = (bp_pause_is_held() != B_FALSE);
    ground_ops_data_format(&data_context, context_now_s(), &data);
    (void)std::snprintf(raw.airport_ident, sizeof(raw.airport_ident), "%s",
        airport_ident);
    (void)std::snprintf(raw.flight, sizeof(raw.flight), "%s", data.flight);
    (void)std::snprintf(raw.schedule, sizeof(raw.schedule), "%s",
        data.schedule);
    (void)std::snprintf(raw.weather, sizeof(raw.weather), "%s",
        data.weather);
    (void)std::snprintf(raw.pressure, sizeof(raw.pressure), "%s",
        data.pressure);
    (void)std::snprintf(raw.advisory, sizeof(raw.advisory), "%s",
        data.advisory);
    (void)std::snprintf(raw.data_source, sizeof(raw.data_source), "%s",
        data.source);
    raw.captions_enabled = (captions_enabled != B_FALSE);
    if (raw.operation_active) {
        raw.prep_state = GROUND_OPS_PREP_AIRPORT_DATA;
    } else if (op_complete) {
        raw.prep_state = GROUND_OPS_PREP_COMPLETE;
    } else if (planner_open) {
        raw.prep_state = GROUND_OPS_PREP_PLANNER_REVIEW;
    } else {
        raw.prep_state = GROUND_OPS_PREP_AIRPORT_DATA;
    }

    msg_get_caption_state(&message);
    raw.caption_active = (message.active != B_FALSE);
    raw.caption = caption_from_message(message.message);
    raw.caption_sequence = message.sequence;

    bp_get_ground_ops_metrics(&speed_mps, &speed_valid, &distance_m,
        &distance_valid);
    raw.speed_valid = (speed_valid != B_FALSE);
    raw.speed_tenths_mps = static_cast<int>(std::lround(speed_mps * 10.0));
    raw.distance_valid = (distance_valid != B_FALSE);
    raw.distance_m = static_cast<int>(std::lround(distance_m));
    raw.prep_state_active = !raw.operation_active;
    return (raw);
}

static void
refresh_snapshot(void)
{
    ground_ops_raw_state_t raw = collect_raw_state();
    const ground_ops_snapshot_t *before = ground_ops_state_get(&state_cache);
    uint64_t previous_transition = before != nullptr ?
        before->transition_sequence : 0;

    if (!ground_ops_state_update(&state_cache, &raw))
        return;
    const ground_ops_snapshot_t *snapshot = ground_ops_state_get(&state_cache);
    if (snapshot->transition_sequence != previous_transition) {
        logMsg(BP_INFO_LOG "Ground Ops state transition %llu: stage %s, "
            "controller %s, prep %s, action %s, caption %s",
            static_cast<unsigned long long>(snapshot->transition_sequence),
            snapshot->stage_name,
            ground_ops_step_name(snapshot->controller_step),
            ground_ops_prep_name(snapshot->prep_state),
            snapshot->action_required ? "required" : "none",
            snapshot->caption_visible ? "active" : "none");
    }
    if (snapshot->secondary_action != GROUND_OPS_ACTION_END_DISCONNECT)
        end_confirmation_armed = false;
}

static ground_ops_rect_t
to_rect(const WndRect &rect)
{
    return {rect.left(), rect.top(), rect.right(), rect.bottom()};
}

static WndRect
to_wnd_rect(const ground_ops_rect_t &rect)
{
    return WndRect(rect.left, rect.top, rect.right, rect.bottom);
}

static void
monitor_callback(int monitor_id, int left, int top, int right, int bottom,
    void *refcon)
{
    MonitorCollection *collection =
        reinterpret_cast<MonitorCollection *>(refcon);

    if (collection->count >= GROUND_OPS_MAX_MONITORS)
        return;
    collection->monitors[collection->count++] = {
        monitor_id, {left, top, right, bottom}
    };
}

static MonitorCollection
collect_monitors(bool os_coordinates)
{
    MonitorCollection collection;

    if (os_coordinates)
        XPLMGetAllMonitorBoundsOS(monitor_callback, &collection);
    else
        XPLMGetAllMonitorBoundsGlobal(monitor_callback, &collection);

    if (collection.count == 0 && !os_coordinates) {
        ground_ops_rect_t bounds;

        XPLMGetScreenBoundsGlobal(&bounds.left, &bounds.top,
            &bounds.right, &bounds.bottom);
        collection.monitors[collection.count++] = {-1, bounds};
    }
    return (collection);
}

static bool_t
recover_rect(ground_ops_rect_t *rect, int width, int height,
    bool os_coordinates)
{
    MonitorCollection collection = collect_monitors(os_coordinates);
    bool recovered = ground_ops_rect_recover(rect, width, height,
        collection.monitors, collection.count, preferred_monitor,
        ground_ops_scaled_pixels(GROUND_OPS_WINDOW_MARGIN),
        ground_ops_scaled_pixels(GROUND_OPS_VISIBLE_MINIMUM));

    if (recovered)
        geometry_recoveries++;
    return (recovered ? B_TRUE : B_FALSE);
}

static bool_t
resize_rect_visible(ground_ops_rect_t *rect, int width, int height,
    bool os_coordinates, int margin)
{
    MonitorCollection collection = collect_monitors(os_coordinates);

    return (ground_ops_rect_resize_visible(rect, width, height,
        collection.monitors, collection.count, preferred_monitor, margin) ?
        B_TRUE : B_FALSE);
}

static void
update_preferred_monitor(const ground_ops_rect_t &rect,
    bool os_coordinates)
{
    MonitorCollection collection = collect_monitors(os_coordinates);
    int monitor = ground_ops_rect_monitor(&rect, collection.monitors,
        collection.count);

    if (monitor >= 0)
        preferred_monitor = monitor;
}

class GroundOpsWindow : public XPImgWindow {
public:
    GroundOpsWindow(ground_ops_presentation_t initial_presentation,
        const ground_ops_rect_t &initial_float_rect,
        ground_ops_window_mode_t initial_mode) :
        XPImgWindow(initial_mode == GROUND_OPS_WINDOW_POPOUT ?
            WND_MODE_POPOUT : WND_MODE_FLOAT, WND_STYLE_HUD,
            to_wnd_rect(initial_float_rect)),
        current_presentation(initial_presentation),
        popped_out(initial_mode == GROUND_OPS_WINDOW_POPOUT)
    {
        SetWindowTitle("BetterPushback Ground Operations");
        apply_presentation_contract();
    }

    void set_presentation(ground_ops_presentation_t next)
    {
        current_presentation = next;
        apply_presentation_contract();
    }

    void set_popped_out(bool value)
    {
        popped_out = value;
    }

protected:
    ImGuiWindowFlags_ beforeBegin() override
    {
        ImGui::SetNextWindowBgAlpha(0.0f);
        return (static_cast<ImGuiWindowFlags_>(
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoNav));
    }

    void buildInterface() override
    {
        uint64_t started = microclock();

        if (current_presentation == GROUND_OPS_PRESENTATION_PANEL)
            draw_panel();
        else
            draw_orb();
        note_draw(current_presentation, microclock() - started);
    }

private:
    ground_ops_presentation_t current_presentation;
    bool popped_out;
    bool orb_press_active = false;
    ground_ops_rect_t orb_press_rect = {};

    static float scaled(float value)
    {
        return (value * static_cast<float>(ground_ops_ui_scale()));
    }

    static ImVec2 point(float x, float y)
    {
        return (ImVec2(scaled(x), scaled(y)));
    }

    void apply_presentation_contract()
    {
        int width, height;

        ground_ops_presentation_size(current_presentation, &width, &height);
        SetWindowResizingLimits(width, height, width, height);
        if (current_presentation == GROUND_OPS_PRESENTATION_ORB)
            SetWindowDragArea(0, 0, width, height);
        else
            SetWindowDragArea(0, 0, width - ground_ops_scaled_pixels(90),
                ground_ops_scaled_pixels(45));
    }

    static void tooltip(const char *message)
    {
        constexpr ImGuiHoveredFlags hover_flags =
            ImGuiHoveredFlags_Stationary |
            ImGuiHoveredFlags_DelayNormal |
            ImGuiHoveredFlags_NoSharedDelay |
            ImGuiHoveredFlags_NoNavOverride;

        if (message == nullptr || message[0] == '\0' ||
            !ImGui::IsItemHovered(hover_flags))
            return;
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        float wrap_width = scaled(220.0f);
        bool inside_panel = window_size.x >=
            scaled(GROUND_OPS_PANEL_WIDTH - 1.0f);

        if (inside_panel) {
            wrap_width = window_size.x - scaled(32.0f);
            ImGui::SetNextWindowPos(ImVec2(window_pos.x + scaled(8.0f),
                window_pos.y + scaled(48.0f)), ImGuiCond_Always);
        }
        ImGui::SetNextWindowSizeConstraints(ImVec2(scaled(80.0f), 0),
            ImVec2(wrap_width + scaled(16.0f), FLT_MAX));
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(wrap_width);
        ImGui::TextUnformatted(message);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    static void draw_text(ImDrawList *draw, float size, float x, float y,
        ImU32 color, const char *text)
    {
        draw->AddText(ImGui::GetFont(), scaled(size), point(x, y), color,
            text);
    }

    static ImVec2 measure_text(float size, const char *text)
    {
        return (ImGui::GetFont()->CalcTextSizeA(scaled(size),
            scaled(10000.0f), 0.0f, text));
    }

    static void draw_text_centered(ImDrawList *draw, float size,
        float center_x, float y, ImU32 color, const char *text)
    {
        ImVec2 measured = measure_text(size, text);

        draw->AddText(ImGui::GetFont(), scaled(size),
            ImVec2(scaled(center_x) - measured.x / 2.0f, scaled(y)), color,
            text);
    }

    static void draw_text_right(ImDrawList *draw, float size, float right,
        float y, ImU32 color, const char *text)
    {
        ImVec2 measured = measure_text(size, text);

        draw->AddText(ImGui::GetFont(), scaled(size),
            ImVec2(scaled(right) - measured.x, scaled(y)), color, text);
    }

    static bool icon_button(ImDrawList *draw, const char *id, float x,
        float y, const char *icon, const char *help)
    {
        const ImVec2 size = point(25.0f, 27.0f);
        const ImU32 hovered = IM_COL32(41, 50, 58, 255);
        const ImU32 foreground = IM_COL32(181, 191, 199, 255);

        ImGui::SetCursorPos(point(x, y));
        bool pressed = ImGui::InvisibleButton(id, size);
        if (ImGui::IsItemHovered())
            draw->AddRectFilled(point(x, y),
                ImVec2(scaled(x) + size.x, scaled(y) + size.y), hovered,
                scaled(7.0f));
        ImVec2 measured = measure_text(13.0f, icon);
        draw->AddText(ImGui::GetFont(), scaled(13.0f),
            ImVec2(scaled(x) + (size.x - measured.x) / 2.0f,
            scaled(y) + (size.y - measured.y) / 2.0f), foreground, icon);
        tooltip(help);
        return (pressed);
    }

    static bool action_button(ImDrawList *draw, const char *id,
        float x, float y, float width, const char *label, bool destructive,
        const char *help)
    {
        const ImVec2 size = point(width, 35.0f);
        ImU32 fill = destructive ? IM_COL32(76, 42, 38, 255) :
            IM_COL32(25, 67, 84, 255);
        ImU32 hovered = destructive ? IM_COL32(105, 52, 45, 255) :
            IM_COL32(31, 88, 110, 255);
        ImU32 border = destructive ? IM_COL32(210, 107, 83, 255) :
            IM_COL32(79, 151, 180, 255);
        ImU32 foreground = destructive ? IM_COL32(255, 210, 199, 255) :
            IM_COL32(220, 239, 247, 255);

        ImGui::SetCursorPos(point(x, y));
        bool pressed = ImGui::InvisibleButton(id, size);
        draw->AddRectFilled(point(x, y), point(x + width, y + 35.0f),
            ImGui::IsItemHovered() ? hovered : fill, scaled(6.0f));
        draw->AddRect(point(x, y), point(x + width, y + 35.0f), border,
            scaled(6.0f), 0, scaled(1.0f));
        draw_text_centered(draw, 10.5f, x + width / 2.0f, y + 10.0f,
            foreground, label);
        tooltip(help);
        return (pressed);
    }

    static UiAction ui_action_for(ground_ops_action_t action)
    {
        switch (action) {
        case GROUND_OPS_ACTION_CALL_TUG:
            return (UiAction::CallTug);
        case GROUND_OPS_ACTION_CALL_EMERGENCY_TOW:
            return (UiAction::CallEmergencyTow);
        case GROUND_OPS_ACTION_OPEN_PLANNER:
            return (UiAction::OpenPlanner);
        case GROUND_OPS_ACTION_CHANGE_PLAN:
            return (UiAction::ChangePlan);
        case GROUND_OPS_ACTION_PAUSE:
            return (UiAction::PausePush);
        case GROUND_OPS_ACTION_RESUME:
            return (UiAction::ResumePush);
        case GROUND_OPS_ACTION_END_DISCONNECT:
            return (UiAction::ArmEndOperation);
        case GROUND_OPS_ACTION_NONE:
        default:
            return (UiAction::None);
        }
    }

    void draw_orb()
    {
        ImDrawList *draw = ImGui::GetWindowDrawList();
        const ground_ops_snapshot_t *snapshot = ground_ops_state_get(
            &state_cache);
        const ImU32 shadow = IM_COL32(0, 0, 0, 105);
        const ImU32 background = IM_COL32(21, 26, 31, 246);
        const ImU32 border = IM_COL32(52, 64, 74, 255);
        const ImU32 inactive = IM_COL32(78, 90, 100, 255);
        const ImU32 connector = IM_COL32(55, 65, 73, 255);
        const ImU32 complete = IM_COL32(23, 128, 95, 255);
        const ImU32 complete_text = IM_COL32(120, 188, 159, 255);
        const ImU32 current = IM_COL32(30, 111, 145, 255);
        const ImU32 current_text = IM_COL32(112, 192, 223, 255);
        const ImU32 amber = IM_COL32(230, 166, 61, 255);
        const ImU32 secondary = IM_COL32(121, 134, 145, 255);
        static const char *stages[] = {"Tug", "Connect", "Comms", "Push",
            "Clear"};

        if (snapshot == nullptr)
            return;

        draw->AddRectFilled(point(3, 4),
            point(GROUND_OPS_ORB_WIDTH, GROUND_OPS_ORB_HEIGHT), shadow,
            scaled(11.0f));
        draw->AddRectFilled(point(1, 1),
            point(GROUND_OPS_ORB_WIDTH - 2,
            GROUND_OPS_ORB_HEIGHT - 2), background, scaled(10.0f));
        draw->AddRect(point(1, 1), point(GROUND_OPS_ORB_WIDTH - 2,
            GROUND_OPS_ORB_HEIGHT - 2), border, scaled(10.0f), 0,
            scaled(1.0f));

        for (int index = 0; index < 5; index++) {
            float center_y = 18.0f + index * 47.0f;
            ground_ops_stage_progress_t progress = snapshot->stages[index];
            ImU32 label = progress == GROUND_OPS_STAGE_COMPLETE ?
                complete_text : progress == GROUND_OPS_STAGE_CURRENT ?
                current_text : secondary;

            if (index != 0) {
                draw->AddLine(point(29, center_y - 40),
                    point(29, center_y - 7),
                    snapshot->stages[index - 1] == GROUND_OPS_STAGE_COMPLETE ?
                    complete : connector, scaled(2.0f));
            }
            if (progress == GROUND_OPS_STAGE_COMPLETE) {
                draw->AddCircleFilled(point(29, center_y), scaled(6),
                    complete, 24);
                draw->AddCircle(point(29, center_y), scaled(7),
                    complete_text, 24, scaled(1.0f));
            } else if (progress == GROUND_OPS_STAGE_CURRENT) {
                draw->AddCircleFilled(point(29, center_y), scaled(6),
                    current, 24);
                draw->AddCircle(point(29, center_y), scaled(7),
                    current_text, 24, scaled(1.0f));
                if (snapshot->action_required) {
                    draw->AddCircleFilled(point(47, center_y - 7),
                        scaled(6), amber, 20);
                    draw_text_centered(draw, 9.0f, 47.0f, center_y - 12,
                        IM_COL32(31, 27, 18, 255), "!");
                }
            } else {
                draw->AddCircleFilled(point(29, center_y), scaled(5),
                    background, 20);
                draw->AddCircle(point(29, center_y), scaled(6), inactive,
                    20, scaled(2.0f));
            }
            draw_text_centered(draw, 10.0f, 29.0f, center_y + 9.0f,
                label, stages[index]);
        }

        ImGui::SetCursorPos(point(0, 0));
        bool pressed = ImGui::InvisibleButton("##ground_ops_stage_rail",
            point(GROUND_OPS_ORB_WIDTH, GROUND_OPS_ORB_HEIGHT));
        if (ImGui::IsItemActivated()) {
            orb_press_active = true;
            orb_press_rect = to_rect(GetCurrentWindowGeometry());
        }
        if (pressed && orb_press_active) {
            ground_ops_rect_t current = to_rect(GetCurrentWindowGeometry());
            int dx = current.left - orb_press_rect.left;
            int dy = current.top - orb_press_rect.top;

            if (ground_ops_click_is_activation(dx, dy,
                ground_ops_scaled_pixels(GROUND_OPS_CLICK_DRAG_THRESHOLD))) {
                click_expansions++;
                queue_action(UiAction::ShowPanel);
            } else {
                drag_suppressions++;
            }
            orb_press_active = false;
        }
        if (!ImGui::IsMouseDown(0) && !pressed)
            orb_press_active = false;
        tooltip(snapshot->hover);
    }

    void draw_panel()
    {
        ImDrawList *draw = ImGui::GetWindowDrawList();
        const ground_ops_snapshot_t *snapshot = ground_ops_state_get(
            &state_cache);
        const ImU32 background = IM_COL32(23, 28, 33, 250);
        const ImU32 top_bar = IM_COL32(29, 36, 42, 255);
        const ImU32 section = IM_COL32(21, 26, 31, 255);
        const ImU32 border = IM_COL32(52, 64, 74, 255);
        const ImU32 rule = IM_COL32(44, 53, 61, 255);
        const ImU32 primary = IM_COL32(237, 243, 247, 255);
        const ImU32 secondary = IM_COL32(153, 165, 174, 255);
        const ImU32 muted = IM_COL32(121, 134, 145, 255);
        const ImU32 green = IM_COL32(23, 128, 95, 255);
        const ImU32 green_text = IM_COL32(120, 197, 165, 255);
        const ImU32 blue_panel = IM_COL32(25, 53, 65, 255);
        const ImU32 blue_text = IM_COL32(112, 192, 223, 255);
        const ImU32 amber = IM_COL32(230, 166, 61, 255);
        const ImU32 amber_panel = IM_COL32(57, 45, 27, 255);
        static const char *stages[] = {"Tug", "Connect", "Comms", "Push",
            "Clear"};

        if (snapshot == nullptr)
            return;

        draw->AddRectFilled(point(3, 4),
            point(GROUND_OPS_PANEL_WIDTH, GROUND_OPS_PANEL_HEIGHT),
            IM_COL32(0, 0, 0, 105), scaled(11.0f));
        draw->AddRectFilled(point(1, 1),
            point(GROUND_OPS_PANEL_WIDTH - 2,
            GROUND_OPS_PANEL_HEIGHT - 2), background, scaled(10.0f));
        draw->AddRectFilled(point(2, 2),
            point(GROUND_OPS_PANEL_WIDTH - 2, 45), top_bar, scaled(9.0f));
        draw->AddRectFilled(point(2, 12),
            point(GROUND_OPS_PANEL_WIDTH - 2, 45), top_bar, 0.0f);
        draw->AddRectFilled(point(2, 46),
            point(GROUND_OPS_PANEL_WIDTH - 2, 99), section, 0.0f);
        draw->AddRectFilled(point(2, 99), point(59, 350), section, 0.0f);
        draw->AddRectFilled(point(2, 390),
            point(GROUND_OPS_PANEL_WIDTH - 2,
            GROUND_OPS_PANEL_HEIGHT - 2), section, 0.0f);
        draw->AddRect(point(1, 1),
            point(GROUND_OPS_PANEL_WIDTH - 2,
            GROUND_OPS_PANEL_HEIGHT - 2), border, scaled(10.0f), 0,
            scaled(1.0f));
        draw->AddLine(point(2, 45), point(GROUND_OPS_PANEL_WIDTH - 2,
            45), rule, scaled(1.0f));
        draw->AddLine(point(2, 99), point(GROUND_OPS_PANEL_WIDTH - 2,
            99), rule, scaled(1.0f));
        draw->AddLine(point(59, 99), point(59, 350), rule,
            scaled(1.0f));
        draw->AddLine(point(2, 390), point(GROUND_OPS_PANEL_WIDTH - 2,
            390), rule, scaled(1.0f));

        draw_text(draw, 11.0f, 10, 15, muted, "::");
        draw_text(draw, 14.0f, 30, 8, primary, "Ground operations");
        draw_text(draw, 10.0f, 30, 26, secondary,
            snapshot->flight);

        if (icon_button(draw, "##ground_ops_popout", 205, 9,
            popped_out ? "IN" : "OUT",
            popped_out ? "Return window to X-Plane" :
            "Pop out to an operating-system window"))
            queue_action(UiAction::ToggleWindowMode);
        if (icon_button(draw, "##ground_ops_collapse", 234, 9,
            "^", "Collapse to the five-stage progress rail"))
            queue_action(UiAction::ShowOrb);
        if (icon_button(draw, "##ground_ops_hide", 263, 9,
            "X", "Hide Ground Operations"))
            queue_action(UiAction::Hide);

        draw_text(draw, 13.0f, 11, 56, primary, snapshot->airport);
        draw_text(draw, 10.5f, 11, 70, secondary, snapshot->weather);
        draw_text(draw, 10.5f, 11, 84, secondary, snapshot->pressure);

        for (int index = 0; index < 5; index++) {
            float center_y = 117.0f + index * 46.0f;
            ground_ops_stage_progress_t progress = snapshot->stages[index];
            ImU32 label = progress == GROUND_OPS_STAGE_COMPLETE ?
                green_text : progress == GROUND_OPS_STAGE_CURRENT ?
                blue_text : muted;

            if (index != 0) {
                draw->AddLine(point(30, center_y - 39),
                    point(30, center_y - 7),
                    snapshot->stages[index - 1] == GROUND_OPS_STAGE_COMPLETE ?
                    green : rule, scaled(2.0f));
            }
            if (progress == GROUND_OPS_STAGE_COMPLETE) {
                draw->AddCircleFilled(point(30, center_y), scaled(6), green,
                    24);
                draw->AddCircle(point(30, center_y), scaled(7), green_text,
                    24, scaled(1.0f));
            } else if (progress == GROUND_OPS_STAGE_CURRENT) {
                draw->AddCircleFilled(point(30, center_y), scaled(6),
                    IM_COL32(30, 111, 145, 255), 24);
                draw->AddCircle(point(30, center_y), scaled(7), blue_text,
                    24, scaled(1.0f));
                if (snapshot->action_required) {
                    draw->AddCircleFilled(point(49, center_y - 7),
                        scaled(6), amber, 20);
                    draw_text_centered(draw, 9.0f, 49.0f, center_y - 12,
                        IM_COL32(31, 27, 18, 255), "!");
                }
            } else {
                draw->AddCircleFilled(point(30, center_y), scaled(5),
                    section, 20);
                draw->AddCircle(point(30, center_y), scaled(6),
                    IM_COL32(78, 90, 100, 255), 20, scaled(2.0f));
            }
            draw_text_centered(draw, 10.0f, 30.0f, center_y + 9.0f,
                label, stages[index]);
        }

        draw->AddRectFilled(point(73, 116), point(108, 151), blue_panel,
            scaled(9.0f));
        draw_text_centered(draw, 13.0f, 90.5f, 126, blue_text,
            snapshot->stage == GROUND_OPS_STAGE_TUG ? "TG" :
            snapshot->stage == GROUND_OPS_STAGE_CONNECT ? "CN" :
            snapshot->stage == GROUND_OPS_STAGE_COMMS ? "CM" :
            snapshot->stage == GROUND_OPS_STAGE_PUSH ? "PB" : "OK");
        draw_text(draw, 10.0f, 73, 164,
            snapshot->action_required ? amber : secondary,
            snapshot->eyebrow);
        draw_text(draw, 16.0f, 73, 182, primary, snapshot->status);
        draw_text(draw, 12.0f, 73, 211, secondary, snapshot->detail);
        if (snapshot->caption_visible) {
            draw_text(draw, 11.0f, 73, 232, green_text,
                snapshot->caption);
        }
        draw_text(draw, 10.0f, 73, 253, secondary, snapshot->speed);
        draw_text(draw, 10.0f, 176, 253, secondary, snapshot->distance);

        draw->AddRectFilled(point(73, 282), point(279, 327),
            snapshot->action_required ? amber_panel : top_bar,
            scaled(7.0f));
        draw->AddRect(point(73, 282), point(279, 327), border,
            scaled(7.0f), 0, scaled(1.0f));
        if (snapshot->action_required) {
            draw->AddRectFilled(point(73, 282), point(77, 327), amber,
                scaled(7.0f));
        }
        draw_text(draw, 10.0f, 84, 291,
            snapshot->action_required ? amber : secondary,
            snapshot->action_required ? "PILOT ACTION" : "CURRENT TASK");
        draw_text(draw, 11.0f, 84, 308, primary,
            snapshot->current_task);

        if (end_confirmation_armed) {
            if (action_button(draw, "##ground_ops_keep", 73, 337, 99,
                "Keep operation", false,
                "Cancel; keep the route and tug connected")) {
                queue_action(UiAction::CancelEndOperation);
            }
            if (action_button(draw, "##ground_ops_confirm_end", 180, 337,
                99, "Confirm end", true,
                "Stop completely, discard the route, and disconnect")) {
                queue_action(UiAction::ConfirmEndOperation);
            }
        } else if (snapshot->primary_action != GROUND_OPS_ACTION_NONE) {
            bool have_secondary = snapshot->secondary_action !=
                GROUND_OPS_ACTION_NONE;
            float primary_width = have_secondary ? 99.0f : 206.0f;

            if (action_button(draw, "##ground_ops_primary", 73, 337,
                primary_width, snapshot->primary_action_label, false,
                snapshot->primary_action == GROUND_OPS_ACTION_PAUSE ?
                "Controlled stop; route and steering state are retained" :
                snapshot->primary_action == GROUND_OPS_ACTION_RESUME ?
                "Continue the accepted route from the hold" :
                snapshot->current_task)) {
                queue_action(ui_action_for(snapshot->primary_action));
            }
            if (have_secondary && action_button(draw,
                "##ground_ops_secondary", 180, 337, 99,
                snapshot->secondary_action_label, true,
                "Stop completely, discard the route, and disconnect")) {
                queue_action(ui_action_for(snapshot->secondary_action));
            }
        }

        draw_text(draw, 10.0f, 10, 400, muted, snapshot->source);
        draw_text_right(draw, 10.0f, 281, 400, green_text,
            "Interactive | Offline safe");
    }
};

static void
note_draw(ground_ops_presentation_t mode, uint64_t elapsed_us)
{
    DrawPerformance *performance = mode == GROUND_OPS_PRESENTATION_PANEL ?
        &panel_performance : &orb_performance;

    performance->draws++;
    performance->total_us += elapsed_us;
    if (elapsed_us > performance->maximum_us)
        performance->maximum_us = elapsed_us;
}

static void
reset_performance(void)
{
    orb_performance = {};
    panel_performance = {};
    click_expansions = 0;
    drag_suppressions = 0;
}

static void
log_performance_summary(const char *reason)
{
    double orb_average = orb_performance.draws != 0 ?
        static_cast<double>(orb_performance.total_us) /
        orb_performance.draws / 1000.0 : 0;
    double panel_average = panel_performance.draws != 0 ?
        static_cast<double>(panel_performance.total_us) /
        panel_performance.draws / 1000.0 : 0;

    logMsg(BP_INFO_LOG "Ground Ops UI summary (%s): compact draws %llu, average "
        "%.3f ms, maximum %.3f ms; panel draws %llu, average %.3f ms, "
        "maximum %.3f ms; click expansions %llu, drag suppressions %llu, "
        "geometry recoveries %llu",
        reason,
        static_cast<unsigned long long>(orb_performance.draws), orb_average,
        orb_performance.maximum_us / 1000.0,
        static_cast<unsigned long long>(panel_performance.draws),
        panel_average, panel_performance.maximum_us / 1000.0,
        static_cast<unsigned long long>(click_expansions),
        static_cast<unsigned long long>(drag_suppressions),
        static_cast<unsigned long long>(geometry_recoveries));
}

static bool
load_rect(const char *left_key, const char *top_key, const char *right_key,
    const char *bottom_key, ground_ops_rect_t *rect)
{
    return (conf_get_i(bp_conf, left_key, &rect->left) &&
        conf_get_i(bp_conf, top_key, &rect->top) &&
        conf_get_i(bp_conf, right_key, &rect->right) &&
        conf_get_i(bp_conf, bottom_key, &rect->bottom));
}

static void
load_preferences(void)
{
    int saved_presentation = GROUND_OPS_PRESENTATION_HIDDEN;
    int saved_mode = GROUND_OPS_WINDOW_FLOAT;

    /* Ground Operations and its captions are part of the standard UI. */
    ui_enabled = B_TRUE;
    captions_enabled = B_TRUE;
    if (conf_get_i(bp_conf, "ground_ops_presentation", &saved_presentation) &&
        ground_ops_presentation_valid(saved_presentation)) {
        presentation = static_cast<ground_ops_presentation_t>(
            saved_presentation);
    } else {
        presentation = GROUND_OPS_PRESENTATION_HIDDEN;
    }
    if (conf_get_i(bp_conf, "ground_ops_window_mode", &saved_mode) &&
        ground_ops_window_mode_valid(saved_mode)) {
        window_mode = static_cast<ground_ops_window_mode_t>(saved_mode);
    } else {
        window_mode = GROUND_OPS_WINDOW_FLOAT;
    }
    (void)conf_get_i(bp_conf, "ground_ops_preferred_monitor",
        &preferred_monitor);
    have_float_rect = load_rect("ground_ops_float_left",
        "ground_ops_float_top", "ground_ops_float_right",
        "ground_ops_float_bottom", &float_rect);
    have_os_rect = load_rect("ground_ops_os_left", "ground_ops_os_top",
        "ground_ops_os_right", "ground_ops_os_bottom", &os_rect);
}

static void
persist_preferences(bool write_file)
{
    (void)conf_set_i(bp_conf, "ground_ops_presentation", presentation);
    (void)conf_set_i(bp_conf, "ground_ops_window_mode", window_mode);
    (void)conf_set_i(bp_conf, "ground_ops_preferred_monitor",
        preferred_monitor);
    if (have_float_rect) {
        (void)conf_set_i(bp_conf, "ground_ops_float_left", float_rect.left);
        (void)conf_set_i(bp_conf, "ground_ops_float_top", float_rect.top);
        (void)conf_set_i(bp_conf, "ground_ops_float_right", float_rect.right);
        (void)conf_set_i(bp_conf, "ground_ops_float_bottom",
            float_rect.bottom);
    }
    if (have_os_rect) {
        (void)conf_set_i(bp_conf, "ground_ops_os_left", os_rect.left);
        (void)conf_set_i(bp_conf, "ground_ops_os_top", os_rect.top);
        (void)conf_set_i(bp_conf, "ground_ops_os_right", os_rect.right);
        (void)conf_set_i(bp_conf, "ground_ops_os_bottom", os_rect.bottom);
    }
    /* Do not turn hiding the UI into an implicit Save Preferences action. */
    if (write_file && !get_pref_widget_status())
        (void)bp_conf_save();
}

static void
capture_geometry(void)
{
    if (ground_window == nullptr)
        return;
    ground_ops_rect_t current = to_rect(
        ground_window->GetCurrentWindowGeometry());

    if (window_mode == GROUND_OPS_WINDOW_POPOUT) {
        os_rect = current;
        have_os_rect = B_TRUE;
        update_preferred_monitor(os_rect, true);
    } else {
        float_rect = current;
        have_float_rect = B_TRUE;
        update_preferred_monitor(float_rect, false);
    }
}

static ground_ops_rect_t
prepared_float_rect(ground_ops_presentation_t mode)
{
    int width, height;
    ground_ops_rect_t rect = have_float_rect ? float_rect :
        ground_ops_rect_t {};

    ground_ops_presentation_size(mode, &width, &height);
    if (have_float_rect)
        (void)resize_rect_visible(&rect, width, height, false, 0);
    else
        recover_rect(&rect, width, height, false);
    return (rect);
}

static void
apply_presentation_geometry(ground_ops_presentation_t next)
{
    int old_width, old_height, new_width, new_height;

    if (ground_window == nullptr)
        return;
    capture_geometry();
    ground_ops_presentation_size(presentation, &old_width, &old_height);
    ground_ops_presentation_size(next, &new_width, &new_height);

    /*
     * Update the exact-size contract before asking X-Plane to resize the
     * window. Otherwise the previous presentation's fixed limits can reject
     * the new geometry (most visibly when expanding the orb).
     */
    presentation = next;
    ground_window->set_presentation(next);

    if (window_mode == GROUND_OPS_WINDOW_POPOUT) {
        int pixel_width = os_rect.right - os_rect.left;
        int pixel_height = os_rect.top - os_rect.bottom;
        double horizontal_scale = old_width > 0 && pixel_width > 0 ?
            static_cast<double>(pixel_width) / old_width : 1.0;
        double vertical_scale = old_height > 0 && pixel_height > 0 ?
            static_cast<double>(pixel_height) / old_height : 1.0;
        int next_pixel_width = static_cast<int>(std::lround(
            new_width * horizontal_scale));
        int next_pixel_height = static_cast<int>(std::lround(
            new_height * vertical_scale));

        (void)resize_rect_visible(&os_rect, next_pixel_width,
            next_pixel_height, true, 0);
        ground_window->SetWindowGeometryOS(os_rect.left, os_rect.top,
            os_rect.right, os_rect.bottom);
    } else {
        (void)resize_rect_visible(&float_rect, new_width, new_height, false,
            0);
        ground_window->SetWindowGeometry(float_rect.left, float_rect.top,
            float_rect.right, float_rect.bottom);
    }
}

static bool
create_window(ground_ops_presentation_t requested)
{
    ground_ops_rect_t initial_float;

    if (!ui_enabled || ground_window != nullptr)
        return (ground_window != nullptr);
    if (!bp_ui_runtime_init())
        return (false);

    refresh_snapshot();
    initial_float = prepared_float_rect(requested);
    try {
        ground_window = new GroundOpsWindow(requested, initial_float,
            window_mode);
    } catch (...) {
        ground_window = nullptr;
        logMsg(BP_ERROR_LOG "Unable to create the Ground Operations "
            "window; classic BetterPushback commands remain available");
        return (false);
    }
    float_rect = initial_float;
    have_float_rect = B_TRUE;
    presentation = requested;

    if (window_mode == GROUND_OPS_WINDOW_POPOUT && have_os_rect) {
        int width = os_rect.right - os_rect.left;
        int height = os_rect.top - os_rect.bottom;

        if (width <= 0 || height <= 0) {
            ground_ops_presentation_size(requested, &width, &height);
        }
        recover_rect(&os_rect, width, height, true);
        ground_window->SetWindowGeometryOS(os_rect.left, os_rect.top,
            os_rect.right, os_rect.bottom);
    }
    ground_window->SetVisible(B_TRUE);
    reset_performance();
    logMsg(BP_INFO_LOG "Ground Ops UI shown in %s mode (%s)",
        requested == GROUND_OPS_PRESENTATION_PANEL ? "panel" :
        "compact rail",
        window_mode == GROUND_OPS_WINDOW_POPOUT ? "popout" : "floating");
    return (true);
}

static void
hide_window(const char *reason)
{
    if (ground_window == nullptr) {
        presentation = GROUND_OPS_PRESENTATION_HIDDEN;
        return;
    }
    capture_geometry();
    log_performance_summary(reason);
    delete ground_window;
    ground_window = nullptr;
    presentation = GROUND_OPS_PRESENTATION_HIDDEN;
    planner_suspended = B_FALSE;
    persist_preferences(true);
}

static void
toggle_window_mode(void)
{
    if (ground_window == nullptr)
        return;
    capture_geometry();
    if (window_mode == GROUND_OPS_WINDOW_FLOAT) {
        ground_window->SetMode(WND_MODE_POPOUT);
        window_mode = GROUND_OPS_WINDOW_POPOUT;
        ground_window->set_popped_out(true);
        if (have_os_rect) {
            int width = os_rect.right - os_rect.left;
            int height = os_rect.top - os_rect.bottom;

            if (width > 0 && height > 0) {
                recover_rect(&os_rect, width, height, true);
                ground_window->SetWindowGeometryOS(os_rect.left, os_rect.top,
                    os_rect.right, os_rect.bottom);
            }
        }
    } else {
        ground_window->SetMode(WND_MODE_FLOAT);
        window_mode = GROUND_OPS_WINDOW_FLOAT;
        ground_window->set_popped_out(false);
        float_rect = prepared_float_rect(presentation);
        have_float_rect = B_TRUE;
        ground_window->SetWindowGeometry(float_rect.left, float_rect.top,
            float_rect.right, float_rect.bottom);
    }
    persist_preferences(false);
    logMsg(BP_INFO_LOG "Ground Ops UI moved to %s mode",
        window_mode == GROUND_OPS_WINDOW_POPOUT ? "popout" : "floating");
}

static void
process_action(UiAction action)
{
    switch (action) {
    case UiAction::ShowOrb:
        if (ground_window == nullptr)
            (void)create_window(GROUND_OPS_PRESENTATION_ORB);
        else
            apply_presentation_geometry(GROUND_OPS_PRESENTATION_ORB);
        break;
    case UiAction::ShowPanel:
        if (ground_window == nullptr)
            (void)create_window(GROUND_OPS_PRESENTATION_PANEL);
        else
            apply_presentation_geometry(GROUND_OPS_PRESENTATION_PANEL);
        break;
    case UiAction::Hide:
        hide_window("hidden");
        break;
    case UiAction::ToggleVisible:
        if (ground_window == nullptr)
            (void)create_window(GROUND_OPS_PRESENTATION_ORB);
        else
            hide_window("visibility toggle");
        break;
    case UiAction::ToggleExpanded:
        if (ground_window == nullptr)
            (void)create_window(GROUND_OPS_PRESENTATION_PANEL);
        else if (presentation == GROUND_OPS_PRESENTATION_PANEL)
            apply_presentation_geometry(GROUND_OPS_PRESENTATION_ORB);
        else
            apply_presentation_geometry(GROUND_OPS_PRESENTATION_PANEL);
        break;
    case UiAction::ToggleWindowMode:
        toggle_window_mode();
        break;
    case UiAction::CallTug:
        XPLMCommandOnce(conn_first);
        break;
    case UiAction::CallEmergencyTow:
        XPLMCommandOnce(call_emergency_tow);
        break;
    case UiAction::OpenPlanner:
        XPLMCommandOnce(start_pb);
        break;
    case UiAction::ChangePlan:
        XPLMCommandOnce(start_pb);
        break;
    case UiAction::PausePush:
    case UiAction::ResumePush:
        XPLMCommandOnce(pause_pb);
        break;
    case UiAction::ArmEndOperation:
        end_confirmation_armed = true;
        break;
    case UiAction::CancelEndOperation:
        end_confirmation_armed = false;
        break;
    case UiAction::ConfirmEndOperation:
        end_confirmation_armed = false;
        XPLMCommandOnce(stop_pb);
        break;
    case UiAction::None:
        break;
    }
    if (ground_window != nullptr)
        persist_preferences(false);
}

static float
manager_callback(float elapsed, float elapsed_flight, int counter,
    void *refcon)
{
    (void)elapsed_flight;
    (void)counter;
    (void)refcon;

    UiAction action = pending_action;
    pending_action = UiAction::None;
    if (action != UiAction::None)
        process_action(action);
    if (ground_window != nullptr && !planner_suspended) {
        local_data_poll_elapsed += elapsed;
        if (local_data_poll_elapsed >= LOCAL_DATA_POLL_SECONDS) {
            (void)refresh_local_data_context();
            local_data_poll_elapsed = 0;
        }
        refresh_snapshot();
        geometry_poll_elapsed += elapsed;
        if (geometry_poll_elapsed >= GEOMETRY_POLL_SECONDS) {
            capture_geometry();
            geometry_poll_elapsed = 0;
        }
        return (STATE_POLL_SECONDS);
    }
    return (0);
}

static void
queue_action(UiAction action)
{
    if (!initialized || !ui_enabled || manager_loop == nullptr) {
        logMsg(BP_WARN_LOG "Ground Ops UI command ignored because the new UI "
            "is disabled; classic BetterPushback commands remain available");
        return;
    }
    pending_action = action;
    XPLMScheduleFlightLoop(manager_loop, -1.0f, 1);
}

} /* namespace */

extern "C" bool_t
ground_ops_ui_init(void)
{
    if (initialized)
        return (B_TRUE);

    load_preferences();
    ground_ops_data_init(&data_context);
    ground_ops_state_init(&state_cache);
    refresh_snapshot();
    initialized = B_TRUE;

    if (!ui_enabled) {
        presentation = GROUND_OPS_PRESENTATION_HIDDEN;
        logMsg(BP_INFO_LOG "Ground Ops UI disabled by preference; classic "
            "commands remain available");
        return (B_TRUE);
    }

    XPLMCreateFlightLoop_t loop_definition = {
        sizeof(loop_definition),
        xplm_FlightLoop_Phase_BeforeFlightModel,
        manager_callback,
        nullptr
    };
    manager_loop = XPLMCreateFlightLoop(&loop_definition);
    if (manager_loop == nullptr) {
        initialized = B_FALSE;
        logMsg(BP_ERROR_LOG "Unable to create the Ground Ops UI manager; "
            "classic BetterPushback commands remain available");
        return (B_FALSE);
    }

    XPLMCreateFlightLoop_t context_loop_definition = {
        sizeof(context_loop_definition),
        xplm_FlightLoop_Phase_BeforeFlightModel,
        airport_context_callback,
        nullptr
    };
    airport_context_loop = XPLMCreateFlightLoop(&context_loop_definition);
    if (airport_context_loop == nullptr) {
        XPLMDestroyFlightLoop(manager_loop);
        manager_loop = nullptr;
        initialized = B_FALSE;
        logMsg(BP_ERROR_LOG "Unable to create the Ground Ops airport-context "
            "reader; classic BetterPushback commands remain available");
        return (B_FALSE);
    }
    XPLMScheduleFlightLoop(airport_context_loop, -1.0f, 1);

    if (presentation == GROUND_OPS_PRESENTATION_ORB)
        queue_action(UiAction::ShowOrb);
    else if (presentation == GROUND_OPS_PRESENTATION_PANEL)
        queue_action(UiAction::ShowPanel);
    else
        logMsg(BP_INFO_LOG "Ground Ops UI initialized hidden "
            "(zero recurring callbacks scheduled)");
    return (B_TRUE);
}

extern "C" void
ground_ops_ui_fini(void)
{
    if (!initialized)
        return;
    if (ground_window != nullptr) {
        capture_geometry();
        log_performance_summary("shutdown");
        delete ground_window;
        ground_window = nullptr;
    }
    if (bp_conf != nullptr)
        persist_preferences(true);
    if (manager_loop != nullptr) {
        XPLMDestroyFlightLoop(manager_loop);
        manager_loop = nullptr;
    }
    if (airport_context_loop != nullptr) {
        XPLMDestroyFlightLoop(airport_context_loop);
        airport_context_loop = nullptr;
    }
    latitude_ref = nullptr;
    longitude_ref = nullptr;
    flight_id_ref = nullptr;
    aircraft_icao_ref = nullptr;
    wind_direction_ref = nullptr;
    wind_speed_ref = nullptr;
    temperature_ref = nullptr;
    qnh_ref = nullptr;
    pending_action = UiAction::None;
    planner_suspended = B_FALSE;
    geometry_poll_elapsed = 0;
    local_data_poll_elapsed = LOCAL_DATA_POLL_SECONDS;
    local_data_logged = false;
    ground_ops_data_init(&data_context);
    ground_ops_state_init(&state_cache);
    airport_ident[0] = '\0';
    initialized = B_FALSE;
}

extern "C" void
ground_ops_ui_reset_context(void)
{
    airport_ident[0] = '\0';
    ground_ops_data_init(&data_context);
    local_data_poll_elapsed = LOCAL_DATA_POLL_SECONDS;
    local_data_logged = false;
    if (initialized) {
        if (airport_context_loop != nullptr)
            XPLMScheduleFlightLoop(airport_context_loop, -1.0f, 1);
        if (manager_loop != nullptr && ground_window != nullptr &&
            !planner_suspended) {
            XPLMScheduleFlightLoop(manager_loop, -1.0f, 1);
        }
    }
}

extern "C" bool_t
ground_ops_ui_is_enabled(void)
{
    return (B_TRUE);
}

extern "C" bool_t
ground_ops_ui_is_visible(void)
{
    return (ground_window != nullptr && !planner_suspended);
}

extern "C" void
ground_ops_ui_set_captions_enabled(bool_t enabled)
{
    captions_enabled = enabled;
    if (manager_loop != nullptr && ground_window != nullptr &&
        !planner_suspended) {
        XPLMScheduleFlightLoop(manager_loop, -1.0f, 1);
    }
}

extern "C" void
ground_ops_ui_toggle_visible(void)
{
    queue_action(UiAction::ToggleVisible);
}

extern "C" void
ground_ops_ui_toggle_expanded(void)
{
    queue_action(UiAction::ToggleExpanded);
}

extern "C" void
ground_ops_ui_suspend_for_planner(void)
{
    if (ground_window == nullptr || planner_suspended)
        return;
    capture_geometry();
    ground_window->SetVisible(B_FALSE);
    planner_suspended = B_TRUE;
    if (manager_loop != nullptr)
        XPLMScheduleFlightLoop(manager_loop, 0, 1);
    logMsg(BP_INFO_LOG "Ground Ops UI suspended while the overhead planner "
        "owns the screen");
}

extern "C" void
ground_ops_ui_resume_after_planner(void)
{
    if (ground_window == nullptr || !planner_suspended)
        return;
    planner_suspended = B_FALSE;
    (void)refresh_local_data_context();
    refresh_snapshot();
    ground_window->SetVisible(B_TRUE);
    if (manager_loop != nullptr)
        XPLMScheduleFlightLoop(manager_loop, -1.0f, 1);
    logMsg(BP_INFO_LOG "Ground Ops UI restored after planner close");
}
