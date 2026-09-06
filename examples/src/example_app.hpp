/**
 * @file example_app.hpp
 * @brief Shared host for the LearnOpenGL-style example executables.
 *
 * Each example is its own tiny executable (examples/src/<name>.cpp defines the
 * scene + CreateApplication). This class provides the common Application loop:
 * it builds the scene, renders it from an orbit camera, and lets you drag with
 * the right mouse button to look around - exactly the "watch one tutorial
 * scene" workflow.
 */

#pragma once

#include <functional>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "core/application.hpp"
#include "scene/scene.hpp"

namespace MEngine {
namespace examples {

class ExampleApp : public MEngine::Application {
 public:
  struct Setup {
    /// @brief Builds the demo scene (called once from Initialize).
    std::function<std::shared_ptr<MEngine::Scene>()> build;
    const char *name = "Example";
    glm::vec3   target{0.0f, 1.0f, 0.0f};
    float yaw   = 0.0f;
    float pitch = 18.0f;
    float dist  = 10.0f;
    float fov   = 55.0f;  // vertical FOV in degrees (LearnOpenGL demos use 45)
    /// @brief Optional per-frame hook (called each frame right before render
    /// with the camera `eye`/`front` of that frame), e.g. to animate a light
    /// or attach a flashlight to the camera. Kept last so the existing
    /// positional `Setup{ build, name, ... }` initializers still line up.
    std::function<void(MEngine::Scene &, const glm::vec3 &eye, const glm::vec3 &front, float dt)> update;
  };

  explicit ExampleApp(Setup setup);
  ~ExampleApp() override;

  void Initialize() override;
  void OnUpdate(float dt) override;

 private:
  Setup setup_;
  std::shared_ptr<MEngine::Scene> scene_;
  bool ready_ = false;

  glm::vec3 cam_target_{0.0f, 1.0f, 0.0f};
  float cam_yaw_   = 0.0f;
  float cam_pitch_ = 18.0f;
  float cam_dist_  = 10.0f;
};

}  // namespace examples
}  // namespace MEngine
