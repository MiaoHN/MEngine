#include "render/rhi/opengl_resource_backend.hpp"

#include <glad/glad.h>

#include <fstream>
#include <vector>

#ifdef ERROR
#undef ERROR
#endif

#include "core/logger.hpp"

namespace MEngine {

OpenGLTextureBackend::OpenGLTextureBackend() {
  glGenTextures(1, &id_);
  glBindTexture(GL_TEXTURE_2D, id_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

OpenGLTextureBackend::~OpenGLTextureBackend() { glDeleteTextures(1, &id_); }

void OpenGLTextureBackend::SetData(unsigned char *data, int width, int height, int channels) {
  GLenum format = GL_RGBA;
  if (channels == 1) {
    format = GL_RED;
  } else if (channels == 3) {
    format = GL_RGB;
  }

  glBindTexture(GL_TEXTURE_2D, id_);
  glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);
}

void OpenGLTextureBackend::Bind(unsigned int slot) const {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, id_);
}

void OpenGLTextureBackend::Unbind() const { glBindTexture(GL_TEXTURE_2D, 0); }

void OpenGLTextureBackend::SetSubTexture(int frame, int h_frames, int v_frames, int width, int height) {
  (void)frame;
  (void)h_frames;
  (void)v_frames;
  (void)width;
  (void)height;
  // TODO: move sprite UV animation to a dedicated vertex/uv abstraction.
}

std::vector<char> OpenGLShaderBackend::ReadFile(const std::string &path) {
  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    LOG_FATAL("Shader") << "Can't open file " << path;
    return {};
  }

  size_t size = static_cast<size_t>(file.tellg());
  std::vector<char> buffer(size + 1);
  file.seekg(0);
  file.read(buffer.data(), static_cast<std::streamsize>(size));
  buffer[size] = '\0';
  return buffer;
}

unsigned int OpenGLShaderBackend::Compile(unsigned int type, const char *src) {
  const unsigned int shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  int  success = 0;
  char infoLog[512];
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(shader, 512, nullptr, infoLog);
    LOG_FATAL("Shader") << "Shader compilation failed:\n" << infoLog;
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

OpenGLShaderBackend::OpenGLShaderBackend(const std::string &vert_path, const std::string &frag_path) {
  const auto vert_src = ReadFile(vert_path);
  const auto frag_src = ReadFile(frag_path);
  if (vert_src.empty() || frag_src.empty()) {
    return;
  }

  const unsigned int vert = Compile(GL_VERTEX_SHADER, vert_src.data());
  const unsigned int frag = Compile(GL_FRAGMENT_SHADER, frag_src.data());
  if (vert == 0 || frag == 0) {
    return;
  }

  program_ = glCreateProgram();
  glAttachShader(program_, vert);
  glAttachShader(program_, frag);
  glLinkProgram(program_);

  int  success = 0;
  char infoLog[512];
  glGetProgramiv(program_, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(program_, 512, nullptr, infoLog);
    LOG_FATAL("Shader") << "Program link failed:\n" << infoLog;
    glDeleteProgram(program_);
    program_ = 0;
  } else {
    valid_ = true;
  }

  glDeleteShader(vert);
  glDeleteShader(frag);
}

OpenGLShaderBackend::~OpenGLShaderBackend() {
  if (program_ != 0) {
    glDeleteProgram(program_);
  }
}

void OpenGLShaderBackend::Bind() const {
  if (program_ != 0) {
    glUseProgram(program_);
  }
}

void OpenGLShaderBackend::Unbind() const { glUseProgram(0); }

void OpenGLShaderBackend::SetUniformInt(const std::string &name, int value) const {
  Bind();
  glUniform1i(glGetUniformLocation(program_, name.c_str()), value);
}

void OpenGLShaderBackend::SetUniformFloat(const std::string &name, float value) const {
  Bind();
  glUniform1f(glGetUniformLocation(program_, name.c_str()), value);
}

void OpenGLShaderBackend::SetUniformVec2(const std::string &name, const glm::vec2 &value) const {
  Bind();
  glUniform2f(glGetUniformLocation(program_, name.c_str()), value.x, value.y);
}

void OpenGLShaderBackend::SetUniformVec3(const std::string &name, const glm::vec3 &value) const {
  Bind();
  glUniform3f(glGetUniformLocation(program_, name.c_str()), value.x, value.y, value.z);
}

void OpenGLShaderBackend::SetUniformVec4(const std::string &name, const glm::vec4 &value) const {
  Bind();
  glUniform4f(glGetUniformLocation(program_, name.c_str()), value.x, value.y, value.z, value.w);
}

void OpenGLShaderBackend::SetUniformMat4(const std::string &name, const glm::mat4 &value) const {
  Bind();
  glUniformMatrix4fv(glGetUniformLocation(program_, name.c_str()), 1, GL_FALSE, &value[0][0]);
}

OpenGLFrameBufferBackend::OpenGLFrameBufferBackend(int width, int height) : width_(width), height_(height) {
  glGenFramebuffers(1, &id_);
}

OpenGLFrameBufferBackend::~OpenGLFrameBufferBackend() {
  glDeleteTextures(1, &texture_id_);
  glDeleteRenderbuffers(1, &render_buffer_id_);
  glDeleteFramebuffers(1, &id_);
}

void OpenGLFrameBufferBackend::Bind() const { glBindFramebuffer(GL_FRAMEBUFFER, id_); }

void OpenGLFrameBufferBackend::Unbind() const { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

void OpenGLFrameBufferBackend::AttachTexture() {
  Bind();
  glDeleteTextures(1, &texture_id_);
  glGenTextures(1, &texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_, height_, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id_, 0);
}

void OpenGLFrameBufferBackend::AttachRenderBuffer() {
  Bind();
  glDeleteRenderbuffers(1, &render_buffer_id_);
  glGenRenderbuffers(1, &render_buffer_id_);
  glBindRenderbuffer(GL_RENDERBUFFER, render_buffer_id_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width_, height_);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, render_buffer_id_);
}

void OpenGLFrameBufferBackend::CheckStatus() const {
  Bind();
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    LOG_ERROR("FrameBuffer") << "Framebuffer is not complete!";
  }
}

