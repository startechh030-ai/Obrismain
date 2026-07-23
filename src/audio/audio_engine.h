#ifndef OBRIS_AUDIO_ENGINE_H
#define OBRIS_AUDIO_ENGINE_H

#include "obris.h"
#include <string>
#include <unordered_map>

namespace obris {

struct SoundSlot {
    std::string path;
    void* data = nullptr;  // ma_sound*
    bool loaded = false;
};

struct PlaybackSlot {
    ObrisSound sound;
    void* instance = nullptr;  // ma_sound*
    bool active = false;
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool init(int sampleRate = 48000, int channels = 2);
    void shutdown();
    void update();

    ObrisSound loadSound(const char* path);
    ObrisPlayback play(ObrisSound sound, float volume, bool looping);
    void stop(ObrisPlayback playback);
    void setMasterVolume(float vol);

    bool isInitialized() const { return initialized_; }

private:
    bool initialized_ = false;
    void* engineContext_ = nullptr;  // ma_engine*

    static constexpr int kMaxSounds = 256;
    static constexpr int kMaxPlaybacks = 64;

    SoundSlot sounds_[kMaxSounds];
    PlaybackSlot playbacks_[kMaxPlaybacks];
    uint32_t nextSoundId_ = 1;
    uint32_t nextPlaybackId_ = 1;
};

} // namespace obris

#endif
