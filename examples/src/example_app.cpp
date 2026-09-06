#include "example_app.hpp"

#include <algorithm>
#include <cmath>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "core/input.hpp"
#include "core/logger.hpp"

namespace MEngine {
namespace examples {

ExampleApp::ExampleApp(Setup setup) : MEngine::Application(Application::GetStartupApi()), setup_(std::move(setup)) {}

ExampleApp::~ExampleApp() {}

void ExampleApp::Initialize() {
  scene_ = setup_.build();
  cam_target_ = setup_.target;
  cam_yaw_    = setup_.yaw;
  cam_pitch_  = setup_.pitch;
  cam_dist_   = setup_.dist;
  ready_      = scene_ != nullptr;
  LOG_INFO("Examples") << "Demo: " << setup_.name;
}

void ExampleApp::OnUpdate(float dt) {
  // Right-drag orbits the camera around the demo target.
  const glm::vec2 delta = Input::GetMouseDelta();
  if (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
    cam_yaw_ += delta.x * 0.25f;
    cam_pitch_ = std::clamp(cam_pitch_ - delta.y * 0.25f, -89.0f, 89.0f);
  }

  int fb_w = 0, fb_h = 0;
  glfwGetFramebufferSize(window_, &fb_w, &fb_h);
  const float aspect = (fb_h > 0) ? static_cast<float>(fb_w) / static_cast<float>(fb_h) : 16.0f / 9.0f;

  const float cy = std::cos(glm::radians(cam_pitch_));
  const glm::vec3 eye =
      cam_target_ + cam_dist_ * glm::vec3(cy * std::sin(glm::radians(cam_yaw_)), std::sin(glm::radians(cam_pitch_)),
                                          cy * std::cos(glm::radians(cam_yaw_)));
  const glm::mat4 view = glm::lookAt(eye, cam_target_, glm::vec3(0.0f, 1.0f, 0.0f));
  const glm::mat4 proj = glm::perspective(glm::radians(setup_.fov), aspect, 0.05f, 400.0f);

  if (ready_) {
    // Optional per-scene animation hook (move lights / objects, anchor a
    // flashlight to the camera) with this frame's camera pose.
    if (setup_.update) {
      setup_.update(*scene_, eye, glm::normalize(cam_target_ - eye), dt);
    }
    scene_->RenderMeshes(view, proj, eye);
  }
}

}  // namespace examples
}  // namespace MEngine
