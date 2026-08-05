/*
 * Shared lifetime for the plugin's XPImgWindow/ImGui font resources.
 */

#include "ImgWindow/xp_img_window.h"
#include "ui_runtime.h"
#include "xplane.h"

#include <acfutils/log.h>

static bool_t initialized = B_FALSE;

extern "C" bool_t
bp_ui_runtime_init(void)
{
    if (initialized)
        return (B_TRUE);
    if (!XPImgWindowInit()) {
        logMsg(BP_ERROR_LOG "Unable to initialize the shared UI runtime; "
            "classic BetterPushback commands remain available");
        return (B_FALSE);
    }
    initialized = B_TRUE;
    logMsg(BP_INFO_LOG "Shared XPImgWindow UI runtime initialized");
    return (B_TRUE);
}

extern "C" void
bp_ui_runtime_cleanup(void)
{
    if (!initialized)
        return;
    XPImgWindowCleanup();
    initialized = B_FALSE;
    logMsg(BP_INFO_LOG "Shared XPImgWindow UI runtime cleaned up");
}

extern "C" bool_t
bp_ui_runtime_is_initialized(void)
{
    return (initialized);
}
