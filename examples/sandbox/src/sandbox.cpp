#include "sandbox.hpp"

#include "core/entry_point.hpp"
#include "core/input.hpp"
#include "scene/component.hpp"
#include "utils/profiler.h"

namespace {
constexpr float kMinZoomLevel = 0.5f;
}

Sandbox::Sandbox() = default;

Sandbox::~Sandbox() = default;

void Sandbox::Initialize() {
  PROFILER_FUNCTION();

  // Create scene and camera
  scene_ = CreateRef<Scene>();
  camera_ = CreateRef<Camera2D>(-1.6f, 1.6f, -0.9f, 0.9f, 1.0f, true);
  camera_->SetZoomLevel(5.0f);
  camera_->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));

  // Create framebuffer for rendering
  frame_buffer_ = CreateRef<FrameBuffer>();

  // Create test entities
  CreateTestEntities();

  // Setup ImGui
  SetupImGui();
}

void Sandbox::SetupImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init("#version 330");
}

void Sandbox::CreateTestEntities() {
  // Create multiple entities in a grid pattern
  const int grid_size = 3;
  const float spacing = 2.0f;
  
  for (int x = -grid_size; x <= grid_size; ++x) {
    for (int y = -grid_size; y <= grid_size; ++y) {
      std::string name = "Entity_" + std::to_string(x + grid_size) + "_" + std::to_string(y + grid_size);
      Entity entity = scene_->CreateEntity(name);
      
      auto& sprite = entity.AddComponent<Sprite2D>();
      sprite.position = glm::vec3(x * spacing, y * spacing, 0.0f);
      sprite.scale = glm::vec3(0.8f, 0.8f, 1.0f);
      sprite.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
      
      // Create a colorful gradient based on position
      float r = (x + grid_size) / (float)(grid_size * 2);
      float g = (y + grid_size) / (float)(grid_size * 2);
      float b = 0.5f;
      sprite.color = glm::vec4(r, g, b, 1.0f);
      sprite.tiling_factor = 1.0f;
      
      test_entities_.push_back(entity);
    }
  }
  
  // Create a special center entity
  Entity center = scene_->CreateEntity("Center Entity");
  auto& center_sprite = center.AddComponent<Sprite2D>();
  center_sprite.position = glm::vec3(0.0f, 0.0f, 0.1f);  // Slightly in front
  center_sprite.scale = glm::vec3(1.5f, 1.5f, 1.0f);
  center_sprite.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
  center_sprite.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.8f);  // White with transparency
  test_entities_.push_back(center);
}

void Sandbox::OnUpdate(float dt) {
  PROFILER_FUNCTION();

  // Handle input
  if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) {
    glfwSetWindowShouldClose(window_, true);
  }

  // Camera movement with WASD
  glm::vec3 camera_pos = camera_->GetPosition();
  if (Input::IsKeyPressed(GLFW_KEY_W)) {
    camera_pos.y += camera_speed_ * dt;
  }
  if (Input::IsKeyPressed(GLFW_KEY_S)) {
    camera_pos.y -= camera_speed_ * dt;
  }
  if (Input::IsKeyPressed(GLFW_KEY_A)) {
    camera_pos.x -= camera_speed_ * dt;
  }
  if (Input::IsKeyPressed(GLFW_KEY_D)) {
    camera_pos.x += camera_speed_ * dt;
  }
  camera_->SetPosition(camera_pos);

  // Camera zoom with Q/E
  float zoom = camera_->GetZoomLevel();
  if (Input::IsKeyPressed(GLFW_KEY_Q)) {
    zoom += camera_zoom_speed_ * dt;
  }
  if (Input::IsKeyPressed(GLFW_KEY_E)) {
    zoom -= camera_zoom_speed_ * dt;
    if (zoom < kMinZoomLevel) zoom = kMinZoomLevel;
  }
  camera_->SetZoomLevel(zoom);

  // Animate entities if enabled
  if (animate_entities_) {
    animation_time_ += dt;
    for (size_t i = 0; i < test_entities_.size(); ++i) {
      if (test_entities_[i].HasComponent<Sprite2D>()) {
        auto& sprite = test_entities_[i].GetComponent<Sprite2D>();
        // Rotate each entity at a different rate
        float rotation_speed = 30.0f + (i * 5.0f);
        sprite.rotation.z = fmod(animation_time_ * rotation_speed, 360.0f);
        
        // Subtle scale pulsing
        float scale_factor = 1.0f + 0.1f * sin(animation_time_ * 2.0f + i * 0.5f);
        sprite.scale.x = 0.8f * scale_factor;
        sprite.scale.y = 0.8f * scale_factor;
      }
    }
  }

  // Render scene
  scene_->OnUpdateEditor(*camera_);

  // Render ImGui
  RenderImGui();
}

void Sandbox::RenderImGui() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  // Main control panel
  ImGui::Begin("Sandbox Controls");
  
  ImGui::Text("FPS: %d", GetFPS());
  ImGui::Separator();
  
  // Camera controls
  if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
    glm::vec3 pos = camera_->GetPosition();
    if (ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.1f)) {
      camera_->SetPosition(pos);
    }
    
    float zoom = camera_->GetZoomLevel();
    if (ImGui::DragFloat("Zoom Level", &zoom, 0.1f, kMinZoomLevel, 20.0f)) {
      camera_->SetZoomLevel(zoom);
    }
    
    float rotation = camera_->GetRotation();
    if (ImGui::DragFloat("Rotation", &rotation, 1.0f, -180.0f, 180.0f)) {
      camera_->SetRotation(rotation);
    }
    
    ImGui::SliderFloat("Move Speed", &camera_speed_, 1.0f, 20.0f);
    ImGui::SliderFloat("Zoom Speed", &camera_zoom_speed_, 0.1f, 5.0f);
  }
  
  // Animation controls
  if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox("Animate Entities", &animate_entities_);
    ImGui::Text("Animation Time: %.2f", animation_time_);
    if (ImGui::Button("Reset Animation")) {
      animation_time_ = 0.0f;
    }
  }
  
  // Entity list
  if (ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("Total Entities: %zu", test_entities_.size());
    
    if (ImGui::Button("Add Entity")) {
      Entity entity = scene_->CreateEntity("New Entity");
      auto& sprite = entity.AddComponent<Sprite2D>();
      sprite.position = glm::vec3(0.0f, 0.0f, 0.0f);
      sprite.scale = glm::vec3(1.0f, 1.0f, 1.0f);
      sprite.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
      sprite.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
      test_entities_.push_back(entity);
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Clear All") && !test_entities_.empty()) {
      for (auto& entity : test_entities_) {
        scene_->DestroyEntity(entity);
      }
      test_entities_.clear();
    }
  }
  
  // Controls help
  if (ImGui::CollapsingHeader("Controls")) {
    ImGui::Text("WASD - Move Camera");
    ImGui::Text("Q/E  - Zoom Out/In");
    ImGui::Text("ESC  - Exit");
  }
  
  ImGui::Separator();
  ImGui::Checkbox("Show ImGui Demo", &show_demo_window_);
  
  ImGui::End();
  
  // Show ImGui demo window if enabled
  if (show_demo_window_) {
    ImGui::ShowDemoWindow(&show_demo_window_);
  }

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

Application *CreateApplication() { return new Sandbox(); }
