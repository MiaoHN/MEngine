#pragma once

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
 */
struct PointLight {
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 color{1.0f, 1.0f, 1.0f};
  float     intensity = 1.0f;
  float     radius    = 4.0f;
};

}  // namespace MEngine
