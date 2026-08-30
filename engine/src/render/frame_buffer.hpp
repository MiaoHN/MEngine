/**
 * @file frame_buffer.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-05-05
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <memory>

namespace MEngine {

class IFrameBufferBackend;

class FrameBuffer {
 public:
  FrameBuffer();
  ~FrameBuffer();
  void Bind() const;
  void Unbind() const;
  void AttachTexture();
  void AttachRenderBuffer();
  void CheckStatus();
  void Clear();
  void Resize(int width, int height);

  unsigned int GetTextureId() const;
  unsigned int GetFrameBufferId() const;

 private:
  std::unique_ptr<IFrameBufferBackend> backend_;

  int width_;
  int height_;
};

}  // namespace MEngine