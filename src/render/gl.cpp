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
  size_t offset = 0;
  for (const auto& layout : layouts_) {
    glEnableVertexAttribArray(offset);
    switch (layout.type) {
      case ShaderDataType::Float:
        glVertexAttribPointer(offset, 1, GL_FLOAT, GL_FALSE, 0,
                              (const void*)offset);
        offset += 1;
        break;
      case ShaderDataType::Float2:
        glVertexAttribPointer(offset, 2, GL_FLOAT, GL_FALSE, 0,
                              (const void*)offset);
        offset += 2;
        break;
      case ShaderDataType::Float3:
        glVertexAttribPointer(offset, 3, GL_FLOAT, GL_FALSE, 0,
                              (const void*)offset);
        offset += 3;
        break;
      case ShaderDataType::Float4:
        glVertexAttribPointer(offset, 4, GL_FLOAT, GL_FALSE, 0,
                              (const void*)offset);
        offset += 4;
        break;
    }
  }
}

VertexArray::VertexArray() {
  glGenVertexArrays(1, &id_);
}

VertexArray::~VertexArray() { glDeleteVertexArrays(1, &id_); }

void VertexArray::Bind() {
  glBindVertexArray(id_);
  vertex_buffer_->Bind();
  vertex_buffer_->SetLayout();
}

void VertexArray::Unbind() { glBindVertexArray(0); }

void VertexArray::SetVertexBuffer(std::shared_ptr<VertexBuffer> vb) {
  vertex_buffer_ = vb;
}

}  // namespace GL

}  // namespace MEngine
