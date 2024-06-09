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

#include <glm/glm.hpp>
#include <memory>

#include "core/logger.hpp"
#include "render/gl.hpp"

namespace MEngine {

struct Sprite2D;
struct AnimatedSprite2D;
class RenderPipeline;

class Renderer {
 public:
  Renderer();
  ~Renderer();

  void RenderSprite(Sprite2D& sprite, const glm::mat4& proj_view);
  void RenderSprite(AnimatedSprite2D& sprite, const glm::mat4& proj_view);

 private:
  std::shared_ptr<RenderPipeline> pipeline_;

  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace MEngine
