#pragma once

#include <array>
#include <string>

#include "core/common.hpp"

namespace MEngine {

class Mesh;
class Shader;

/**
 * @brief Skybox + image-based lighting (IBL) environment.
 *
 * Loads a cubemap from six faces, precomputes an irradiance cubemap for
 * diffuse IBL, renders the skybox as a background, and exposes the environment
 * / irradiance cubemaps to the PBR shader. OpenGL-specific for now.
 */
class Skybox {
 public:
  /// Face order: +X, -X, +Y, -Y, +Z, -Z (right, left, top, bottom, front, back).
  Skybox(const std::array<std::string, 6> &faces, int face_size = 1024);
  ~Skybox();

  Skybox(const Skybox &)            = delete;
  Skybox &operator=(const Skybox &) = delete;

  /// Draws the skybox behind the scene (depth test LEQUAL, no depth write).
  void Render(const glm::mat4 &view, const glm::mat4 &proj) const;

  void BindEnvironment(unsigned int slot) const;
  void BindIrradiance(unsigned int slot) const;

  [[nodiscard]] float GetMaxMipLevel() const { return max_mip_level_; }

 private:
  void GenerateIrradiance();
  void RenderCube() const;

  unsigned int env_cubemap_        = 0;
  unsigned int irradiance_cubemap_ = 0;
  unsigned int capture_fbo_        = 0;
  unsigned int capture_rbo_        = 0;

  int   irradiance_size_ = 32;
  float max_mip_level_   = 0.0f;

  Ref<Shader> skybox_shader_;
  Ref<Shader> irradiance_shader_;
  Ref<Mesh>   cube_;
};

}  // namespace MEngine
