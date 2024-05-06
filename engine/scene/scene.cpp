#include "scene/scene.hpp"

#include "render/gl.hpp"
#include "render/shader.hpp"
#include "render/texture.hpp"
#include "scene/component.hpp"

namespace MEngine {

Scene::Scene() {
  logger_ = Logger::Get("Scene");

  Entity entity = CreateEntity();

  auto shader = std::make_shared<Shader>("res/shaders/test_vert.glsl",
                                         "res/shaders/test_frag.glsl");

  auto texture = std::make_shared<Texture>("res/textures/checkerboard.png");

  // checkboard six vertices with uv for test
  float vertices[] = {
      // positions        // texture coords
      0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  // top right
      0.5f,  -0.5f, 0.0f, 1.0f, 0.0f,  // bottom right
      -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,  // bottom left
      -0.5f, 0.5f,  0.0f, 0.0f, 1.0f   // top left
  };
  unsigned int indices[] = {
      0, 1, 3,  // first triangle
      1, 2, 3   // second triangle
  };

  auto vertex_buffer = std::make_shared<GL::VertexBuffer>();
  vertex_buffer->SetData(vertices, sizeof(vertices));
  vertex_buffer->AddLayout({
      {GL::ShaderDataType::Float3, "aPos"},
      {GL::ShaderDataType::Float2, "aTexCoord"},
  });

  auto index_buffer = std::make_shared<GL::IndexBuffer>();
  index_buffer->SetData(indices, 6);

  auto vertex_array = std::make_shared<GL::VertexArray>();
  vertex_array->SetVertexBuffer(vertex_buffer);
  vertex_array->SetIndexBuffer(index_buffer);

  auto& render_info = entity.AddComponent<RenderInfo>();

  render_info.texture      = texture;
  render_info.shader       = shader;
  render_info.vertex_array = vertex_array;

  entity.AddComponent<Transform>(glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(1.0f, 1.0f, 1.0f));

  entity.AddComponent<Tag>("checkerboard");

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
