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

#include "core/common.hpp"

namespace MEngine {

namespace GL {

enum class ShaderDataType { Float, Float2, Float3, Float4, Mat3, Mat4, Int, Int2, Int3, Int4, Bool };

struct ElementLayout {
  ShaderDataType type;
  std::string    name;

  ElementLayout(ShaderDataType type, const std::string &name) : type(type), name(name) {}
};

class VertexBuffer {
 public:
  VertexBuffer();
  ~VertexBuffer();
  void Bind() const;
  void Unbind() const;
  void SetData(const void *data, size_t size);

  void AddLayout(const std::vector<ElementLayout> &layouts);

  void SetLayout();

 private:
  unsigned int               id_;
  std::vector<ElementLayout> layouts_;
};

class IndexBuffer {
 public:
  IndexBuffer();
  ~IndexBuffer();
  void Bind() const;
  void Unbind() const;
  void SetData(const unsigned int *data, int count);
  int  GetCount() const { return count_; }

 private:
  unsigned int id_;
  int          count_;
};

class VertexArray {
 public:
  VertexArray();
  ~VertexArray();
  void Bind();
  void Unbind();
  void SetVertexBuffer(Ref<VertexBuffer> vb);
  void SetIndexBuffer(Ref<IndexBuffer> ib);

  int GetCount() const { return index_buffer_->GetCount(); }

 private:
  unsigned int id_;

  Ref<VertexBuffer> vertex_buffer_;
  Ref<IndexBuffer>  index_buffer_;
};

}  // namespace GL

}  // namespace MEngine
