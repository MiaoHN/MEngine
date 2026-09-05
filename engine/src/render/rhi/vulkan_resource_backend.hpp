#pragma once

#include "render/rhi/resource_backend.hpp"

namespace MEngine {

class VulkanTextureBackend final : public ITextureBackend {
 public:
  VulkanTextureBackend();
  ~VulkanTextureBackend() override = default;

  void SetData(unsigned char *data, int width, int height, int channels) override;
  void Bind(unsigned int slot) const override;
  void Unbind() const override;
  void SetSubTexture(int frame, int h_frames, int v_frames, int width, int height) override;
  unsigned int GetID() const override { return id_; }

  [[nodiscard]] bool HasPixels() const { return !pixels_.empty(); }
  [[nodiscard]] int GetWidth() const { return width_; }
  [[nodiscard]] int GetHeight() const { return height_; }
  [[nodiscard]] int GetChannels() const { return channels_; }
  [[nodiscard]] const std::vector<unsigned char> &GetPixels() const { return pixels_; }

 private:
  unsigned int id_ = 0;
  int          width_ = 0;
  int          height_ = 0;
  int          channels_ = 0;
  std::vector<unsigned char> pixels_;
};

class VulkanShaderBackend final : public IShaderBackend {
 public:
  VulkanShaderBackend(const std::string &vert_path, const std::string &frag_path);
  ~VulkanShaderBackend() override = default;

  bool IsValid() const override { return false; }

  void Bind() const override;
  void Unbind() const override;

  void SetUniformInt(const std::string &name, int value) const override;
  void SetUniformFloat(const std::string &name, float value) const override;
  void SetUniformVec2(const std::string &name, const glm::vec2 &value) const override;
  void SetUniformVec3(const std::string &name, const glm::vec3 &value) const override;
  void SetUniformVec4(const std::string &name, const glm::vec4 &value) const override;
  void SetUniformMat4(const std::string &name, const glm::mat4 &value) const override;

  [[nodiscard]] const glm::mat4 &GetModelMatrix() const { return model_matrix_; }
  [[nodiscard]] const glm::mat4 &GetProjectionViewMatrix() const { return projection_view_matrix_; }
  [[nodiscard]] int GetTextureSlot() const { return texture_slot_; }

 private:
  glm::mat4 model_matrix_           = glm::mat4(1.0f);
  glm::mat4 projection_view_matrix_  = glm::mat4(1.0f);
  int       texture_slot_           = 0;
};

class VulkanFrameBufferBackend final : public IFrameBufferBackend {
 public:
  VulkanFrameBufferBackend(int width, int height);
  ~VulkanFrameBufferBackend() override = default;

  void Bind() const override;
  void Unbind() const override;
  void AttachTexture() override;
  void AttachRenderBuffer() override;
  void CheckStatus() const override;
  void Clear() const override;
  void Resize(int width, int height) override;
  unsigned int GetTextureId() const override { return 0; }
  unsigned int GetFrameBufferId() const override { return 0; }
};

class VulkanVertexArrayBackend final : public IVertexArrayBackend {
 public:
  VulkanVertexArrayBackend();
  ~VulkanVertexArrayBackend() override = default;

  void SetVertexBuffer(const void *data, size_t size, const std::vector<VertexAttribute> &layouts) override;
  void SetIndexBuffer(const unsigned int *data, int count) override;
  void SetInstanceData(const glm::mat4 *data, int count) override;
  void Bind() const override;
  void Unbind() const override;
  int  GetCount() const override { return count_; }

  [[nodiscard]] const std::vector<unsigned char> &GetVertexBytes() const { return vertex_bytes_; }
  [[nodiscard]] const std::vector<unsigned int> &GetIndices() const { return indices_; }
  [[nodiscard]] const std::vector<VertexAttribute> &GetLayouts() const { return layouts_; }
  [[nodiscard]] size_t GetStride() const { return stride_; }

 private:
  int count_ = 0;
  size_t stride_ = 0;
  std::vector<unsigned char> vertex_bytes_;
  std::vector<unsigned int> indices_;
  std::vector<VertexAttribute> layouts_;
};

VulkanTextureBackend *GetBoundVulkanTextureBackend();
VulkanShaderBackend *GetBoundVulkanShaderBackend();
VulkanVertexArrayBackend *GetBoundVulkanVertexArrayBackend();

}  // namespace MEngine
