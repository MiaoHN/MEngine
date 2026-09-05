#pragma once

#include <string>

#include "core/common.hpp"

namespace MEngine {

class Mesh;
class Shader;

/**
 * @brief Skybox + image-based lighting (IBL) environment.
 *
 * Loads an equirectangular HDR environment, converts it to a cubemap, and
 * precomputes an irradiance cubemap (diffuse IBL) and a prefiltered cubemap
 * (specular IBL with per-roughness mip levels). Also renders the environment
 * as the scene background. OpenGL-specific for now.
 */
class Skybox {
 public:
  Skybox(const std::string &hdr_path, int env_size = 512, int irradiance_size = 32, int prefilter_size = 128);
  ~Skybox();

  Skybox(const Skybox &)            = delete;
  Skybox &operator=(const Skybox &) = delete;

  /// Draws the skybox behind the scene (depth test LEQUAL, no depth write).
  void Render(const glm::mat4 &view, const glm::mat4 &proj) const;

  void BindEnvironment(unsigned int slot) const;
  void BindIrradiance(unsigned int slot) const;
  void BindPrefilter(unsigned int slot) const;

  /// @brief Highest valid mip level of the prefiltered cubemap (0..max).
  [[nodiscard]] float GetMaxPrefilterMip() const { return static_cast<float>(prefilter_mip_levels_ - 1); }

 private:
  void GenerateEnvironment();
  void GenerateIrradiance();
  void GeneratePrefilter();
  void RenderCube() const;

  unsigned int env_cubemap_        = 0;
  unsigned int irradiance_cubemap_ = 0;
  unsigned int prefilter_cubemap_  = 0;
  unsigned int equirect_texture_   = 0;
  unsigned int capture_fbo_        = 0;
  unsigned int capture_rbo_        = 0;

  int env_size_             = 512;
  int irradiance_size_      = 32;
  int prefilter_size_       = 128;
  int prefilter_mip_levels_ = 5;

  Ref<Shader> skybox_shader_;
  Ref<Shader> irradiance_shader_;
  Ref<Shader> prefilter_shader_;
  Ref<Shader> equirect_shader_;
  Ref<Mesh>   cube_;
};

}  // namespace MEngine
