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

namespace MEngine {

struct Sprite2D;
struct AnimatedSprite2D;
class RenderPipeline;
class RenderPass;
class Mesh;
class Shader;
class Texture;

class Renderer {
 public:
  Renderer();
  ~Renderer();

  void RenderSprite(Sprite2D &sprite, const glm::mat4 &proj_view) const;
  void RenderSprite(AnimatedSprite2D &sprite, const glm::mat4 &proj_view) const;

  /// @brief Draw a 3D mesh with the given shader and optional texture.
  void DrawMesh(const Ref<Mesh> &mesh, const Ref<Shader> &shader, const Ref<Texture> &texture,
                const glm::mat4 &model, const glm::mat4 &proj_view, const glm::vec3 &view_pos) const;

  unsigned int GetFramebuffer() const;

 private:
  Ref<RenderPass>     pass_;
  Ref<RenderPipeline> pipeline_;
  Ref<Texture>        default_texture_;
};

}  // namespace MEngine
