/* Shared text measurement policy used by the UI and its layout regression test. */
#ifndef BP_GROUND_OPS_TEXT_FIT_H
#define BP_GROUND_OPS_TEXT_FIT_H
#include <cfloat>
#include "imgui.h"

inline float ground_ops_fit_text_size(ImFont *font, float scale,
    float preferred, float minimum, float width, float height,
    const char *text, bool wrap)
{
    for (float size = preferred; size >= minimum; size -= 0.5f) {
        ImVec2 measured = font->CalcTextSizeA(size * scale, FLT_MAX,
            wrap ? width * scale : 0.0f, text);
        if (measured.x <= width * scale && measured.y <= height * scale)
            return size;
    }
    return minimum;
}
#endif
