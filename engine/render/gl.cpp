#include "render/gl.hpp"

#include <glad/glad.h>

namespace MEngine {

namespace GL {

VertexBuffer::VertexBuffer() { glGenBuffers(1, &id_); }

VertexBuffer::~VertexBuffer() { glDeleteBuffers(1, &id_); }

void VertexBuffer::Bind() const { glBindBuffer(GL_ARRAY_BUFFER, id_); }

void VertexBuffer::Unbind() const { glBindBuffer(GL_ARRAY_BUFFER, 0); }

void VertexBuffer::SetData(const void* data, size_t size) {
  Bind();
  glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
}

void VertexBuffer::AddLayout(const std::vector<ElementLayout>& layouts) {
  layouts_ = layouts;
}

void VertexBuffer::SetLayout() {
  Bind();
  unsigned int index  = 0;
  size_t       offset = 0;
  unsigned int stride = 0;
  for (const auto& layout : layouts_) {
    switch (layout.type) {
      case ShaderDataType::Float:
        stride += 1 * sizeof(float);
        break;
      case ShaderDataType::Float2:
        stride += 2 * sizeof(float);
        break;
      case ShaderDataType::Float3:
        stride += 3 * sizeof(float);
        break;
      case ShaderDataType::Float4:
        stride += 4 * sizeof(float);
        break;
      default:
        break;
    }
  }
  for (const auto& layout : layouts_) {
    glEnableVertexAttribArray(index);
    switch (layout.type) {
      case ShaderDataType::Float:
        glVertexAttribPointer(index, 1, GL_FLOAT, GL_FALSE, stride,
                              (const void*)offset);
        offset += sizeof(float);
        break;
      case ShaderDataType::Float2:
        glVertexAttribPointer(index, 2, GL_FLOAT, GL_FALSE, stride,
                              (const void*)offset);
        offset += 2 * sizeof(float);
        break;
      case ShaderDataType::Float3:
        glVertexAttribPointer(index, 3, GL_FLOAT, GL_FALSE, stride,
                              (const void*)offset);
        offset += 3 * sizeof(float);
        break;
      case ShaderDataType::Float4:
        glVertexAttribPointer(index, 4, GL_FLOAT, GL_FALSE, stride,
                              (const void*)offset);
        offset += 4 * sizeof(float);
        break;
      default:
        break;
    }
    index++;
  }
}

IndexBuffer::IndexBuffer() { glGenBuffers(1, &id_); }

IndexBuffer::~IndexBuffer() { glDeleteBuffers(1, &id_); }

void IndexBuffer::Bind() const { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id_); }

void IndexBuffer::Unbind() const { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); }

void IndexBuffer::SetData(const unsigned int* data, int count) {
  Bind();
  count_ = count;
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(unsigned int), data,
               GL_STATIC_DRAW);
}

VertexArray::VertexArray() { glGenVertexArrays(1, &id_); }

VertexArray::~VertexArray() { glDeleteVertexArrays(1, &id_); }

void VertexArray::Bind() { glBindVertexArray(id_); }

void VertexArray::Unbind() { glBindVertexArray(0); }

void VertexArray::SetVertexBuffer(std::shared_ptr<VertexBuffer> vb) {
  vertex_buffer_ = vb;
  Bind();
  vertex_buffer_->Bind();
  vertex_buffer_->SetLayout();
}

void VertexArray::SetIndexBuffer(std::shared_ptr<IndexBuffer> ib) {
  index_buffer_ = ib;
  Bind();
  index_buffer_->Bind();
}

}  // namespace GL

}  // namespace MEngine
