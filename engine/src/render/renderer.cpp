#include "render/renderer.hpp"

#include "core/command.hpp"
#include "render/material.hpp"
#include "render/mesh.hpp"
#include "render/rhi/resource_backend.hpp"
#include "render/rhi/rhi.hpp"
#include "render/render_pass.hpp"
#include "render/render_pipeline.hpp"
#include "render/shader.hpp"
#include "render/shadow_map.hpp"
#include "render/texture.hpp"
#include "scene/component.hpp"
#include "utils/profiler.h"

namespace MEngine {

Renderer::Renderer() {
  constexpr float vertices[] = {
      // positions        // texture coords
      0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  // top right
      0.5f,  -0.5f, 0.0f, 1.0f, 0.0f,  // bottom right
      -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,  // bottom left
      -0.5f, 0.5f,  0.0f, 0.0f, 1.0f   // top left
  };
  const unsigned int indices[] = {
      0, 1, 3,  // first triangle
      1, 2, 3   // second triangle
  };

  auto vertex_array = CreateVertexArrayBackend();
    vertex_array->SetVertexBuffer(vertices, sizeof(vertices),
                  {
                    {VertexAttributeType::Float3, "aPos"},
                    {VertexAttributeType::Float2, "aTexCoord"},
                  });
  vertex_array->SetIndexBuffer(indices, 6);

  // TODO: 默认 shader 怎么存放
  const auto shader = CreateRef<Shader>("res/shaders/default_vert.glsl", "res/shaders/default_frag.glsl");
  shader->Bind();
  shader->SetUniform("texture1", 0);
  MEngine::Shader::Unbind();

  pipeline_ = CreateRef<RenderPipeline>();

  pipeline_->SetVertexArray(std::move(vertex_array));
  pipeline_->SetShader(shader);

  pass_ = CreateRef<RenderPass>();
  pass_->AddPipeline(pipeline_);

  // 1x1 white fallback texture for meshes without a texture.
  unsigned char white[4] = {255, 255, 255, 255};
  default_texture_       = CreateRef<Texture>();
  default_texture_->SetData(white, 1, 1);

  // Directional shadow map + depth-only shader.
  shadow_map_    = CreateRef<ShadowMap>(2048, 2048);
  depth_shader_  = CreateRef<Shader>("res/shaders/shadow_depth_vert.glsl", "res/shaders/shadow_depth_frag.glsl");
}

Renderer::~Renderer() = default;

void Renderer::RenderSprite(Sprite2D &sprite, const glm::mat4 &proj_view) const {
  PROFILER_FUNCTION();

  static Ref<Texture> plain_texture = CreateRef<Texture>();
  if (!sprite.texture) {
    // 根据 sprite 颜色绘制纯色texture

    // 生成纯色纹理
    unsigned char color[4] = {static_cast<unsigned char>(sprite.color[0]), static_cast<unsigned char>(sprite.color[1]),
                              static_cast<unsigned char>(sprite.color[2]), static_cast<unsigned char>(sprite.color[3])};
    plain_texture->SetData(color, 1, 1);

    const auto shader = pipeline_->GetShader();

    shader->Bind();
    plain_texture->Bind();

    shader->SetUniform("model", sprite.GetModelMatrix());
    shader->SetUniform("proj_view", proj_view);
    shader->SetUniform("texture1", 0);

    pipeline_->Execute();
  } else {
    const auto shader  = pipeline_->GetShader();
    const auto texture = sprite.texture;

    shader->Bind();
    texture->Bind();

    shader->SetUniform("model", sprite.GetModelMatrix());
    shader->SetUniform("proj_view", proj_view);
    shader->SetUniform("texture1", 0);

    pipeline_->Execute();
  }
}

void Renderer::RenderSprite(AnimatedSprite2D &sprite, const glm::mat4 &proj_view) const {
  PROFILER_FUNCTION();

  const auto shader  = pipeline_->GetShader();
  const auto texture = sprite.texture;

  shader->Bind();
  texture->SetSubTexture(sprite.current_frame);

  shader->SetUniform("model", sprite.GetModelMatrix());
  shader->SetUniform("proj_view", proj_view);
  shader->SetUniform("texture1", 0);

  pipeline_->Execute();
  // pass_->Begin();

  // pass_->Execute();

  // pass_->End();

  // texture->Unbind();
}

void Renderer::BeginShadowPass(const glm::mat4 &light_view_proj) const {
  shadow_map_->Bind();
  depth_shader_->Bind();
  depth_shader_->SetUniform("light_view_proj", light_view_proj);
}

void Renderer::DrawMeshShadow(const Ref<Mesh> &mesh, const glm::mat4 &model, const glm::mat4 &light_view_proj) const {
  if (!mesh) {
    return;
  }
  (void)light_view_proj;
  depth_shader_->SetUniform("model", model);
  mesh->Bind();
  if (const auto *rhi = GetActiveRHI(); rhi) {
    rhi->DrawIndexedTriangles(mesh->GetIndexCount());
  }
  mesh->Unbind();
}

void Renderer::EndShadowPass() const {
  depth_shader_->Unbind();
  shadow_map_->Unbind();
}

void Renderer::DrawMesh(const Ref<Mesh> &mesh, const Ref<Material> &material, const glm::mat4 &model,
                        const glm::mat4 &proj_view, const glm::vec3 &view_pos, const glm::mat4 &light_view_proj) const {
  PROFILER_FUNCTION();

  if (!mesh || !material || !material->GetShader()) {
    return;
  }

  const Ref<Shader> &shader = material->GetShader();
  shader->Bind();

  const auto bind_texture = [&](const Ref<Texture> &texture, int slot, const char *map_uniform, const char *has_uniform) {
    const Ref<Texture> &tex = texture ? texture : default_texture_;
    tex->Bind(slot);
    shader->SetUniform(map_uniform, slot);
    shader->SetUniform(has_uniform, texture ? 1 : 0);
  };

  bind_texture(material->GetAlbedoMap(), 0, "albedo_map", "has_albedo_map");
  bind_texture(material->GetNormalMap(), 1, "normal_map", "has_normal_map");
  bind_texture(material->GetMetallicRoughnessMap(), 2, "metallic_roughness_map", "has_metallic_roughness_map");
  bind_texture(material->GetAOMap(), 3, "ao_map", "has_ao_map");

  shader->SetUniform("base_color_factor", material->GetBaseColorFactor());
  shader->SetUniform("metallic_factor", material->GetMetallicFactor());
  shader->SetUniform("roughness_factor", material->GetRoughnessFactor());

  shader->SetUniform("model", model);
  shader->SetUniform("proj_view", proj_view);
  shader->SetUniform("view_pos", view_pos);

  // Directional light + shadow map.
  shader->SetUniform("light_dir", light_.direction);
  shader->SetUniform("light_color", light_.color);
  shadow_map_->BindTexture(4);
  shader->SetUniform("shadow_map", 4);
  shader->SetUniform("light_view_proj", light_view_proj);

  mesh->Bind();
  if (const auto *rhi = GetActiveRHI(); rhi) {
    rhi->DrawIndexedTriangles(mesh->GetIndexCount());
  }
  mesh->Unbind();

  shader->Unbind();
}

unsigned int Renderer::GetFramebuffer() const { return pass_->GetFramebuffer(); }

}  // namespace MEngine