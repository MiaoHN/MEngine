#include "render/shadow_map.hpp"

#include <glad/glad.h>

namespace MEngine {

ShadowMap::ShadowMap(int width, int height) : width_(width), height_(height) {
  glGenTextures(1, &depth_texture_);
  glBindTexture(GL_TEXTURE_2D, depth_texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width_, height_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  const float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

  glGenFramebuffers(1, &fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

ShadowMap::~ShadowMap() {
  glDeleteTextures(1, &depth_texture_);
  glDeleteFramebuffers(1, &fbo_);
}

void ShadowMap::Bind() {
  glGetIntegerv(GL_VIEWPORT, saved_viewport_);
  glViewport(0, 0, width_, height_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMap::Unbind() {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(saved_viewport_[0], saved_viewport_[1], saved_viewport_[2], saved_viewport_[3]);
}

void ShadowMap::BindTexture(unsigned int slot) const {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, depth_texture_);
}

}  // namespace MEngine
