/**
 * @file gl.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-19
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <vector>

namespace MEngine {

namespace GL {

class VertexBuffer {
 public:
  VertexBuffer();
  ~VertexBuffer();
  void Bind();
  void Unbind();
  void SetData(const void* data, size_t size);

 private:
  unsigned int id_;
};

class VertexArray {
 public:
  VertexArray();
  ~VertexArray();
  void Bind();
  void Unbind();
  void AddVertexBuffer(const VertexBuffer& vb);

 private:
  unsigned int id_;

  std::vector<VertexBuffer> vertex_buffers_;
};

}  // namespace GL

}  // namespace MEngine
