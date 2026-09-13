/**
 * @file audio.cpp
 * @brief Implementation of the engine audio subsystem (miniaudio wrapper).
 *
 * The public engine headers never include miniaudio; everything miniaudio is
 * confined to this translation unit (and the `miniaudio` static library that
 * compiles its implementation exactly once). All API access is from the main
 * thread; miniaudio's own audio thread stays internal.
 */

#include "audio/audio.hpp"

#include <algorithm>
#include <vector>

#include <miniaudio.h>

#include "core/logger.hpp"

namespace MEngine {

struct AudioSystemImpl {
  ma_engine  engine;
  bool       initialized = false;  // ma_engine_init attempted
  bool       ok          = false;  // device open, audible output available
  float      master_volume = 1.0f;
  std::vector<std::weak_ptr<Sound>> sounds;
};

struct SoundImpl {
  ma_sound sound;
  bool     valid = false;
  bool     paused = false;
  ma_uint64 pause_cursor = 0;
};

AudioSystem::AudioSystem() : impl_(std::make_unique<AudioSystemImpl>()) {}

AudioSystem::~AudioSystem() {
  if (impl_ && impl_->ok) {
    ma_engine_uninit(&impl_->engine);
    impl_->ok = false;
    impl_->initialized = true;
  }
}

bool AudioSystem::Initialize() {
  if (impl_->initialized) {
    return impl_->ok;
  }
  impl_->initialized = true;

  ma_engine_config config = ma_engine_config_init();
  const ma_result  result = ma_engine_init(&config, &impl_->engine);
  if (result != MA_SUCCESS) {
    LOG_WARN("Audio") << "No audio output device (ma_engine_init: "
                      << (ma_result_description(result) != nullptr ? ma_result_description(result) : "unknown")
                      << "); sound playback disabled";
    return false;
  }

  impl_->ok = true;
  ma_engine_set_volume(&impl_->engine, impl_->master_volume);
  if (ma_engine_start(&impl_->engine) != MA_SUCCESS) {
    LOG_WARN("Audio") << "ma_engine_start failed (device may be suspended)";
  }
  LOG_INFO("Audio") << "Audio system ready (" << ma_engine_get_sample_rate(&impl_->engine) << " Hz)";
  return true;
}

bool AudioSystem::IsAvailable() const { return impl_->ok; }

void AudioSystem::SetMasterVolume(float volume) {
  impl_->master_volume = std::max(0.0f, std::min(1.0f, volume));
  if (impl_->ok) {
    ma_engine_set_volume(&impl_->engine, impl_->master_volume);
  }
}

float AudioSystem::GetMasterVolume() const { return impl_->master_volume; }

Ref<Sound> AudioSystem::LoadSound(const std::string &path) {
  if (!Initialize()) {
    return nullptr;  // no output device; nothing to play on
  }

  auto impl = std::make_unique<SoundImpl>();
  const ma_result result =
      ma_sound_init_from_file(&impl_->engine, path.c_str(), 0, nullptr, nullptr, &impl->sound);
  if (result != MA_SUCCESS) {
    LOG_ERROR("Audio") << "Failed to load sound '" << path << "' ("
                       << (ma_result_description(result) != nullptr ? ma_result_description(result) : "unknown")
                       << ")";
    return nullptr;
  }
  impl->valid = true;

  // Constructed through `new` inside this friend so the private ctor stays
  // engine-internal (std::make_shared would not be able to reach it).
  Ref<Sound> sound(new Sound(shared_from_this(), std::move(impl)));
  impl_->sounds.emplace_back(sound);  // weak ref for StopAll()
  return sound;
}

Ref<Sound> AudioSystem::Play(const std::string &path, float volume, bool loop) {
  Ref<Sound> sound = LoadSound(path);
  if (sound) {
    sound->Play(volume, loop);
  }
  return sound;
}

void AudioSystem::StopAll() {
  for (auto &weak : impl_->sounds) {
    if (auto sound = weak.lock()) {
      sound->Stop();
    }
  }
}

bool AudioSystem::ProbeFile(const std::string &path, float *out_duration) {
  ma_decoder decoder;
  if (ma_decoder_init_file(path.c_str(), nullptr, &decoder) != MA_SUCCESS) {
    return false;
  }
  ma_uint64 frames = 0;
  const ma_result res = ma_decoder_get_length_in_pcm_frames(&decoder, &frames);
  const ma_uint32 rate = decoder.outputSampleRate;
  ma_decoder_uninit(&decoder);
  if (res != MA_SUCCESS || rate == 0) {
    return false;
  }
  if (out_duration != nullptr) {
    *out_duration = static_cast<float>(frames) / static_cast<float>(rate);
  }
  return true;
}

// --- Sound ------------------------------------------------------------------

Sound::Sound(std::shared_ptr<AudioSystem> owner, std::unique_ptr<SoundImpl> impl)
    : owner_(std::move(owner)), impl_(std::move(impl)) {}

Sound::~Sound() {
  if (impl_ && impl_->valid) {
    ma_sound_uninit(&impl_->sound);
    impl_->valid = false;
  }
}

void Sound::Play(float volume, bool loop) {
  if (!impl_ || !impl_->valid || !owner_->IsAvailable()) {
    return;
  }
  ma_sound_set_volume(&impl_->sound, std::max(0.0f, std::min(1.0f, volume)));
  ma_sound_set_looping(&impl_->sound, loop ? MA_TRUE : MA_FALSE);
  // Restart from the beginning (also the natural "play again" path).
  ma_sound_seek_to_pcm_frame(&impl_->sound, 0);
  if (ma_sound_start(&impl_->sound) == MA_SUCCESS) {
    impl_->paused = false;
  }
}

void Sound::Stop() {
  if (!impl_ || !impl_->valid || !owner_->IsAvailable()) {
    return;
  }
  ma_sound_stop(&impl_->sound);
  ma_sound_seek_to_pcm_frame(&impl_->sound, 0);
  impl_->paused = false;
}

void Sound::Pause() {
  if (!impl_ || !impl_->valid || !owner_->IsAvailable() || !ma_sound_is_playing(&impl_->sound)) {
    return;
  }
  ma_sound_get_cursor_in_pcm_frames(&impl_->sound, &impl_->pause_cursor);
  ma_sound_stop(&impl_->sound);
  impl_->paused = true;
}

void Sound::Resume() {
  if (!impl_ || !impl_->valid || !owner_->IsAvailable() || !impl_->paused) {
    return;
  }
  if (ma_sound_start(&impl_->sound) == MA_SUCCESS) {
    ma_sound_seek_to_pcm_frame(&impl_->sound, impl_->pause_cursor);
    impl_->paused = false;
  }
}

void Sound::SetVolume(float volume) {
  if (impl_ && impl_->valid && owner_->IsAvailable()) {
    ma_sound_set_volume(&impl_->sound, std::max(0.0f, std::min(1.0f, volume)));
  }
}

void Sound::SetPitch(float pitch) {
  if (impl_ && impl_->valid && owner_->IsAvailable()) {
    ma_sound_set_pitch(&impl_->sound, std::max(0.01f, pitch));
  }
}

void Sound::SetLooping(bool loop) {
  if (impl_ && impl_->valid && owner_->IsAvailable()) {
    ma_sound_set_looping(&impl_->sound, loop ? MA_TRUE : MA_FALSE);
  }
}

void Sound::SetPan(float pan) {
  if (impl_ && impl_->valid && owner_->IsAvailable()) {
    ma_sound_set_pan(&impl_->sound, std::max(-1.0f, std::min(1.0f, pan)));
  }
}

bool Sound::IsPlaying() const {
  return impl_ && impl_->valid && owner_->IsAvailable() && ma_sound_is_playing(&impl_->sound);
}

float Sound::GetDuration() const {
  if (!impl_ || !impl_->valid) {
    return 0.0f;
  }
  float seconds = 0.0f;
  if (ma_sound_get_length_in_seconds(&impl_->sound, &seconds) != MA_SUCCESS) {
    return 0.0f;
  }
  return seconds;
}

}  // namespace MEngine
