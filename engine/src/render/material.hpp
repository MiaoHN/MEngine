#pragma once

#include <glm/glm.hpp>

#include "core/common.hpp"
#include "render/rhi/rhi.hpp"
#include "render/shader.hpp"
#include "render/texture.hpp"

namespace MEngine {

/**
 * @brief A PBR material following the glTF metallic-roughness workflow.
 *
 * Holds a shader, up to four textures (albedo, normal, metallic-roughness,
 * ambient occlusion) and the scalar factors from the material definition.
 * Binding of textures and uploading of uniforms is done by Renderer.
 */
class Material {
 public:
  Material()  = default;
  ~Material() = default;

  /// @brief Face culling applied while drawing with this material.
  void SetCullMode(CullMode mode) { cull_mode_ = mode; }
  [[nodiscard]] CullMode GetCullMode() const { return cull_mode_; }

  void SetShader(Ref<Shader> shader) { shader_ = std::move(shader); }
  [[nodiscard]] Ref<Shader> GetShader() const { return shader_; }

  void SetAlbedoMap(Ref<Texture> texture) { albedo_map_ = std::move(texture); }
  void SetNormalMap(Ref<Texture> texture) { normal_map_ = std::move(texture); }
  void SetMetallicRoughnessMap(Ref<Texture> texture) { metallic_roughness_map_ = std::move(texture); }
  void SetAOMap(Ref<Texture> texture) { ao_map_ = std::move(texture); }

  [[nodiscard]] Ref<Texture> GetAlbedoMap() const { return albedo_map_; }
  [[nodiscard]] Ref<Texture> GetNormalMap() const { return normal_map_; }
  [[nodiscard]] Ref<Texture> GetMetallicRoughnessMap() const { return metallic_roughness_map_; }
  [[nodiscard]] Ref<Texture> GetAOMap() const { return ao_map_; }

  void SetBaseColorFactor(const glm::vec4 &factor) { base_color_factor_ = factor; }
  void SetMetallicFactor(float factor) { metallic_factor_ = factor; }
  void SetRoughnessFactor(float factor) { roughness_factor_ = factor; }
  void SetSpecularFactor(float factor) { specular_factor_ = factor; }

  [[nodiscard]] const glm::vec4 &GetBaseColorFactor() const { return base_color_factor_; }
  [[nodiscard]] float GetMetallicFactor() const { return metallic_factor_; }
  [[nodiscard]] float GetRoughnessFactor() const { return roughness_factor_; }
  [[nodiscard]] float GetSpecularFactor() const { return specular_factor_; }

 private:
  Ref<Shader>  shader_;
  Ref<Texture> albedo_map_;
  Ref<Texture> normal_map_;
  Ref<Texture> metallic_roughness_map_;
  Ref<Texture> ao_map_;

  glm::vec4 base_color_factor_{1.0f};
  float     metallic_factor_ = 1.0f;
  float     roughness_factor_ = 1.0f;
  float     specular_factor_ = 1.0f;
  // Closed, opaque meshes are the norm (primitives, models, stress grids), so
  // back-face culling is on by default: interior/back faces of tightly packed
  // geometry (e.g. adjacent cubes) are not rasterised, which removes the
  // "overlapping interior faces" look when the camera goes inside geometry and
  // the redundant shared-face fill. Materials that must be double-sided
  // (planes, decals, editor overlays) set CullMode::None explicitly.
  CullMode cull_mode_ = CullMode::Back;
};

}  // namespace MEngine
