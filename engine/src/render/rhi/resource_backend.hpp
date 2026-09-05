#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

namespace MEngine {

enum class VertexAttributeType { Float, Float2, Float3, Float4, Mat3, Mat4, Int, Int2, Int3, Int4, Bool };

struct VertexAttribute {
  VertexAttributeType type;
  std::string         name;

  VertexAttribute(VertexAttributeType type, const std::string &name) : type(type), name(name) {}
};

class ITextureBackend {
 public:
  virtual ~ITextureBackend() = default;

  virtual void SetData(unsigned char *data, int width, int height, int channels) = 0;
  virtual void Bind(unsigned int slot) const                                         = 0;
  virtual void Unbind() const                                                        = 0;
  virtual void SetSubTexture(int frame, int h_frames, int v_frames, int width, int height) = 0;
  virtual unsigned int GetID() const                                                 = 0;
};

class IShaderBackend {
 public:
  virtual ~IShaderBackend() = default;

  virtual bool IsValid() const = 0;

  virtual void Bind() const   = 0;
  virtual void Unbind() const = 0;

  virtual void SetUniformInt(const std::string &name, int value) const           = 0;
  virtual void SetUniformFloat(const std::string &name, float value) const       = 0;
  virtual void SetUniformVec2(const std::string &name, const glm::vec2 &value) const = 0;
  virtual void SetUniformVec3(const std::string &name, const glm::vec3 &value) const = 0;
  virtual void SetUniformVec4(const std::string &name, const glm::vec4 &value) const = 0;
  virtual void SetUniformMat4(const std::string &name, const glm::mat4 &value) const = 0;
};

class IFrameBufferBackend {
 public:
  virtual ~IFrameBufferBackend() = default;

  virtual void Bind() const          = 0;
  virtual void Unbind() const        = 0;
  virtual void AttachTexture()       = 0;
  virtual void AttachRenderBuffer()  = 0;
  virtual void CheckStatus() const   = 0;
  virtual void Clear() const         = 0;
  virtual void Resize(int width, int height) = 0;
  virtual unsigned int GetTextureId() const = 0;
  virtual unsigned int GetFrameBufferId() const = 0;
};

class IVertexArrayBackend {
 public:
  virtual ~IVertexArrayBackend() = default;

  virtual void SetVertexBuffer(const void *data, size_t size, const std::vector<VertexAttribute> &layouts) = 0;
  virtual void SetIndexBuffer(const unsigned int *data, int count)                                   = 0;
  virtual void Bind() const                                                                          = 0;
  virtual void Unbind() const                                                                        = 0;
  virtual int  GetCount() const                                                                      = 0;
};

std::unique_ptr<ITextureBackend> CreateTextureBackend();

std::unique_ptr<IShaderBackend> CreateShaderBackend(const std::string &vert_path, const std::string &frag_path);

std::unique_ptr<IFrameBufferBackend> CreateFrameBufferBackend(int width, int height);

std::unique_ptr<IVertexArrayBackend> CreateVertexArrayBackend();

}  // namespace MEngine
