#include "audio_engine.h"
#include <android/log.h>

#define LOG_TAG "ObrisAudio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace obris {

AudioEngine::AudioEngine() {
    for (auto& s : sounds_)   s.loaded = false;
    for (auto& p : playbacks_) p.active = false;
}

AudioEngine::~AudioEngine() { shutdown(); }

bool AudioEngine::init(int sampleRate, int channels) {
    if (initialized_) return true;
#if defined(OBRIS_USE_MINIAUDIO) && OBRIS_USE_MINIAUDIO
    // ma_engine_config config = ma_engine_config_init();
    // config.sampleRate = sampleRate;
    // config.channels = channels;
    // ma_result r = ma_engine_init(&config, &engineContext_);
    // if (r != MA_SUCCESS) return false;
    LOGI("AudioEngine init: %dHz %dch", sampleRate, channels);
#else
    LOGI("AudioEngine stub init");
#endif
    initialized_ = true;
    return true;
}

void AudioEngine::shutdown() {
    if (!initialized_) return;
    for (auto& pb : playbacks_) {
        if (pb.active) stop(static_cast<ObrisPlayback>(&pb - playbacks_ + 1));
    }
#if defined(OBRIS_USE_MINIAUDIO) && OBRIS_USE_MINIAUDIO
    // ma_engine_uninit(&engineContext_);
#endif
    initialized_ = false;
}

void AudioEngine::update() {
    // miniaudio handles its own threading
}

ObrisSound AudioEngine::loadSound(const char* path) {
    for (uint32_t i = 0; i < kMaxSounds; ++i) {
        auto& slot = sounds_[i];
        if (!slot.loaded) {
            slot.path = path;
#if defined(OBRIS_USE_MINIAUDIO) && OBRIS_USE_MINIAUDIO
            // ma_sound_init_from_file(&engineContext_, path, 0, NULL, NULL, (ma_sound**)&slot.data);
#endif
            slot.loaded = true;
            ObrisSound h = i + 1;
            LOGI("Loaded sound: %s (id=%u)", path, h);
            return h;
        }
    }
    return OBRIS_INVALID_HANDLE;
}

ObrisPlayback AudioEngine::play(ObrisSound sound, float volume, bool looping) {
    if (sound == OBRIS_INVALID_HANDLE || sound > kMaxSounds) return OBRIS_INVALID_HANDLE;
    if (!sounds_[sound - 1].loaded) return OBRIS_INVALID_HANDLE;

    for (uint32_t i = 0; i < kMaxPlaybacks; ++i) {
        auto& slot = playbacks_[i];
        if (!slot.active) {
            slot.sound = sound;
#if defined(OBRIS_USE_MINIAUDIO) && OBRIS_USE_MINIAUDIO
            // ma_sound_init_copy(&engineContext_, (ma_sound*)sounds_[sound-1].data, 0, NULL, (ma_sound**)&slot.instance);
            // ma_sound_set_volume((ma_sound*)slot.instance, volume);
            // ma_sound_set_looping((ma_sound*)slot.instance, looping);
            // ma_sound_start((ma_sound*)slot.instance);
#endif
            slot.active = true;
            ObrisPlayback h = i + 1;
            LOGI("Play sound=%u (pb=%u, vol=%.2f, loop=%d)", sound, h, volume, looping);
            return h;
        }
    }
    return OBRIS_INVALID_HANDLE;
}

void AudioEngine::stop(ObrisPlayback playback) {
    if (playback == OBRIS_INVALID_HANDLE || playback > kMaxPlaybacks) return;
    auto& slot = playbacks_[playback - 1];
    if (slot.active) {
#if defined(OBRIS_USE_MINIAUDIO) && OBRIS_USE_MINIAUDIO
        // ma_sound_stop((ma_sound*)slot.instance);
        // ma_sound_uninit((ma_sound*)slot.instance);
#endif
        slot.active = false;
        slot.instance = nullptr;
    }
}

void AudioEngine::setMasterVolume(float vol) {
#if defined(OBRIS_USE_MINIAUDIO) && OBRIS_USE_MINIAUDIO
    // ma_engine_set_volume(&engineContext_, vol);
#else
    (void)vol;
#endif
}

} // namespace obris
