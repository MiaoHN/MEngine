/**
 * @file audio.hpp
 * @brief Engine audio subsystem (sound playback), built on miniaudio.
 *
 * Provides a generic, cross-platform sound module: 2D playback with volume /
 * pitch / pan / loop today, and a 3D-positional API surface that is added on
 * top later (the underlying ma_engine + ma_sound already support it). The
 * public surface never exposes miniaudio types - all implementation detail is
 * hidden behind a pimpl so engine headers stay third-party free.
 */

#pragma once

#include <memory>
#include <string>

#include "core/base.hpp"

namespace MEngine {

class Sound;

// Implementation details live in audio.cpp; only incomplete types appear here.
struct AudioSystemImpl;
struct SoundImpl;

/**
 * @brief The audio subsystem (device + playback engine).
 *
 * Own one `Ref<AudioSystem>` (the engine's Application owns a default one;
 * `Application::GetInstance()->GetAudio()` returns it). Audio output is opened
 * lazily on first use so apps that never play a sound don't touch the device.
 * On machines without an audio endpoint the system stays unavailable and every
 * call degrades to a harmless no-op.
 */
class AudioSystem : public std::enable_shared_from_this<AudioSystem> {
 public:
  explicit AudioSystem();
  ~AudioSystem();
  AudioSystem(const AudioSystem &)            = delete;
  AudioSystem &operator=(const AudioSystem &) = delete;

  /// @brief Opens the default output device (idempotent). Returns true when
  /// audio output is available and playing will actually be audible.
  bool Initialize();

  /// @brief True once the output device is open and usable.
  [[nodiscard]] bool IsAvailable() const;

  /// @brief Master volume in [0, 1] (applies to every sound).
  void SetMasterVolume(float volume);
  [[nodiscard]] float GetMasterVolume() const;

  /// @brief Loads a sound (WAV / FLAC / MP3 / OGG) from a path. The path is
  /// used as-is (pass `AssetManager::Instance().Resolve(...)` for an asset).
  /// Returns nullptr when the file cannot be opened/decoded.
  Ref<Sound> LoadSound(const std::string &path);

  /// @brief Load + start playing as a non-positional (2D) one-shot.
  Ref<Sound> Play(const std::string &path, float volume = 1.0f, bool loop = false);

  /// @brief Stops every sound currently playing.
  void StopAll();

  /// @brief Decodes only a file's header (no output device needed) and writes
  /// its length in seconds to `out_duration`. Returns false when the file
  /// cannot be opened/decoded - handy for asset checks / self tests.
  static bool ProbeFile(const std::string &path, float *out_duration);

 private:
  friend class Sound;
  std::unique_ptr<AudioSystemImpl> impl_;
};

/**
 * @brief A playable sound handle (2D in this phase; 3D hooks come later).
 *
 * Obtained from AudioSystem (load or play). Keeps the AudioSystem alive while
 * it exists, and stops/unloads itself on destruction. Not copyable - share it
 * through `Ref<Sound>`.
 */
class Sound {
 public:
  ~Sound();
  Sound(const Sound &)            = delete;
  Sound &operator=(const Sound &) = delete;

  /// @brief Starts (or restarts) playback.
  void Play(float volume = 1.0f, bool loop = false);

  /// @brief Stops playback and rewinds to the start.
  void Stop();

  /// @brief Pauses at the current position (Resume continues from there).
  void Pause();

  /// @brief Continues playback from the paused position.
  void Resume();

  void SetVolume(float volume);  // 0..1 (relative to master)
  void SetPitch(float pitch);    // playback-rate multiplier (1 = normal)
  void SetLooping(bool loop);
  /// @brief 2D stereo balance in [-1, 1]: -1 = left, 0 = centre, +1 = right.
  void SetPan(float pan);

  [[nodiscard]] bool IsPlaying() const;

  /// @brief Total length in seconds (0 when it cannot be determined).
  [[nodiscard]] float GetDuration() const;

 private:
  friend class AudioSystem;
  Sound(std::shared_ptr<AudioSystem> owner, std::unique_ptr<SoundImpl> impl);

  std::shared_ptr<AudioSystem> owner_;  // keeps the audio device alive with us
  std::unique_ptr<SoundImpl>   impl_;
};

}  // namespace MEngine
