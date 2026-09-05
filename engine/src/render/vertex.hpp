#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "render/rhi/resource_backend.hpp"

namespace MEngine {

/**
 * @brief A single interleaved vertex used by meshes.
 *
 * Layout: position (vec3) + normal (vec3) + texcoord (vec2).
 */
struct Vertex {
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 normal{0.0f, 0.0f, 1.0f};
  glm::vec2 texcoord{0.0f, 0.0f};

  /**
   * @brief Returns the attribute layout matching the Vertex memory layout.
   *
   * The returned order must match the memory order of the fields above so that
   * the backend can compute stride/offsets correctly.
   */
  static std::vector<VertexAttribute> GetLayout() {
    return {
        {VertexAttributeType::Float3, "aPos"},
        {VertexAttributeType::Float3, "aNormal"},
        {VertexAttributeType::Float2, "aTexCoord"},
    };
  }
};

}  // namespace MEngine
