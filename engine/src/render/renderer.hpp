/**
 * @file renderer.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-19
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include "core/common.hpp"
#include "render/light.hpp"

namespace MEngine {

struct Sprite2D;
struct AnimatedSprite2D;
class RenderPipeline;
class RenderPass;
class Mesh;
class Material;
class PostProcessing;
class Shader;
class ShadowMap;
class CubeShadowMap;
class Skybox;
class SSAO;
class Texture;

class Renderer {
 public:
  Renderer();
  ~Renderer();

  void RenderSprite(Sprite2D &sprite, const glm::mat4 &proj_view) const;
  void RenderSprite(AnimatedSprite2D &sprite, const glm::mat4 &proj_view) const;

  /// @brief Begins the directional shadow pass.
  void BeginShadowPass(const glm::mat4 &light_view_proj) const;
  /// @brief Renders a mesh into the shadow map.
  void DrawMeshShadow(const Ref<Mesh> &mesh, const glm::mat4 &model, const glm::mat4 &light_view_proj) const;
  /// @brief Ends the shadow pass and restores the default framebuffer.
  void EndShadowPass() const;

  /// @brief Begins the cube shadow pass for a shadow-casting point light.
  void BeginPointShadowPass(int light_index, const glm::vec3 &light_pos, float far_plane) const;
  /// @brief Attaches + clears one cube face and sets its light-space matrix.
  void BindPointShadowFace(int light_index, int face, const glm::mat4 &light_space_matrix) const;
  /// @brief Renders a mesh into the current point shadow face.
  void DrawMeshPointShadow(const Ref<Mesh> &mesh, const glm::mat4 &model) const;
  /// @brief Ends the point shadow pass.
  void EndPointShadowPass(int light_index) const;

  /// @brief Begins the SSAO geometry pass (view-space position + normal).
  void BeginSSAOPass(const glm::mat4 &proj, const glm::mat4 &view) const;
  /// @brief Renders a mesh into the SSAO G-buffer.
  void DrawMeshSSAO(const Ref<Mesh> &mesh, const glm::mat4 &model) const;
  /// @brief Ends the SSAO geometry pass.
  void EndSSAOPass() const;
  /// @brief Runs the SSAO sampling + blur passes.
  void GenerateSSAO(const glm::mat4 &proj, const glm::mat4 &view) const;
  /// @brief Binds the blurred SSAO texture to the given texture unit.
  void BindSSAO(unsigned int slot) const;

  void SetSSAOEnabled(bool enabled) { ssao_enabled_ = enabled; }
  [[nodiscard]] bool IsSSAOEnabled() const { return ssao_enabled_; }

  /// @brief Draw a 3D mesh with the given PBR material (shadowed by the light).
  void DrawMesh(const Ref<Mesh> &mesh, const Ref<Material> &material, const glm::mat4 &model,
                const glm::mat4 &proj_view, const glm::vec3 &view_pos, const glm::mat4 &light_view_proj) const;

  [[nodiscard]] const DirectionalLight &GetLight() const { return light_; }
  DirectionalLight &GetLight() { return light_; }
  void SetLight(const DirectionalLight &light) { light_ = light; }

  void AddPointLight(const PointLight &light) { point_lights_.push_back(light); }
  void ClearPointLights() { point_lights_.clear(); }
  [[nodiscard]] const std::vector<PointLight> &GetPointLights() const { return point_lights_; }

  void AddSpotLight(const SpotLight &light) { spot_lights_.push_back(light); }
  void ClearSpotLights() { spot_lights_.clear(); }
  [[nodiscard]] const std::vector<SpotLight> &GetSpotLights() const { return spot_lights_; }

  /// @brief Maps a point light index to its cube shadow map index, or -1 when
  /// the light does not cast a shadow (or exceeds the shadow budget).
  [[nodiscard]] int GetPointShadowIndex(int light_index) const;

  /// @brief Maximum number of point lights that can cast cube shadows.
  static constexpr int kMaxPointShadows = 4;

  /// @brief Binds the HDR scene framebuffer for the main pass.
  void BeginScene() const;
  /// @brief Unbinds the HDR scene framebuffer.
  void EndScene() const;
  /// @brief Runs god rays + bloom + tone mapping to the default framebuffer.
  void PostProcess(const glm::mat4 &view, const glm::mat4 &proj) const;

  /// @brief Draws the skybox background.
  void RenderSkybox(const glm::mat4 &view, const glm::mat4 &proj) const;

  void SetExposure(float exposure);
  void SetBloomStrength(float strength);
  void SetBloomThreshold(float threshold);
  void SetShadowPcfRadius(float radius);
  void SetIblIntensity(float intensity);
  void SetGodRaysStrength(float strength);

  unsigned int GetFramebuffer() const;

 private:
  Ref<RenderPass>     pass_;
  Ref<RenderPipeline> pipeline_;
  Ref<Texture>        default_texture_;
  Ref<ShadowMap>      shadow_map_;
  Ref<Shader>         depth_shader_;
  std::vector<Ref<CubeShadowMap>> point_light_shadow_maps_;
  Ref<Shader>         point_light_depth_shader_;
  Ref<PostProcessing> post_processing_;
  Ref<Skybox>         skybox_;
  Ref<SSAO>           ssao_;
  DirectionalLight    light_;
  std::vector<PointLight> point_lights_;
  std::vector<SpotLight>  spot_lights_;
  float shadow_pcf_radius_ = 2.0f;
  float ibl_intensity_     = 1.0f;
  bool  ssao_enabled_      = false;
};

}  // namespace MEngine
