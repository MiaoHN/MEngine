#include "render/post_processing.hpp"

#include <glad/glad.h>

#include "render/shader.hpp"

namespace MEngine {

PostProcessing::PostProcessing(int width, int height) {
  // When no explicit size is given, use the current (GLFW-provided) viewport,
  // which matches the window's framebuffer size.
  if (width <= 0 || height <= 0) {
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    width  = viewport[2];
    height = viewport[3];
  }
  width_  = width;
  height_ = height;
  bloom_width_  = width_ / 2;
  bloom_height_ = height_ / 2;

  // HDR scene framebuffer: RGBA16F color + depth/stencil renderbuffer.
  glGenFramebuffers(1, &scene_fbo_);
  glGenTextures(1, &scene_texture_);
  glBindTexture(GL_TEXTURE_2D, scene_texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenRenderbuffers(1, &scene_depth_);
  glBindRenderbuffer(GL_RENDERBUFFER, scene_depth_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width_, height_);

  glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scene_texture_, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, scene_depth_);

  CreateColorFramebuffer(bright_fbo_, bright_texture_, bloom_width_, bloom_height_);
  CreateColorFramebuffer(blur_fbo_[0], blur_texture_[0], bloom_width_, bloom_height_);
  CreateColorFramebuffer(blur_fbo_[1], blur_texture_[1], bloom_width_, bloom_height_);
  CreateColorFramebuffer(god_rays_fbo_, god_rays_texture_, bloom_width_, bloom_height_);

  // TAA history buffers (full resolution, ping-pong).
  glGenFramebuffers(1, &taa_fbo_);
  for (int i = 0; i < 2; ++i) {
    glGenTextures(1, &taa_textures_[i]);
    glBindTexture(GL_TEXTURE_2D, taa_textures_[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  glBindTexture(GL_TEXTURE_2D, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Dummy VAO for the fullscreen triangle (core profile requires a bound VAO).
  glGenVertexArrays(1, &fullscreen_vao_);

  brightness_shader_ = CreateRef<Shader>("res/shaders/post_vert.glsl", "res/shaders/brightness_frag.glsl");
  blur_shader_       = CreateRef<Shader>("res/shaders/post_vert.glsl", "res/shaders/blur_frag.glsl");
  composite_shader_  = CreateRef<Shader>("res/shaders/post_vert.glsl", "res/shaders/composite_frag.glsl");
  god_rays_shader_   = CreateRef<Shader>("res/shaders/post_vert.glsl", "res/shaders/god_rays_frag.glsl");
  taa_shader_        = CreateRef<Shader>("res/shaders/post_vert.glsl", "res/shaders/taa_frag.glsl");
}

PostProcessing::~PostProcessing() {
  glDeleteTextures(1, &scene_texture_);
  glDeleteRenderbuffers(1, &scene_depth_);
  glDeleteFramebuffers(1, &scene_fbo_);
  glDeleteTextures(1, &bright_texture_);
  glDeleteFramebuffers(1, &bright_fbo_);
  glDeleteTextures(2, blur_texture_);
  glDeleteFramebuffers(2, blur_fbo_);
  glDeleteTextures(1, &god_rays_texture_);
  glDeleteFramebuffers(1, &god_rays_fbo_);
  glDeleteTextures(2, taa_textures_);
  glDeleteFramebuffers(1, &taa_fbo_);
  glDeleteVertexArrays(1, &fullscreen_vao_);
}

void PostProcessing::CreateColorFramebuffer(unsigned int &fbo, unsigned int &texture, int width, int height) const {
  glGenFramebuffers(1, &fbo);
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessing::BeginScene() const {
  glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo_);
  glViewport(0, 0, width_, height_);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PostProcessing::EndScene() const { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

void PostProcessing::DrawFullscreenTriangle() const {
  glBindVertexArray(fullscreen_vao_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
}

unsigned int PostProcessing::GetSceneColorTexture() const {
  return taa_enabled_ ? taa_textures_[taa_current_] : scene_texture_;
}

glm::vec2 PostProcessing::GetJitter() const {
  // Halton sequence (bases 2 and 3) for low-discrepancy sub-pixel offsets.
  const auto halton = [](int index, int base) {
    float f = 1.0f, r = 0.0f;
    while (index > 0) {
      f /= static_cast<float>(base);
      r += f * static_cast<float>(index % base);
      index /= base;
    }
    return r;
  };

  const float hx = halton(taa_frame_, 2);
  const float hy = halton(taa_frame_, 3);
  ++taa_frame_;

  // Map [0,1) to a sub-pixel NDC offset.
  return glm::vec2((hx - 0.5f) * 2.0f / width_, (hy - 0.5f) * 2.0f / height_);
}

void PostProcessing::ResolveTAA() const {
  if (!taa_enabled_) {
    return;
  }

  const unsigned int dst = taa_textures_[1 - taa_current_];
  const unsigned int src = taa_textures_[taa_current_];

  glBindFramebuffer(GL_FRAMEBUFFER, taa_fbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst, 0);
  glViewport(0, 0, width_, height_);
  glClear(GL_COLOR_BUFFER_BIT);

  taa_shader_->Bind();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, scene_texture_);  // jittered current frame
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, src);  // previous resolved frame
  taa_shader_->SetUniform("current", 0);
  taa_shader_->SetUniform("history", 1);
  taa_shader_->SetUniform("texel_size", glm::vec2(1.0f / width_, 1.0f / height_));
  taa_shader_->SetUniform("blend", taa_first_frame_ ? 1.0f : taa_blend_);
  DrawFullscreenTriangle();

  taa_current_     = 1 - taa_current_;
  taa_first_frame_ = false;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessing::Render(const glm::vec2 &light_screen_pos) const {
  const unsigned int scene_color = GetSceneColorTexture();

  // 1. God rays: radial blur of the scene from the light source (half res).
  glBindFramebuffer(GL_FRAMEBUFFER, god_rays_fbo_);
  glViewport(0, 0, bloom_width_, bloom_height_);
  glClear(GL_COLOR_BUFFER_BIT);
  god_rays_shader_->Bind();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, scene_color);
  god_rays_shader_->SetUniform("scene", 0);
  god_rays_shader_->SetUniform("light_pos", light_screen_pos);
  DrawFullscreenTriangle();

  // 2. Brightness pass: scene -> bright (half res).
  glBindFramebuffer(GL_FRAMEBUFFER, bright_fbo_);
  glViewport(0, 0, bloom_width_, bloom_height_);
  glClear(GL_COLOR_BUFFER_BIT);
  brightness_shader_->Bind();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, scene_color);
  brightness_shader_->SetUniform("scene", 0);
  brightness_shader_->SetUniform("threshold", bloom_threshold_);
  DrawFullscreenTriangle();

  // 3. Gaussian blur ping-pong.
  unsigned int src = bright_texture_;
  for (int i = 0; i < blur_passes_; ++i) {
    const bool         horizontal = (i % 2) == 0;
    const unsigned int target_fbo = horizontal ? blur_fbo_[0] : blur_fbo_[1];

    glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
    glViewport(0, 0, bloom_width_, bloom_height_);
    blur_shader_->Bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src);
    blur_shader_->SetUniform("image", 0);
    blur_shader_->SetUniform("horizontal", horizontal ? 1 : 0);
    blur_shader_->SetUniform("texel_size", horizontal ? 1.0f / bloom_width_ : 1.0f / bloom_height_);
    DrawFullscreenTriangle();

    src = horizontal ? blur_texture_[0] : blur_texture_[1];
  }

  // 4. Composite to the default framebuffer.
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, width_, height_);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  composite_shader_->Bind();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, scene_color);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, src);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, god_rays_texture_);
  composite_shader_->SetUniform("scene", 0);
  composite_shader_->SetUniform("bloom", 1);
  composite_shader_->SetUniform("god_rays", 2);
  composite_shader_->SetUniform("exposure", exposure_);
  composite_shader_->SetUniform("bloom_strength", bloom_strength_);
  composite_shader_->SetUniform("god_rays_strength", god_rays_strength_);
  DrawFullscreenTriangle();
  composite_shader_->Unbind();
}

}  // namespace MEngine
