#include "render/frame_buffer.hpp"

#include <glad/glad.h>

namespace MEngine {

FrameBuffer::FrameBuffer() {
  logger_ = Logger::Get("FrameBuffer");
  // TODO: Make the width and height configurable
  width_ = 1600;
  height_ = 900;
  glGenFramebuffers(1, &id_);
}

FrameBuffer::~FrameBuffer() { glDeleteFramebuffers(1, &id_); }

void FrameBuffer::Bind() const { glBindFramebuffer(GL_FRAMEBUFFER, id_); }

void FrameBuffer::Unbind() const { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

void FrameBuffer::AttachTexture() {
  Bind();
  glGenTextures(1, &texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_, height_, 0, GL_RGB,
               GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         texture_id_, 0);
}

void FrameBuffer::AttachRenderBuffer() {
  Bind();
  glGenRenderbuffers(1, &render_buffer_id_);
  glBindRenderbuffer(GL_RENDERBUFFER, render_buffer_id_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width_, height_);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER, render_buffer_id_);
}

void FrameBuffer::CheckStatus() {
  Bind();
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    logger_->error("Framebuffer is not complete!");
  }
}

void FrameBuffer::Clear() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void FrameBuffer::Resize(int width, int height) {
  width_  = width;
  height_ = height;

  glDeleteTextures(1, &texture_id_);
  glDeleteRenderbuffers(1, &render_buffer_id_);
  glViewport(0, 0, width, height);
  AttachTexture();
  AttachRenderBuffer();
}

}  // namespace MEngine
