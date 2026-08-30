#include "render/renderer.hpp"

#include "core/command.hpp"
#include "render/material.hpp"
#include "render/mesh.hpp"
#include "render/cube_shadow_map.hpp"
#include "render/post_processing.hpp"
#include "render/rhi/resource_backend.hpp"
#include "render/rhi/rhi.hpp"
#include "render/render_pass.hpp"
#include "render/render_pipeline.hpp"
#include "render/shader.hpp"
#include "render/shadow_map.hpp"
#include "render/skybox.hpp"
#include "render/ssao.hpp"
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

  // Omnidirectional (point light) shadow maps + depth shader.
  point_light_shadow_maps_.reserve(kMaxPointShadows);
  for (int i = 0; i < kMaxPointShadows; ++i) {
    point_light_shadow_maps_.push_back(CreateRef<CubeShadowMap>(1024));
  }
  point_light_depth_shader_ =
      CreateRef<Shader>("res/shaders/point_shadow_depth_vert.glsl", "res/shaders/point_shadow_depth_frag.glsl");

  // HDR + bloom post-processing (auto-sizes to the window).
  post_processing_ = CreateRef<PostProcessing>(0, 0);

  // Screen-space ambient occlusion.
  ssao_ = CreateRef<SSAO>(0, 0);

  // Skybox + IBL environment (equirectangular HDR).
  skybox_ = CreateRef<Skybox>("res/textures/hdr/kloppenheim_06_puresky_1k.hdr");
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

int Renderer::GetPointShadowIndex(int light_index) const {
  if (light_index < 0 || light_index >= static_cast<int>(point_lights_.size())) {
    return -1;
  }
  if (!point_lights_[static_cast<size_t>(light_index)].casts_shadow) {
    return -1;
  }
  int shadow_index = 0;
  for (int i = 0; i < light_index; ++i) {
    if (point_lights_[static_cast<size_t>(i)].casts_shadow) {
      ++shadow_index;
    }
  }
  return shadow_index < kMaxPointShadows ? shadow_index : -1;
}

void Renderer::BeginPointShadowPass(int light_index, const glm::vec3 &light_pos, float far_plane) const {
  point_light_shadow_maps_[static_cast<size_t>(light_index)]->Bind();
  point_light_depth_shader_->Bind();
  point_light_depth_shader_->SetUniform("light_pos", light_pos);
  point_light_depth_shader_->SetUniform("far_plane", far_plane);
}

void Renderer::BindPointShadowFace(int light_index, int face, const glm::mat4 &light_space_matrix) const {
  point_light_shadow_maps_[static_cast<size_t>(light_index)]->BindFace(face);
  point_light_depth_shader_->SetUniform("light_space_matrix", light_space_matrix);
}

void Renderer::DrawMeshPointShadow(const Ref<Mesh> &mesh, const glm::mat4 &model) const {
  if (!mesh) {
    return;
  }
  point_light_depth_shader_->SetUniform("model", model);
  mesh->Bind();
  if (const auto *rhi = GetActiveRHI(); rhi) {
    rhi->DrawIndexedTriangles(mesh->GetIndexCount());
  }
  mesh->Unbind();
}

void Renderer::EndPointShadowPass(int light_index) const {
  point_light_depth_shader_->Unbind();
  point_light_shadow_maps_[static_cast<size_t>(light_index)]->Unbind();
}

void Renderer::BeginSSAOPass(const glm::mat4 &proj, const glm::mat4 &view) const {
  ssao_->BeginGeometryPass(proj, view);
}

void Renderer::DrawMeshSSAO(const Ref<Mesh> &mesh, const glm::mat4 &model) const {
  if (!mesh) {
    return;
  }
  ssao_->SetGeometryModel(model);
  mesh->Bind();
  if (const auto *rhi = GetActiveRHI(); rhi) {
    rhi->DrawIndexedTriangles(mesh->GetIndexCount());
  }
  mesh->Unbind();
}

void Renderer::EndSSAOPass() const { ssao_->EndGeometryPass(); }

void Renderer::GenerateSSAO(const glm::mat4 &proj, const glm::mat4 &view) const { ssao_->Generate(proj, view); }

void Renderer::BindSSAO(unsigned int slot) const { ssao_->BindTexture(slot); }

void Renderer::BeginScene() const { post_processing_->BeginScene(); }

void Renderer::EndScene() const { post_processing_->EndScene(); }

void Renderer::PostProcess(const glm::mat4 &view, const glm::mat4 &proj) const {
  // The directional light travels along `direction`, so the sun is opposite.
  // Project it to screen space as the god-rays light source (falls back to the
  // center when the sun is behind the camera).
  const glm::vec4 sun_clip = proj * view * glm::vec4(-light_.direction, 0.0f);
  glm::vec2       light_pos(0.5f, 0.5f);
  if (sun_clip.w > 0.0f) {
    const glm::vec2 ndc = glm::vec2(sun_clip.x, sun_clip.y) / sun_clip.w;
    light_pos           = ndc * 0.5f + 0.5f;
  }
  post_processing_->Render(light_pos);
}

