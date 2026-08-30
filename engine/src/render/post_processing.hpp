#pragma once

#include "core/common.hpp"

namespace MEngine {

class Shader;

/**
 * @brief HDR render target + bloom post-processing (OpenGL-specific for now).
 *
 * The scene is rendered into an RGBA16F framebuffer, then a bloom pass
 * (brightness threshold + Gaussian blur) is composited on top before ACES
 * tone mapping and gamma correction to the default framebuffer.
 */
class PostProcessing {
 public:
  PostProcessing(int width, int height);
  ~PostProcessing();

  PostProcessing(const PostProcessing &)            = delete;
  PostProcessing &operator=(const PostProcessing &) = delete;

  /// Binds the HDR scene framebuffer, sets the viewport and clears it.
  void BeginScene() const;
  /// Unbinds the HDR scene framebuffer.
  void EndScene() const;
  /// Runs god rays + bloom + tone mapping and composites to `target_fbo`
  /// (0 = default framebuffer). `target_width`/`target_height` override the
  /// composite viewport when rendering into a custom framebuffer.
  /// `god_rays_scale` (0..1) gates the god-rays contribution per frame.
  void Render(const glm::vec2 &light_screen_pos, unsigned int target_fbo = 0, int target_width = 0,
              int target_height = 0, float god_rays_scale = 1.0f) const;

  /// @brief Resolves the jittered scene into the TAA history buffer.
  void ResolveTAA() const;
  /// @brief Returns the current sub-pixel jitter (NDC) and advances the frame.
  glm::vec2 GetJitter() const;
  /// @brief Returns the scene color texture (TAA-resolved when enabled).
  [[nodiscard]] unsigned int GetSceneColorTexture() const;

  void SetExposure(float exposure) { exposure_ = exposure; }
  void SetBloomStrength(float strength) { bloom_strength_ = strength; }
  void SetBloomThreshold(float threshold) { bloom_threshold_ = threshold; }
  void SetGodRaysStrength(float strength) { god_rays_strength_ = strength; }
  void SetTAAEnabled(bool enabled) { taa_enabled_ = enabled; }
  [[nodiscard]] bool IsTAAEnabled() const { return taa_enabled_; }

 private:
  void DrawFullscreenTriangle() const;
  void CreateColorFramebuffer(unsigned int &fbo, unsigned int &texture, int width, int height) const;

  unsigned int scene_fbo_     = 0;
  unsigned int scene_texture_ = 0;
  unsigned int scene_depth_   = 0;

  unsigned int bright_fbo_     = 0;
  unsigned int bright_texture_ = 0;

  unsigned int blur_fbo_[2]     = {0, 0};
  unsigned int blur_texture_[2] = {0, 0};

  unsigned int god_rays_fbo_     = 0;
  unsigned int god_rays_texture_ = 0;

  unsigned int taa_fbo_         = 0;
  unsigned int taa_textures_[2] = {0, 0};
  mutable int  taa_current_     = 0;
  mutable int  taa_frame_       = 0;
  mutable bool taa_first_frame_ = true;
  bool         taa_enabled_     = false;
  float        taa_blend_       = 0.1f;

  int width_        = 0;
  int height_       = 0;
  int bloom_width_  = 0;
  int bloom_height_ = 0;

  int   blur_passes_     = 8;
  float exposure_        = 1.2f;
  float bloom_strength_  = 0.02f;
  float bloom_threshold_ = 1.0f;
  float god_rays_strength_ = 0.05f;

  unsigned int fullscreen_vao_ = 0;

  Ref<Shader> brightness_shader_;
  Ref<Shader> blur_shader_;
  Ref<Shader> composite_shader_;
  Ref<Shader> god_rays_shader_;
  Ref<Shader> taa_shader_;
};

}  // namespace MEngine
