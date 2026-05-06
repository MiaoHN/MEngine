#pragma once

#include <vector>

#include "render/rhi/resource_backend.hpp"

namespace MEngine {

class OpenGLTextureBackend final : public ITextureBackend {
 public:
  OpenGLTextureBackend();
  ~OpenGLTextureBackend() override;

  void SetData(unsigned char *data, int width, int height, int channels) override;
  void Bind(unsigned int slot) const override;
  void Unbind() const override;
  void SetSubTexture(int frame, int h_frames, int v_frames, int width, int height) override;
  unsigned int GetID() const override { return id_; }

 private:
  unsigned int id_ = 0;
};

class OpenGLShaderBackend final : public IShaderBackend {
 public:
  OpenGLShaderBackend(const std::string &vert_path, const std::string &frag_path);
  ~OpenGLShaderBackend() override;

  bool IsValid() const override { return valid_; }

  void Bind() const override;
  void Unbind() const override;

  void SetUniformInt(const std::string &name, int value) const override;
  void SetUniformFloat(const std::string &name, float value) const override;
  void SetUniformVec2(const std::string &name, const glm::vec2 &value) const override;
  void SetUniformVec3(const std::string &name, const glm::vec3 &value) const override;
  void SetUniformVec4(const std::string &name, const glm::vec4 &value) const override;
  void SetUniformMat4(const std::string &name, const glm::mat4 &value) const override;

 private:
  static std::vector<char> ReadFile(const std::string &path);
  static unsigned int Compile(unsigned int type, const char *src);

  unsigned int program_ = 0;
  bool         valid_   = false;
};

class OpenGLFrameBufferBackend final : public IFrameBufferBackend {
 public:
  OpenGLFrameBufferBackend(int width, int height);
  ~OpenGLFrameBufferBackend() override;

  void Bind() const override;
  void Unbind() const override;
  void AttachTexture() override;
  void AttachRenderBuffer() override;
  void CheckStatus() const override;
  void Clear() const override;
  void Resize(int width, int height) override;
  unsigned int GetTextureId() const override { return texture_id_; }

 private:
  unsigned int id_               = 0;
  unsigned int texture_id_       = 0;
  unsigned int render_buffer_id_ = 0;
  int          width_            = 0;
  int          height_           = 0;
};

class OpenGLVertexArrayBackend final : public IVertexArrayBackend {
 public:
  OpenGLVertexArrayBackend();
  ~OpenGLVertexArrayBackend() override;

  void SetVertexBuffer(const void *data, size_t size, const std::vector<VertexAttribute> &layouts) override;
  void SetIndexBuffer(const unsigned int *data, int count) override;
  void Bind() const override;
  void Unbind() const override;
  int  GetCount() const override { return count_; }

 private:
  unsigned int              id_     = 0;
  unsigned int              vbo_    = 0;
  unsigned int              ibo_    = 0;
  int                       count_  = 0;
  std::vector<VertexAttribute> layouts_;
};

}  // namespace MEngine