void Renderer::RenderSkybox(const glm::mat4 &view, const glm::mat4 &proj) const { skybox_->Render(view, proj); }

void Renderer::ResolveTAA() const { post_processing_->ResolveTAA(); }

glm::mat4 Renderer::GetJitteredProjection(const glm::mat4 &proj) const {
  glm::mat4       jittered = proj;
  const glm::vec2 jitter   = post_processing_->GetJitter();
  jittered[2][0]           = jitter.x;
  jittered[2][1]           = jitter.y;
  return jittered;
}

void Renderer::SetTAAEnabled(bool enabled) { post_processing_->SetTAAEnabled(enabled); }

bool Renderer::IsTAAEnabled() const { return post_processing_->IsTAAEnabled(); }

void Renderer::SetExposure(float exposure) { post_processing_->SetExposure(exposure); }

void Renderer::SetBloomStrength(float strength) { post_processing_->SetBloomStrength(strength); }

void Renderer::SetBloomThreshold(float threshold) { post_processing_->SetBloomThreshold(threshold); }

void Renderer::SetShadowPcfRadius(float radius) { shadow_pcf_radius_ = radius; }

void Renderer::SetIblIntensity(float intensity) { ibl_intensity_ = intensity; }

void Renderer::SetGodRaysStrength(float strength) { post_processing_->SetGodRaysStrength(strength); }

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
  shader->SetUniform("shadow_map_size", static_cast<float>(shadow_map_->GetWidth()));
  shader->SetUniform("shadow_pcf_radius", shadow_pcf_radius_);

  // IBL environment (irradiance + prefiltered specular cubemaps).
  skybox_->BindIrradiance(5);
  skybox_->BindPrefilter(6);
  shader->SetUniform("irradiance_map", 5);
  shader->SetUniform("prefiltered_map", 6);
  shader->SetUniform("max_prefilter_mip", skybox_->GetMaxPrefilterMip());
  shader->SetUniform("ibl_intensity", ibl_intensity_);

  // Screen-space ambient occlusion.
  ssao_->BindTexture(7);
  shader->SetUniform("ssao_map", 7);
  shader->SetUniform("ssao_enabled", ssao_enabled_ ? 1 : 0);

  // Point lights (indexed uniform arrays, capped to the shader's MAX).
  constexpr int kMaxPointLights = 8;
  const int     point_light_count = static_cast<int>(point_lights_.size()) < kMaxPointLights
                                         ? static_cast<int>(point_lights_.size())
                                         : kMaxPointLights;
  shader->SetUniform("point_light_count", point_light_count);
  for (int i = 0; i < point_light_count; ++i) {
    const PointLight  &light = point_lights_[static_cast<size_t>(i)];
    const std::string  index = std::to_string(i);
    shader->SetUniform("point_light_positions[" + index + "]", light.position);
    shader->SetUniform("point_light_colors[" + index + "]", light.color);
    shader->SetUniform("point_light_intensities[" + index + "]", light.intensity);
    shader->SetUniform("point_light_radii[" + index + "]", light.radius);

    const int shadow_index = GetPointShadowIndex(i);
    const int has_shadow   = shadow_index >= 0 ? 1 : 0;
    shader->SetUniform("point_light_has_shadow[" + index + "]", has_shadow);
    if (has_shadow) {
      const int slot = 8 + shadow_index;
      point_light_shadow_maps_[static_cast<size_t>(shadow_index)]->BindTexture(slot);
      shader->SetUniform("point_light_shadow_maps[" + index + "]", slot);
    }
    shader->SetUniform("point_light_far_planes[" + index + "]", light.radius);
  }

  // Spot lights (indexed uniform arrays, capped to the shader's MAX).
  constexpr int kMaxSpotLights = 4;
  const int     spot_light_count = static_cast<int>(spot_lights_.size()) < kMaxSpotLights
                                        ? static_cast<int>(spot_lights_.size())
                                        : kMaxSpotLights;
  shader->SetUniform("spot_light_count", spot_light_count);
  for (int i = 0; i < spot_light_count; ++i) {
    const SpotLight  &light = spot_lights_[static_cast<size_t>(i)];
    const std::string index = std::to_string(i);
    shader->SetUniform("spot_light_positions[" + index + "]", light.position);
    shader->SetUniform("spot_light_directions[" + index + "]", light.direction);
    shader->SetUniform("spot_light_colors[" + index + "]", light.color);
    shader->SetUniform("spot_light_intensities[" + index + "]", light.intensity);
    shader->SetUniform("spot_light_ranges[" + index + "]", light.range);
    shader->SetUniform("spot_light_cutoffs[" + index + "]", light.cutoff);
    shader->SetUniform("spot_light_outer_cutoffs[" + index + "]", light.outer_cutoff);
  }

  mesh->Bind();
  if (const auto *rhi = GetActiveRHI(); rhi) {
    rhi->DrawIndexedTriangles(mesh->GetIndexCount());
  }
  mesh->Unbind();

  shader->Unbind();
}

unsigned int Renderer::GetFramebuffer() const { return pass_->GetFramebuffer(); }

}  // namespace MEngine