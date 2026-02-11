#include "rotating_cube.hpp"

#include "core/entry_point.hpp"
#include "core/input.hpp"
#include "scene/component.hpp"
#include "utils/profiler.h"

RotatingCube::RotatingCube() = default;

RotatingCube::~RotatingCube() = default;

void RotatingCube::Initialize() {
  PROFILER_FUNCTION();

  // Create scene and camera
  scene_ = CreateRef<Scene>();
  camera_ = CreateRef<Camera2D>(-1.6f, 1.6f, -0.9f, 0.9f, 1.0f, true);
  camera_->SetZoomLevel(2.0f);
  camera_->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));

  // Create a cube entity with a sprite (rendered as a 2D quad representing a cube face)
  cube_entity_ = scene_->CreateEntity("Rotating Cube");
  
  auto& sprite = cube_entity_.AddComponent<Sprite2D>();
  sprite.position = glm::vec3(0.0f, 0.0f, 0.0f);
  sprite.scale = glm::vec3(1.0f, 1.0f, 1.0f);
  sprite.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
  sprite.color = glm::vec4(0.2f, 0.6f, 0.9f, 1.0f);  // Blue color
  sprite.tiling_factor = 1.0f;

  // Setup ImGui for basic info display
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init("#version 330");
}

void RotatingCube::OnUpdate(float dt) {
  PROFILER_FUNCTION();

  // Handle escape key to close
  if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) {
    glfwSetWindowShouldClose(window_, true);
  }

  // Update cube rotation
  auto& sprite = cube_entity_.GetComponent<Sprite2D>();
  sprite.rotation.z += rotation_speed_ * dt;
  
  // Wrap rotation around 360 degrees
  if (sprite.rotation.z > 360.0f) {
    sprite.rotation.z -= 360.0f;
  }

  // Render the scene
  scene_->OnUpdateEditor(*camera_);

  // ImGui overlay showing info
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::Begin("Rotating Cube Info");
  ImGui::Text("FPS: %d", GetFPS());
  ImGui::Text("Rotation: %.1f degrees", sprite.rotation.z);
  ImGui::SliderFloat("Rotation Speed", &rotation_speed_, 0.0f, 360.0f, "%.1f deg/s");
  
  ImGui::Separator();
  ImGui::Text("Controls:");
  ImGui::Text("  ESC - Exit");
  
  ImGui::Separator();
  ImGui::ColorEdit4("Cube Color", glm::value_ptr(sprite.color));
  ImGui::DragFloat3("Scale", glm::value_ptr(sprite.scale), 0.1f, 0.1f, 5.0f);
  ImGui::DragFloat3("Position", glm::value_ptr(sprite.position), 0.1f);
  
  ImGui::End();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

Application *CreateApplication() { return new RotatingCube(); }
