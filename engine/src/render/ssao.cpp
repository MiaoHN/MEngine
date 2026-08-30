#include "render/ssao.hpp"

#include <random>
#include <string>

#include <glad/glad.h>

#include "render/asset_manager.hpp"
#include "render/shader.hpp"

namespace MEngine {

namespace {
float Lerp(float a, float b, float f) { return a + f * (b - a); }
}  // namespace

SSAO::SSAO(int width, int height) {
  if (width <= 0 || height <= 0) {
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    width  = viewport[2];
    height = viewport[3];
  }
  width_  = width;
  height_ = height;

  // G-buffer: view-space position (attachment 0) + normal (attachment 1).
  glGenFramebuffers(1, &g_buffer_fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, g_buffer_fbo_);

  glGenTextures(1, &g_position_);
  glBindTexture(GL_TEXTURE_2D, g_position_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_position_, 0);

  glGenTextures(1, &g_normal_);
  glBindTexture(GL_TEXTURE_2D, g_normal_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g_normal_, 0);

  glGenRenderbuffers(1, &g_depth_rbo_);
  glBindRenderbuffer(GL_RENDERBUFFER, g_depth_rbo_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width_, height_);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_depth_rbo_);

  const unsigned int attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  glDrawBuffers(2, attachments);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // SSAO framebuffer (single red channel).
  glGenFramebuffers(1, &ssao_fbo_);
  glGenTextures(1, &ssao_texture_);
  glBindTexture(GL_TEXTURE_2D, ssao_texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width_, height_, 0, GL_RED, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindFramebuffer(GL_FRAMEBUFFER, ssao_fbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssao_texture_, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Blur framebuffer.
  glGenFramebuffers(1, &blur_fbo_);
  glGenTextures(1, &blur_texture_);
  glBindTexture(GL_TEXTURE_2D, blur_texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width_, height_, 0, GL_RED, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindFramebuffer(GL_FRAMEBUFFER, blur_fbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blur_texture_, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // 4x4 random rotation vectors tiled across the screen.
  std::uniform_real_distribution<float> random_floats(0.0f, 1.0f);
  std::default_random_engine            generator;
  std::vector<glm::vec3>                ssao_noise;
  for (int i = 0; i < 16; ++i) {
    ssao_noise.push_back(glm::normalize(
        glm::vec3(random_floats(generator) * 2.0f - 1.0f, random_floats(generator) * 2.0f - 1.0f, 0.0f)));
  }
  glGenTextures(1, &noise_texture_);
  glBindTexture(GL_TEXTURE_2D, noise_texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, ssao_noise.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  // Tangent-space hemisphere sample kernel, biased toward the center.
  for (int i = 0; i < 64; ++i) {
    glm::vec3 sample(random_floats(generator) * 2.0f - 1.0f, random_floats(generator) * 2.0f - 1.0f,
                     random_floats(generator));
    sample = glm::normalize(sample);
    sample *= random_floats(generator);
    const float scale = Lerp(0.1f, 1.0f, std::pow(static_cast<float>(i) / 64.0f, 2.0f));
    sample *= scale;
    kernel_.push_back(sample);
  }

  glGenVertexArrays(1, &fullscreen_vao_);

  geometry_shader_ = AssetManager::Instance().GetShader("ssao_geometry");
  ssao_shader_     = AssetManager::Instance().GetShader("ssao");
  blur_shader_     = AssetManager::Instance().GetShader("ssao_blur");
}

SSAO::~SSAO() {
  glDeleteFramebuffers(1, &g_buffer_fbo_);
  glDeleteTextures(1, &g_position_);
  glDeleteTextures(1, &g_normal_);
  glDeleteRenderbuffers(1, &g_depth_rbo_);
  glDeleteTextures(1, &noise_texture_);
  glDeleteFramebuffers(1, &ssao_fbo_);
  glDeleteTextures(1, &ssao_texture_);
  glDeleteFramebuffers(1, &blur_fbo_);
  glDeleteTextures(1, &blur_texture_);
  glDeleteVertexArrays(1, &fullscreen_vao_);
}

void SSAO::DrawFullscreenTriangle() const {
  glBindVertexArray(fullscreen_vao_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
}

void SSAO::BeginGeometryPass(const glm::mat4 &proj, const glm::mat4 &view) const {
  glBindFramebuffer(GL_FRAMEBUFFER, g_buffer_fbo_);
  glViewport(0, 0, width_, height_);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  geometry_shader_->Bind();
  geometry_shader_->SetUniform("proj", proj);
  geometry_shader_->SetUniform("view", view);
}

void SSAO::SetGeometryModel(const glm::mat4 &model) const { geometry_shader_->SetUniform("model", model); }

void SSAO::EndGeometryPass() const {
  geometry_shader_->Unbind();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAO::Generate(const glm::mat4 &proj, const glm::mat4 &view) const {
  (void)view;

  // SSAO sampling pass.
  glBindFramebuffer(GL_FRAMEBUFFER, ssao_fbo_);
  glViewport(0, 0, width_, height_);
  glClear(GL_COLOR_BUFFER_BIT);

  ssao_shader_->Bind();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, g_position_);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, g_normal_);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, noise_texture_);
  ssao_shader_->SetUniform("g_position", 0);
  ssao_shader_->SetUniform("g_normal", 1);
  ssao_shader_->SetUniform("tex_noise", 2);
  for (int i = 0; i < 64; ++i) {
    ssao_shader_->SetUniform("samples[" + std::to_string(i) + "]", kernel_[static_cast<size_t>(i)]);
  }
  ssao_shader_->SetUniform("proj", proj);
  ssao_shader_->SetUniform("noise_scale", glm::vec2(static_cast<float>(width_) / 4.0f,
                                                    static_cast<float>(height_) / 4.0f));
  ssao_shader_->SetUniform("radius", radius_);
  ssao_shader_->SetUniform("bias", bias_);
  DrawFullscreenTriangle();

  // Blur pass.
  glBindFramebuffer(GL_FRAMEBUFFER, blur_fbo_);
  glClear(GL_COLOR_BUFFER_BIT);
  blur_shader_->Bind();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, ssao_texture_);
  blur_shader_->SetUniform("ssao_input", 0);
  DrawFullscreenTriangle();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAO::BindTexture(unsigned int slot) const {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, blur_texture_);
}

}  // namespace MEngine
