/**
 * @file frame_buffer.hpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-05-05
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <memory>

#include "core/logger.hpp"

namespace MEngine {

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

  unsigned int GetTextureId() const { return texture_id_; }

 private:
  unsigned int id_;
  unsigned int texture_id_;
  unsigned int render_buffer_id_;

  int width_;
  int height_;

  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace MEngine