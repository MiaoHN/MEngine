#include "scene/scene.hpp"

#include "render/gl.hpp"
#include "render/shader.hpp"
#include "render/texture.hpp"
#include "scene/component.hpp"

namespace MEngine {

Scene::Scene() { logger_ = Logger::Get("Scene"); }

Scene::~Scene() {}

void Scene::LoadScene(const std::string& path) {
  // TODO: Implement
}

void Scene::SaveScene(const std::string& path) {
  // TODO: Implement
}

}  // namespace MEngine
