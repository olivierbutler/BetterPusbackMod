/* Audio lifecycle contract with SDK mocks; audibility still needs simulator QA. */
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <XPLMSound.h>
#include <XPLMDataAccess.h>
#include <XPLMUtilities.h>
#include "cfg.h"
#include "ui_click_sound.h"

conf_t *bp_conf = nullptr;
static bool api_available = true, sound_enabled = true;
static bool gain_available = true, have_volume = true, have_switch = true;
static bool fail_play = false, fail_gain = false;
static bool_t click_enabled = B_TRUE;
static double volume = 0.20;
static int playing = 0, plays = 0, stops = 0;
static XPLMPCMComplete_f callback = nullptr;
static void *callback_ref = nullptr;
static int token;
static float gain = -1;
static void *active_pcm = nullptr;
static int16_t original_pcm[882];
static int configuration_writes = 0;
static constexpr FMOD_RESULT mock_error = static_cast<FMOD_RESULT>(1);

extern "C" bool_t conf_get_b(const conf_t *, const char *, bool_t *value)
{ if (!have_switch) return B_FALSE; *value = click_enabled; return B_TRUE; }
extern "C" bool_t conf_get_d(const conf_t *, const char *, double *value)
{ if (!have_volume) return B_FALSE; *value = volume; return B_TRUE; }
extern "C" void conf_set_d(conf_t *, const char *key, double value)
{
    assert(!std::strcmp(key, "ground_ops_click_volume"));
    volume = value; have_volume = true; ++configuration_writes;
}
extern "C" void conf_set_b(conf_t *, const char *key, bool_t value)
{
    assert(!std::strcmp(key, "ground_ops_click_sound"));
    click_enabled = value; have_switch = true; ++configuration_writes;
}
extern "C" XPLMDataRef XPLMFindDataRef(const char *) { return &token; }
extern "C" int XPLMGetDatai(XPLMDataRef) { return sound_enabled; }

static void finish()
{
    assert(playing == 1);
    playing = 0;
    callback(callback_ref, FMOD_OK);
}

extern "C" FMOD_CHANNEL *XPLMPlayPCMOnBus(void *pcm, uint32_t bytes,
    FMOD_SOUND_FORMAT format, int rate, int channels, int loop,
    XPLMAudioBus bus, XPLMPCMComplete_f cb, void *ref)
{
    assert(playing == 0 && bytes == 1764 && rate == 22050 && channels == 1);
    assert(format == FMOD_SOUND_FORMAT_PCM16 && loop == 0 && bus == xplm_AudioUI);
    assert(pcm != nullptr && cb != nullptr);
    auto *samples = static_cast<int16_t *>(pcm);
    assert(samples[0] == 0 && samples[881] == 0);
    bool audible = false;
    for (int i = 0; i < 882; ++i) if (samples[i]) audible = true;
    assert(audible);
    if (fail_play) { cb(ref, mock_error); return nullptr; }
    active_pcm = pcm;
    std::memcpy(original_pcm, pcm, sizeof(original_pcm));
    playing = 1; ++plays; callback = cb; callback_ref = ref;
    return &token;
}

extern "C" FMOD_RESULT XPLMSetAudioVolume(FMOD_CHANNEL *channel, float value)
{
    assert(channel == &token && playing == 1);
    assert(std::isfinite(value) && value >= 0 && value <= 1);
    assert(std::memcmp(active_pcm, original_pcm, sizeof(original_pcm)) == 0);
    gain = value;
    return fail_gain ? mock_error : FMOD_OK;
}

extern "C" FMOD_RESULT XPLMStopAudio(FMOD_CHANNEL *channel)
{
    assert(channel == &token);
    ++stops;
    finish();
    return FMOD_OK;
}

extern "C" void *XPLMFindSymbol(const char *name)
{
    if (!api_available) return nullptr;
    if (!std::strcmp(name, "XPLMPlayPCMOnBus")) return reinterpret_cast<void *>(&XPLMPlayPCMOnBus);
    if (!std::strcmp(name, "XPLMStopAudio")) return reinterpret_cast<void *>(&XPLMStopAudio);
    if (!std::strcmp(name, "XPLMSetAudioVolume"))
        return gain_available ? reinterpret_cast<void *>(&XPLMSetAudioVolume) : nullptr;
    assert(false);
    return nullptr;
}

