/* Short, original synthesized switch click. No files, threads or per-frame work.
 * Optional SDK lookup keeps this cosmetic feedback from raising requirements. */
#include <cmath>
#include <cstdint>
#include <XPLMDataAccess.h>
#include <XPLMSound.h>
#include <XPLMUtilities.h>
#include <acfutils/conf.h>
#include "cfg.h"
#include "ui_click_sound.h"

namespace {
constexpr int rate = 22050;
constexpr int samples = 882;
constexpr double default_volume = 0.35;
int16_t pcm[samples];
FMOD_CHANNEL *channel = nullptr;
decltype(&XPLMPlayPCMOnBus) play = nullptr;
decltype(&XPLMStopAudio) stop = nullptr;
decltype(&XPLMSetAudioVolume) set_volume = nullptr;
XPLMDataRef sound_on = nullptr;
double click_volume = default_volume;

void completed(void *, FMOD_RESULT) { channel = nullptr; }

double bounded_volume(double value)
{
    return std::isfinite(value) ? std::fmax(0.0, std::fmin(1.0, value)) :
        default_volume;
}

void stop_active()
{
    FMOD_CHANNEL *previous = channel;
    channel = nullptr;
    if (previous != nullptr && stop != nullptr) stop(previous);
}
}

void bp_ui_click_init()
{
    bool_t configured = B_TRUE;
    double volume = default_volume;
    (void)conf_get_b(bp_conf, "ground_ops_click_sound", &configured);
    (void)conf_get_d(bp_conf, "ground_ops_click_volume", &volume);
    click_volume = configured ? bounded_volume(volume) : 0.0;
    /* The PCM buffer stays immutable during playback. Adjust the channel's
     * gain, never rewrite memory still owned by the simulator's sound engine. */
    for (int i = 0; i < samples; ++i) {
        double t = static_cast<double>(i) / rate;
        double attack = std::fmin(1.0, i / 22.0);
        double tail = 1.0 - static_cast<double>(i) / (samples - 1);
        double click = std::sin(2.0 * 3.141592653589793 * 1700.0 * t) +
            0.45 * std::sin(2.0 * 3.141592653589793 * 2900.0 * t);
        pcm[i] = static_cast<int16_t>(18000 * attack * tail *
            std::exp(-t * 180.0) * click);
    }
    play = reinterpret_cast<decltype(play)>(XPLMFindSymbol("XPLMPlayPCMOnBus"));
    stop = reinterpret_cast<decltype(stop)>(XPLMFindSymbol("XPLMStopAudio"));
    set_volume = reinterpret_cast<decltype(set_volume)>(
        XPLMFindSymbol("XPLMSetAudioVolume"));
    sound_on = XPLMFindDataRef("sim/operation/sound/sound_on");
}

void bp_ui_click_play()
{
    if (click_volume <= 0 || play == nullptr || stop == nullptr ||
        set_volume == nullptr ||
        (sound_on != nullptr && XPLMGetDatai(sound_on) == 0)) return;
    stop_active();
    /* UI bus follows the simulator's UI/master volume and output device. */
    channel = play(pcm, sizeof(pcm), FMOD_SOUND_FORMAT_PCM16, rate, 1, 0,
        xplm_AudioUI, completed, nullptr);
    /* X-Plane starts queued audio at its next sound update, so gain is set
     * before playback, including a preview immediately after slider edits. */
    if (channel != nullptr &&
        set_volume(channel, static_cast<float>(click_volume)) != FMOD_OK)
        stop_active(); // Never fall back to unexpectedly full-volume audio.
}

double bp_ui_click_get_volume()
{
    return click_volume;
}

void bp_ui_click_set_volume(double volume)
{
    click_volume = bounded_volume(volume);
    (void)conf_set_d(bp_conf, "ground_ops_click_volume", click_volume);
    /* Preserve the old config switch, but let a positive slider value unmute. */
    (void)conf_set_b(bp_conf, "ground_ops_click_sound", click_volume > 0);
    if (click_volume == 0) {
        stop_active();
    } else if (channel != nullptr && set_volume != nullptr &&
        set_volume(channel, static_cast<float>(click_volume)) != FMOD_OK) {
        stop_active();
    }
}

void bp_ui_click_fini()
{
    stop_active();
    play = nullptr;
    stop = nullptr;
    set_volume = nullptr;
    sound_on = nullptr;
}
