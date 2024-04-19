#include "gl.hpp"

#include <glad/glad.h>

namespace MEngine {

namespace GL {

VertexBuffer::VertexBuffer() {
  glGenBuffers(1, &id_);
  glBindBuffer(GL_ARRAY_BUFFER, id_);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

VertexBuffer::~VertexBuffer() { glDeleteBuffers(1, &id_); }

void VertexBuffer::Bind() { glBindBuffer(GL_ARRAY_BUFFER, id_); }

void VertexBuffer::Unbind() { glBindBuffer(GL_ARRAY_BUFFER, 0); }

void VertexBuffer::SetData(const void* data, size_t size) {
  Bind();
  glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
  Unbind();
}

VertexArray::VertexArray() {
  glGenVertexArrays(1, &id_);
  glBindVertexArray(id_);
  glBindVertexArray(0);
}

VertexArray::~VertexArray() { glDeleteVertexArrays(1, &id_); }

void VertexArray::Bind() { glBindVertexArray(id_); }

void VertexArray::Unbind() { glBindVertexArray(0); }

void VertexArray::AddVertexBuffer(const VertexBuffer& vb) {
  vertex_buffers_.push_back(vb);
}

}  // namespace GL

}  // namespace MEngine
