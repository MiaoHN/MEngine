#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "core/common.hpp"

namespace MEngine {

class Shader;

/**
 * @brief Screen-space ambient occlusion (OpenGL-specific for now).
 *
 * A geometry pass writes view-space position + normal into a G-buffer, then a
 * fullscreen pass samples a tangent-space hemisphere kernel against the
 * G-buffer to estimate occlusion, followed by a 4x4 box blur. The result is
 * sampled by the PBR shader to darken the ambient term in crevices.
 */
class SSAO {
 public:
  SSAO(int width, int height);
  ~SSAO();

  SSAO(const SSAO &)            = delete;
  SSAO &operator=(const SSAO &) = delete;

  /// Binds the G-buffer and the geometry shader, sets proj/view and clears.
  void BeginGeometryPass(const glm::mat4 &proj, const glm::mat4 &view) const;
  /// Uploads the per-mesh model matrix to the bound geometry shader.
  void SetGeometryModel(const glm::mat4 &model) const;
  /// Unbinds the G-buffer.
  void EndGeometryPass() const;

  /// Runs the SSAO sampling pass and the blur pass.
  void Generate(const glm::mat4 &proj, const glm::mat4 &view) const;

  /// Binds the (blurred) SSAO texture to the given texture unit.
  void BindTexture(unsigned int slot) const;

  /// @brief Full-resolution size (used by the PBR shader to sample SSAO).
  [[nodiscard]] int GetWidth() const { return width_; }
  [[nodiscard]] int GetHeight() const { return height_; }

  void SetRadius(float radius) { radius_ = radius; }
  void SetBias(float bias) { bias_ = bias; }

 private:
  void DrawFullscreenTriangle() const;

  unsigned int g_buffer_fbo_ = 0;
  unsigned int g_position_   = 0;
  unsigned int g_normal_     = 0;
  unsigned int g_depth_rbo_  = 0;

  unsigned int noise_texture_ = 0;

  unsigned int ssao_fbo_     = 0;
  unsigned int ssao_texture_ = 0;

  unsigned int blur_fbo_     = 0;
  unsigned int blur_texture_ = 0;

  unsigned int fullscreen_vao_ = 0;

  int width_  = 0;
  int height_ = 0;

  // SSAO runs at half resolution; the box blur smooths the result back out.
  int ssao_width_  = 0;
  int ssao_height_ = 0;
  int sample_count_ = 32;

  float radius_ = 0.5f;
  float bias_   = 0.025f;

  std::vector<glm::vec3> kernel_;

  Ref<Shader> geometry_shader_;
  Ref<Shader> ssao_shader_;
  Ref<Shader> blur_shader_;
};

}  // namespace MEngine
