#include <cassert>
#include <cstdio>
#include <cstring>
#include <climits>
#include <initializer_list>
#include "ground_ops_state.h"
#include "ground_ops_text_fit.h"

static unsigned checks = 0;
static void check(ImFont *font, float scale, const char *field, const char *text,
    float preferred, float minimum, float width, float height, bool wrap = false)
{
    float size = ground_ops_fit_text_size(font, scale, preferred, minimum,
        width, height, text, wrap);
    ImVec2 measured = font->CalcTextSizeA(size * scale, FLT_MAX,
        wrap ? width * scale : 0, text);
    if (measured.x > width * scale + 0.01f || measured.y > height * scale + 0.01f) {
        std::fprintf(stderr, "%s does not fit at scale %.2f: %s (%.2f x %.2f in %.2f x %.2f)\n",
            field, scale, text, measured.x, measured.y, width * scale, height * scale);
        assert(false);
    }
    for (const unsigned char *c = reinterpret_cast<const unsigned char *>(text); *c; ++c)
        if (*c >= 32) assert(font->FindGlyphNoFallback(*c) != nullptr);
    ++checks;
}

int main(int argc, char **argv)
{
    ImGui::CreateContext();
    ImFontAtlas *atlas = ImGui::GetIO().Fonts;
    ImFont *font = argc > 1 ? atlas->AddFontFromFileTTF(argv[1], 17, nullptr,
        atlas->GetGlyphRangesCyrillic()) : atlas->AddFontDefault();
    assert(font != nullptr && atlas->Build());
    for (float scale : {1.0f, 1.35f, 2.0f}) {
        for (int step = PB_STEP_OFF; step < PB_STEP_COUNT; ++step) {
            for (int variant = 0; variant < 64; ++variant) {
                for (int caption = 0; caption < GROUND_OPS_CAPTION_COUNT; ++caption) {
                    ground_ops_state_t state;
                    ground_ops_state_init(&state);
                    ground_ops_raw_state_t raw = {};
                    raw.operation_active = step != PB_STEP_OFF;
                    raw.step = static_cast<pushback_step_t>(step);
                    raw.emergency_tow = variant & 1;
                    raw.late_plan = raw.awaiting_plan = variant & 2;
                    raw.plan_complete = raw.replan_available = variant & 4;
                    raw.pause_requested = raw.pause_held = variant & 8;
                    raw.clear_signal_displayed = variant & 16;
                    raw.clear_signal_acknowledged = raw.disconnect_approved = variant & 32;
                    raw.prep_state = static_cast<ground_ops_prep_state_t>(variant % 3);
                    raw.caption_active = raw.captions_enabled = true;
                    raw.caption = static_cast<ground_ops_caption_t>(caption);
                    raw.speed_valid = raw.distance_valid = variant & 1;
                    raw.speed_tenths_mps = raw.distance_m = INT_MAX;
                    ground_ops_state_update(&state, &raw);
                    const auto &s = state.snapshot;
                    check(font,scale,"status",s.status,16,8,206,34,true);
                    check(font,scale,"detail",s.detail,12,7,206,28,true);
                    check(font,scale,"task",s.current_task,10.5f,6.5f,184,27,true);
                    check(font,scale,"eyebrow",s.eyebrow,10,8,206,13);
                    check(font,scale,"caption",s.caption,10.5f,6.5f,206,24,true);
                    check(font,scale,"speed",s.speed,10,6,95,12);
                    check(font,scale,"distance",s.distance,10,6,103,12);
                    float width = s.secondary_action == GROUND_OPS_ACTION_NONE ? 206 : 99;
                    check(font,scale,"primary",s.primary_action_label,10.5f,7,width-12,14);
                    check(font,scale,"secondary",s.secondary_action_label,10.5f,7,87,14);
                    check(font,scale,"airport",s.airport,13,8,270,15);
                    check(font,scale,"weather",s.weather,10.5f,7,270,12);
                    check(font,scale,"pressure",s.pressure,10.5f,7,270,12);
                }
            }
        }
    }
    ImGui::DestroyContext();
    std::printf("Ground Operations text-fit checks passed: %u (%s)\n", checks,
        argc > 1 ? argv[1] : "ImGui default font");
}
