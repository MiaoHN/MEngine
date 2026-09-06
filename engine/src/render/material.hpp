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
  /// @brief Per-pixel specular strength map (LearnOpenGL's `material.specular`
  /// texture, e.g. container2_specular.png). Only sampled by the "blinn" shader
  /// in LO-exact lighting mode; the pbr shader ignores it.
  void SetSpecularMap(Ref<Texture> texture) { specular_map_ = std::move(texture); }

  [[nodiscard]] Ref<Texture> GetAlbedoMap() const { return albedo_map_; }
  [[nodiscard]] Ref<Texture> GetNormalMap() const { return normal_map_; }
  [[nodiscard]] Ref<Texture> GetMetallicRoughnessMap() const { return metallic_roughness_map_; }
  [[nodiscard]] Ref<Texture> GetAOMap() const { return ao_map_; }
  [[nodiscard]] Ref<Texture> GetSpecularMap() const { return specular_map_; }

  void SetBaseColorFactor(const glm::vec4 &factor) { base_color_factor_ = factor; }
  void SetMetallicFactor(float factor) { metallic_factor_ = factor; }
  void SetRoughnessFactor(float factor) { roughness_factor_ = factor; }
  void SetSpecularFactor(float factor) { specular_factor_ = factor; }

  /// @brief Marks the material translucent: it is drawn after the opaque scene
  /// with alpha blending (no depth write). base_color_factor.a is the opacity.
  void SetTranslucent(bool translucent) { translucent_ = translucent; }
  [[nodiscard]] bool IsTranslucent() const { return translucent_; }

  /// @brief Blinn-Phong specular exponent (used only by the "blinn" shader;
  /// the pbr shader ignores it).
  void SetShininess(float shininess) { shininess_ = shininess; }
  [[nodiscard]] float GetShininess() const { return shininess_; }

  /// @brief Explicit specular color (LearnOpenGL's `material.specular` vec3 for
  /// untextured materials, e.g. the grey (0.5,0.5,0.5) of 3.1.materials). When
  /// set it overrides the scalar `specular_factor` in LO-exact mode.
  void SetSpecularColor(const glm::vec3 &color) {
    specular_color_ = color;
    has_specular_color_ = true;
  }
  [[nodiscard]] const glm::vec3 &GetSpecularColor() const { return specular_color_; }
  [[nodiscard]] bool HasSpecularColor() const { return has_specular_color_; }

  /// @brief Emissive / unlit: the surface outputs its albedo directly (no
  /// lighting), like LearnOpenGL's small light-source cubes.
  void SetUnlit(bool unlit) { unlit_ = unlit; }
  [[nodiscard]] bool IsUnlit() const { return unlit_; }

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
  Ref<Texture> specular_map_;

  glm::vec4 base_color_factor_{1.0f};
  float     metallic_factor_  = 1.0f;
  float     roughness_factor_ = 1.0f;
  float     specular_factor_  = 1.0f;
  float     shininess_        = 32.0f;
  glm::vec3 specular_color_{1.0f};
  bool      has_specular_color_ = false;
  bool      translucent_      = false;
  bool      unlit_            = false;
  // Closed, opaque meshes are the norm (primitives, models, stress grids), so
  // back-face culling is on by default: interior/back faces of tightly packed
  // geometry (e.g. adjacent cubes) are not rasterised, which removes the
  // "overlapping interior faces" look when the camera goes inside geometry and
  // the redundant shared-face fill. Materials that must be double-sided
  // (planes, decals, editor overlays) set CullMode::None explicitly.
  CullMode cull_mode_ = CullMode::Back;
};

}  // namespace MEngine
