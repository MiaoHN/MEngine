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

  /// @brief Draw a 3D mesh with the given PBR material (shadowed by the light).
  void DrawMesh(const Ref<Mesh> &mesh, const Ref<Material> &material, const glm::mat4 &model,
                const glm::mat4 &proj_view, const glm::vec3 &view_pos, const glm::mat4 &light_view_proj) const;

  [[nodiscard]] const DirectionalLight &GetLight() const { return light_; }
  DirectionalLight &GetLight() { return light_; }
  void SetLight(const DirectionalLight &light) { light_ = light; }

  void AddPointLight(const PointLight &light) { point_lights_.push_back(light); }
  void ClearPointLights() { point_lights_.clear(); }

  /// @brief Binds the HDR scene framebuffer for the main pass.
  void BeginScene() const;
  /// @brief Unbinds the HDR scene framebuffer.
  void EndScene() const;
  /// @brief Runs bloom + tone mapping to the default framebuffer.
  void PostProcess() const;

  unsigned int GetFramebuffer() const;

 private:
  Ref<RenderPass>     pass_;
  Ref<RenderPipeline> pipeline_;
  Ref<Texture>        default_texture_;
  Ref<ShadowMap>      shadow_map_;
  Ref<Shader>         depth_shader_;
  Ref<PostProcessing> post_processing_;
  DirectionalLight    light_;
  std::vector<PointLight> point_lights_;
};

}  // namespace MEngine
