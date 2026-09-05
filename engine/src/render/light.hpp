#pragma once

#include <array>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace MEngine {

/**
 * @brief A directional light used for lighting and shadow mapping.
 *
 * `direction` points the way the light travels (i.e. away from the sun).
 */
struct DirectionalLight {
  glm::vec3 direction = glm::normalize(glm::vec3(-0.3f, -1.0f, -0.4f));
  glm::vec3 color     = glm::vec3(2.5f);

  /// @brief Orthographic light-space matrix covering a sphere of `radius`
  /// around `scene_center`, used to render and sample the shadow map.
  [[nodiscard]] glm::mat4 GetLightSpaceMatrix(const glm::vec3 &scene_center, float radius) const {
    const glm::vec3 light_pos = scene_center - direction * radius;
    const glm::mat4 view      = glm::lookAt(light_pos, scene_center, glm::vec3(0.0f, 1.0f, 0.0f));

    const float     half = radius * 1.5f;
    const glm::mat4 proj = glm::ortho(-half, half, -half, half, radius * 0.01f, radius * 3.0f);
    return proj * view;
  }
};

/**
 * @brief A point light (positional light with distance attenuation).
 *
 * `radius` bounds the light's reach; beyond it the contribution fades to zero.
 * `casts_shadow` enables an omnidirectional cube shadow map for this light.
 */
struct PointLight {
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 color{1.0f, 1.0f, 1.0f};
  float     intensity = 1.0f;
  float     radius    = 4.0f;
  bool      casts_shadow = false;

  /// @brief Six perspective view-projection matrices (one per cube face, in
  /// GL_TEXTURE_CUBE_MAP_* order) for omnidirectional shadow mapping.
  [[nodiscard]] std::array<glm::mat4, 6> GetShadowMatrices() const {
    const float     near = glm::max(radius * 0.01f, 0.01f);
    const glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, near, radius);

    const glm::vec3 directions[6] = {
        glm::vec3(1.0f, 0.0f, 0.0f),   // +X
        glm::vec3(-1.0f, 0.0f, 0.0f),  // -X
        glm::vec3(0.0f, 1.0f, 0.0f),   // +Y
        glm::vec3(0.0f, -1.0f, 0.0f),  // -Y
        glm::vec3(0.0f, 0.0f, 1.0f),   // +Z
        glm::vec3(0.0f, 0.0f, -1.0f),  // -Z
    };
    const glm::vec3 ups[6] = {
        glm::vec3(0.0f, -1.0f, 0.0f),  // +X
        glm::vec3(0.0f, -1.0f, 0.0f),  // -X
        glm::vec3(0.0f, 0.0f, 1.0f),   // +Y
        glm::vec3(0.0f, 0.0f, -1.0f),  // -Y
        glm::vec3(0.0f, -1.0f, 0.0f),  // +Z
        glm::vec3(0.0f, -1.0f, 0.0f),  // -Z
    };

    std::array<glm::mat4, 6> out{};
    for (int face = 0; face < 6; ++face) {
      out[face] = proj * glm::lookAt(position, position + directions[face], ups[face]);
    }
    return out;
  }
};

/**
 * @brief A spot light (positional light with a cone falloff).
 *
 * `cutoff` / `outer_cutoff` are cosines of the inner/outer cone half angles;
 * `direction` points the way the light travels.
 */
struct SpotLight {
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 direction{0.0f, -1.0f, 0.0f};
  glm::vec3 color{1.0f, 1.0f, 1.0f};
  float     intensity    = 1.0f;
  float     range        = 8.0f;
  float     cutoff       = glm::cos(glm::radians(12.5f));
  float     outer_cutoff = glm::cos(glm::radians(17.5f));
};

}  // namespace MEngine
