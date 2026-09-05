#include "sandbox.hpp"
#include "utils/profiler.h"

Sandbox::Sandbox() : Application(GraphicsAPI::OpenGL) {
  active_scene_ = std::make_shared<Scene>();

  Entity camera_entity = active_scene_->CreateEntity("Camera Entity");
  auto  &camera        = camera_entity.AddComponent<Camera2D>();
  camera.primary       = true;
  camera.zoom_level    = 1.0f;

  rotating_square_ = active_scene_->CreateEntity("Rotating Square");
  auto &sprite     = rotating_square_.AddComponent<Sprite2D>();
  sprite.position  = glm::vec3(0.0f, 0.0f, 0.0f);
  sprite.scale     = glm::vec3(0.5f, 0.5f, 1.0f);
  sprite.rotation  = glm::vec3(0.0f, 0.0f, 0.0f);
  sprite.color     = glm::vec4(128.0f, 120.0f, 70.0f, 255.0f);
  sprite.texture   = Texture::Create("res/textures/checkerboard.png");
}

Sandbox::~Sandbox() {}

void Sandbox::Initialize() {}

void Sandbox::OnUpdate(float dt) {
  PROFILER_FUNCTION();

  if (rotating_square_.HasComponent<Sprite2D>()) {
    auto &sprite = rotating_square_.GetComponent<Sprite2D>();
    sprite.rotation.z += rotation_speed_ * dt;
  }

  active_scene_->OnUpdateRuntime(dt, viewport_width_, viewport_height_);
}

Application *CreateApplication() { return new Sandbox(); }