int main()
{
    bp_ui_click_init();
    for (int i = 0; i < 10000; ++i) bp_ui_click_play();
    assert(plays == 10000 && stops == 9999 && playing == 1);
    finish();
    bp_ui_click_fini();
    assert(stops == 9999 && playing == 0);

    bp_ui_click_init();
    bp_ui_click_play();
    bp_ui_click_fini();
    assert(plays == 10001 && stops == 10000 && playing == 0);
    bp_ui_click_fini(); // repeated cleanup is harmless

    for (int scenario = 0; scenario < 5; ++scenario) {
        api_available = scenario != 0;
        sound_enabled = scenario != 1;
        click_enabled = scenario == 2 ? B_FALSE : B_TRUE;
        volume = scenario == 3 ? 0.0 : 0.20;
        gain_available = scenario != 4;
        bp_ui_click_init();
        bp_ui_click_play();
        bp_ui_click_fini();
        assert(plays == 10001 && playing == 0);
    }
    api_available = gain_available = sound_enabled = true;
    click_enabled = B_TRUE;
    have_volume = have_switch = false;
    bp_ui_click_init();
    assert(bp_ui_click_get_volume() == 0.35); // Louder default only if unset.
    assert(configuration_writes == 0); // Loading never overwrites preferences.
    bp_ui_click_play();
    assert(std::fabs(gain - 0.35f) < 0.00001f);

    // Volume updates neither create channels nor alter a live PCM buffer.
    int before_plays = plays;
    for (int i = 1; i <= 10000; ++i) {
        double next = ((i % 100) + 1) / 100.0;
        bp_ui_click_set_volume(next);
        assert(std::fabs(gain - next) < 0.00001);
        assert(playing == 1 && plays == before_plays);
        assert(volume == next && click_enabled);
    }
    bp_ui_click_set_volume(0);
    assert(playing == 0 && volume == 0 && !click_enabled);
    bp_ui_click_play();
    assert(plays == before_plays);
    bp_ui_click_fini();
    bp_ui_click_init();
    assert(bp_ui_click_get_volume() == 0); // Muted preference survives reload.
    bp_ui_click_set_volume(0.67);
    bp_ui_click_play();
    assert(playing == 1 && std::fabs(gain - 0.67f) < 0.00001f);
    bp_ui_click_fini();
    bp_ui_click_init();
    assert(bp_ui_click_get_volume() == 0.67);
    bp_ui_click_fini();

    // Existing values and the legacy mute flag take priority over the default.
    volume = 0.20;
    click_enabled = B_TRUE;
    bp_ui_click_init();
    assert(bp_ui_click_get_volume() == 0.20);
    bp_ui_click_fini();
    click_enabled = B_FALSE;
    bp_ui_click_init();
    assert(bp_ui_click_get_volume() == 0);
    bp_ui_click_set_volume(0.50);
    assert(click_enabled && bp_ui_click_get_volume() == 0.50);

    bp_ui_click_set_volume(-10);
    assert(bp_ui_click_get_volume() == 0);
    bp_ui_click_set_volume(10);
    assert(bp_ui_click_get_volume() == 1);
    bp_ui_click_set_volume(std::numeric_limits<double>::quiet_NaN());
    assert(bp_ui_click_get_volume() == 0.35);
    bp_ui_click_set_volume(std::numeric_limits<double>::infinity());
    assert(bp_ui_click_get_volume() == 0.35);

    fail_play = true;
    bp_ui_click_play();
    assert(playing == 0);
    fail_play = false;
    fail_gain = true;
    bp_ui_click_play();
    assert(playing == 0); // Failed gain cannot produce full-volume playback.
    fail_gain = false;
    bp_ui_click_play();
    assert(playing == 1);
    fail_gain = true;
    bp_ui_click_set_volume(0.25);
    assert(playing == 0);
    fail_gain = false;
    bp_ui_click_fini();
    std::puts("UI click sound lifecycle, live volume, saved settings, mute and optional-API tests passed");
}