void OpenGLFrameBufferBackend::Clear() const { glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }

void OpenGLFrameBufferBackend::Resize(int width, int height) {
  width_  = width;
  height_ = height;
  glViewport(0, 0, width, height);
  AttachTexture();
  AttachRenderBuffer();
}

OpenGLVertexArrayBackend::OpenGLVertexArrayBackend() { glGenVertexArrays(1, &id_); }

OpenGLVertexArrayBackend::~OpenGLVertexArrayBackend() {
  glDeleteBuffers(1, &vbo_);
  glDeleteBuffers(1, &ibo_);
  glDeleteVertexArrays(1, &id_);
}

void OpenGLVertexArrayBackend::SetVertexBuffer(const void *data, size_t size, const std::vector<VertexAttribute> &layouts) {
  layouts_ = layouts;
  Bind();
  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);

  unsigned int stride = 0;
  for (const auto &layout : layouts_) {
    switch (layout.type) {
      case VertexAttributeType::Float:
        stride += 1 * sizeof(float);
        break;
      case VertexAttributeType::Float2:
        stride += 2 * sizeof(float);
        break;
      case VertexAttributeType::Float3:
        stride += 3 * sizeof(float);
        break;
      case VertexAttributeType::Float4:
        stride += 4 * sizeof(float);
        break;
      default:
        break;
    }
  }

  size_t       offset = 0;
  unsigned int index  = 0;
  for (const auto &layout : layouts_) {
    glEnableVertexAttribArray(index);
    switch (layout.type) {
      case VertexAttributeType::Float:
        glVertexAttribPointer(index, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(offset));
        offset += sizeof(float);
        break;
      case VertexAttributeType::Float2:
        glVertexAttribPointer(index, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(offset));
        offset += 2 * sizeof(float);
        break;
      case VertexAttributeType::Float3:
        glVertexAttribPointer(index, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(offset));
        offset += 3 * sizeof(float);
        break;
      case VertexAttributeType::Float4:
        glVertexAttribPointer(index, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(offset));
        offset += 4 * sizeof(float);
        break;
      default:
        break;
    }
    ++index;
  }
}

void OpenGLVertexArrayBackend::SetIndexBuffer(const unsigned int *data, int count) {
  count_ = count;
  Bind();
  glGenBuffers(1, &ibo_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(count * sizeof(unsigned int)), data, GL_STATIC_DRAW);
}

void OpenGLVertexArrayBackend::Bind() const { glBindVertexArray(id_); }

void OpenGLVertexArrayBackend::Unbind() const { glBindVertexArray(0); }

}  // namespace MEngine
