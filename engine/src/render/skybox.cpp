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
}  // namespace

Skybox::Skybox(const std::array<std::string, 6> &faces, int face_size) {
  // Environment cubemap.
  glGenTextures(1, &env_cubemap_);
  glBindTexture(GL_TEXTURE_CUBE_MAP, env_cubemap_);

  for (int i = 0; i < 6; ++i) {
    int            width = 0, height = 0, channels = 0;
    unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &channels, 4);
    if (!data) {
      LOG_WARN("Skybox") << "Failed to load skybox face: " << faces[i];
      continue;
    }
    glTexImage2D(kCubeTargets[i], 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

  max_mip_level_ = static_cast<float>(std::log2(face_size));

  // Irradiance cubemap.
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

  // Capture framebuffer for the irradiance convolution.
  glGenFramebuffers(1, &capture_fbo_);
  glGenRenderbuffers(1, &capture_rbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo_);
  glBindRenderbuffer(GL_RENDERBUFFER, capture_rbo_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, irradiance_size_, irradiance_size_);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, capture_rbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  cube_ = Mesh::CreateCube(1.0f);
  skybox_shader_     = CreateRef<Shader>("res/shaders/skybox_vert.glsl", "res/shaders/skybox_frag.glsl");
  irradiance_shader_ = CreateRef<Shader>("res/shaders/skybox_vert.glsl", "res/shaders/irradiance_frag.glsl");

  GenerateIrradiance();
}

Skybox::~Skybox() {
  glDeleteTextures(1, &env_cubemap_);
  glDeleteTextures(1, &irradiance_cubemap_);
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

void Skybox::GenerateIrradiance() {
  const glm::mat4 capture_proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

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

}  // namespace MEngine
