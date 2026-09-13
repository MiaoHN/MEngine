#include "render/frame_buffer.hpp"

#include "render/rhi/resource_backend.hpp"

namespace MEngine {

FrameBuffer::FrameBuffer() {
  // TODO: Make the width and height configurable
  width_  = 1600;
  height_ = 900;
  backend_ = CreateFrameBufferBackend(width_, height_);
}

FrameBuffer::~FrameBuffer() = default;

void FrameBuffer::Bind() const { backend_->Bind(); }

void FrameBuffer::Unbind() const { backend_->Unbind(); }

void FrameBuffer::AttachTexture() { backend_->AttachTexture(); }

void FrameBuffer::AttachRenderBuffer() { backend_->AttachRenderBuffer(); }

void FrameBuffer::CheckStatus() { backend_->CheckStatus(); }

void FrameBuffer::Clear() { backend_->Clear(); }

void FrameBuffer::Resize(int width, int height) {
  width_  = width;
  height_ = height;
  backend_->Resize(width, height);
}

unsigned int FrameBuffer::GetTextureId() const { return backend_->GetTextureId(); }

unsigned int FrameBuffer::GetFrameBufferId() const { return backend_->GetFrameBufferId(); }

}  // namespace MEngine
