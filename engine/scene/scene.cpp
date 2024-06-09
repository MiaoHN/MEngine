#include "scene/scene.hpp"

#include "render/gl.hpp"
#include "render/shader.hpp"
#include "render/texture.hpp"
#include "scene/component.hpp"

namespace MEngine {

Scene::Scene() {
  logger_ = Logger::Get("Scene");
  default_camera_info_ =
      std::make_shared<Camera2D>(-1.6f, 1.6f, -0.9f, 0.9f, 1.0f, true);
}

Scene::~Scene() {}

void Scene::LoadScene(const std::string& path) {
  // TODO: Implement
}

void Scene::SaveScene(const std::string& path) {
  // TODO: Implement
}

}  // namespace MEngine
