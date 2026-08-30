#include "render/skybox.hpp"

#include <cmath>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>

#include "core/logger.hpp"
#include "render/mesh.hpp"
#include "render/rhi/rhi.hpp"
#include "render/shader.hpp"

namespace MEngine {

namespace {
constexpr unsigned int kCubeTargets[6] = {
    GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
};

glm::mat4 CaptureView(int face) {
  static const glm::vec3 targets[6] = {
      {1.0f, 0.0f, 0.0f},  {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
      {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},  {0.0f, 0.0f, -1.0f},
  };
  static const glm::vec3 ups[6] = {
      {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
      {0.0f, 0.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
  };
  return glm::lookAt(glm::vec3(0.0f), targets[face], ups[face]);
}

glm::mat4 CaptureProjection() { return glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f); }
}  // namespace

Skybox::Skybox(const std::string &hdr_path, int env_size, int irradiance_size, int prefilter_size)
    : env_size_(env_size), irradiance_size_(irradiance_size), prefilter_size_(prefilter_size) {
  // Load the equirectangular HDR as a floating-point 2D texture.
  int    width = 0, height = 0, channels = 0;
  float *hdr_data = stbi_loadf(hdr_path.c_str(), &width, &height, &channels, 4);
  if (!hdr_data) {
    LOG_WARN("Skybox") << "Failed to load HDR environment: " << hdr_path;
  }

  glGenTextures(1, &equirect_texture_);
  glBindTexture(GL_TEXTURE_2D, equirect_texture_);
  if (hdr_data) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, hdr_data);
    stbi_image_free(hdr_data);
  } else {
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1, 1, 0, GL_RGBA, GL_FLOAT, black);
  }
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Environment cubemap (source for the background and the IBL passes).
  glGenTextures(1, &env_cubemap_);
  glBindTexture(GL_TEXTURE_CUBE_MAP, env_cubemap_);
  for (int i = 0; i < 6; ++i) {
    glTexImage2D(kCubeTargets[i], 0, GL_RGBA16F, env_size_, env_size_, 0, GL_RGBA, GL_FLOAT, nullptr);
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  // Irradiance cubemap (diffuse IBL).
  glGenTextures(1, &irradiance_cubemap_);
  glBindTexture(GL_TEXTURE_CUBE_MAP, irradiance_cubemap_);
  for (int i = 0; i < 6; ++i) {
    glTexImage2D(kCubeTargets[i], 0, GL_RGBA16F, irradiance_size_, irradiance_size_, 0, GL_RGBA, GL_FLOAT, nullptr);
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  // Prefiltered cubemap (specular IBL, one mip level per roughness).
  glGenTextures(1, &prefilter_cubemap_);
  glBindTexture(GL_TEXTURE_CUBE_MAP, prefilter_cubemap_);
  for (int i = 0; i < 6; ++i) {
    glTexImage2D(kCubeTargets[i], 0, GL_RGBA16F, prefilter_size_, prefilter_size_, 0, GL_RGBA, GL_FLOAT, nullptr);
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

  // Shared capture framebuffer + depth renderbuffer for all cube passes.
  glGenFramebuffers(1, &capture_fbo_);
  glGenRenderbuffers(1, &capture_rbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo_);
  glBindRenderbuffer(GL_RENDERBUFFER, capture_rbo_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, env_size_, env_size_);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, capture_rbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  cube_              = Mesh::CreateCube(1.0f);
  skybox_shader_     = CreateRef<Shader>("res/shaders/skybox_vert.glsl", "res/shaders/skybox_frag.glsl");
  irradiance_shader_ = CreateRef<Shader>("res/shaders/skybox_vert.glsl", "res/shaders/irradiance_frag.glsl");
  prefilter_shader_  = CreateRef<Shader>("res/shaders/skybox_vert.glsl", "res/shaders/prefilter_frag.glsl");
  equirect_shader_   = CreateRef<Shader>("res/shaders/skybox_vert.glsl", "res/shaders/equirect_to_cube_frag.glsl");

  GenerateEnvironment();
  GenerateIrradiance();
  GeneratePrefilter();
}

Skybox::~Skybox() {
  glDeleteTextures(1, &env_cubemap_);
  glDeleteTextures(1, &irradiance_cubemap_);
  glDeleteTextures(1, &prefilter_cubemap_);
  glDeleteTextures(1, &equirect_texture_);
  glDeleteFramebuffers(1, &capture_fbo_);
  glDeleteRenderbuffers(1, &capture_rbo_);
}

void Skybox::RenderCube() const {
  cube_->Bind();
  if (const auto *rhi = GetActiveRHI(); rhi) {
    rhi->DrawIndexedTriangles(cube_->GetIndexCount());
  }
  cube_->Unbind();
}

void Skybox::GenerateEnvironment() {
  const glm::mat4 capture_proj = CaptureProjection();

  equirect_shader_->Bind();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, equirect_texture_);
  equirect_shader_->SetUniform("equirectangular_map", 0);
  equirect_shader_->SetUniform("proj", capture_proj);

  glViewport(0, 0, env_size_, env_size_);
  glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo_);
  for (int i = 0; i < 6; ++i) {
    equirect_shader_->SetUniform("view", CaptureView(i));
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, env_cubemap_, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    RenderCube();
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glBindTexture(GL_TEXTURE_CUBE_MAP, env_cubemap_);
  glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
}

void Skybox::GenerateIrradiance() {
  const glm::mat4 capture_proj = CaptureProjection();

  irradiance_shader_->Bind();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, env_cubemap_);
  irradiance_shader_->SetUniform("environment", 0);
  irradiance_shader_->SetUniform("proj", capture_proj);

  glViewport(0, 0, irradiance_size_, irradiance_size_);
  glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo_);
  for (int i = 0; i < 6; ++i) {
    irradiance_shader_->SetUniform("view", CaptureView(i));
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                           irradiance_cubemap_, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    RenderCube();
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Skybox::GeneratePrefilter() {
  const glm::mat4 capture_proj = CaptureProjection();

  prefilter_shader_->Bind();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, env_cubemap_);
  prefilter_shader_->SetUniform("environment", 0);
  prefilter_shader_->SetUniform("proj", capture_proj);

  glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo_);
  for (int mip = 0; mip < prefilter_mip_levels_; ++mip) {
    const int mip_size = static_cast<int>(prefilter_size_ * std::pow(0.5, mip));
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mip_size, mip_size);
    glViewport(0, 0, mip_size, mip_size);

    const float roughness = static_cast<float>(mip) / static_cast<float>(prefilter_mip_levels_ - 1);
    prefilter_shader_->SetUniform("roughness", roughness);

    for (int i = 0; i < 6; ++i) {
      prefilter_shader_->SetUniform("view", CaptureView(i));
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                             prefilter_cubemap_, mip);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      RenderCube();
    }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Skybox::Render(const glm::mat4 &view, const glm::mat4 &proj) const {
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);

  skybox_shader_->Bind();
  skybox_shader_->SetUniform("view", glm::mat4(glm::mat3(view)));
  skybox_shader_->SetUniform("proj", proj);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, env_cubemap_);
  skybox_shader_->SetUniform("skybox", 0);
  RenderCube();

  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);
}

void Skybox::BindEnvironment(unsigned int slot) const {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_CUBE_MAP, env_cubemap_);
}

void Skybox::BindIrradiance(unsigned int slot) const {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_CUBE_MAP, irradiance_cubemap_);
}

void Skybox::BindPrefilter(unsigned int slot) const {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_CUBE_MAP, prefilter_cubemap_);
}

}  // namespace MEngine
