#include "render/cube_shadow_map.hpp"

#include <glad/glad.h>

namespace MEngine {

CubeShadowMap::CubeShadowMap(int size) : size_(size) {
  glGenTextures(1, &depth_cubemap_);
  glBindTexture(GL_TEXTURE_CUBE_MAP, depth_cubemap_);
  for (unsigned int face = 0; face < 6; ++face) {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT, size_, size_, 0, GL_DEPTH_COMPONENT,
                 GL_FLOAT, nullptr);
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  glGenFramebuffers(1, &fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, depth_cubemap_, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

CubeShadowMap::~CubeShadowMap() {
  glDeleteTextures(1, &depth_cubemap_);
  glDeleteFramebuffers(1, &fbo_);
}

void CubeShadowMap::Bind() {
  glGetIntegerv(GL_VIEWPORT, saved_viewport_);
  glViewport(0, 0, size_, size_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
}

void CubeShadowMap::BindFace(int face) {
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, depth_cubemap_, 0);
  glClear(GL_DEPTH_BUFFER_BIT);
}

void CubeShadowMap::Unbind() {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(saved_viewport_[0], saved_viewport_[1], saved_viewport_[2], saved_viewport_[3]);
}

void CubeShadowMap::BindTexture(unsigned int slot) const {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_CUBE_MAP, depth_cubemap_);
}

}  // namespace MEngine
