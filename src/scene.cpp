#include "scene.hpp"

#include "component.hpp"
#include "gl.hpp"
#include "shader.hpp"

namespace MEngine {

Scene::Scene() {
  logger_ = Logger::Get("Scene");

  Entity entity = CreateEntity();

  auto shader = std::make_shared<Shader>("res/shaders/test_vert.glsl",
                                         "res/shaders/test_frag.glsl");

  // For test
  float vertices[] = {
      -0.5f, -0.5f, 0.0f,  // left
      0.5f,  -0.5f, 0.0f,  // right
      0.0f,  0.5f,  0.0f   // top
  };

  auto vertex_buffer = std::make_shared<GL::VertexBuffer>();
  vertex_buffer->SetData(vertices, sizeof(vertices));
  vertex_buffer->AddLayout({{GL::ShaderDataType::Float3, "aPos"}});

  auto vertex_array = std::make_shared<GL::VertexArray>();
  vertex_array->SetVertexBuffer(vertex_buffer);

  auto& render_info = entity.AddComponent<RenderInfo>();

  render_info.vertex_array = vertex_array;
  render_info.shader       = shader;

  entity.AddComponent<Transform>(glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(1.0f, 1.0f, 1.0f));

  camera_ = std::make_shared<OrthographicCamera>(-1.0f, 1.0f, -1.0f, 1.0f);
}

Scene::~Scene() {}

void Scene::LoadScene(const std::string& path) {
  // TODO: Implement
}

void Scene::SaveScene(const std::string& path) {
  // TODO: Implement
}

}  // namespace MEngine
