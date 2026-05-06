#include "render/rhi/vulkan_resource_backend.hpp"

#include "core/logger.hpp"

namespace MEngine {

namespace {
static unsigned int g_next_id = 1;
static VulkanTextureBackend *g_bound_texture = nullptr;
static VulkanShaderBackend *g_bound_shader = nullptr;
static VulkanVertexArrayBackend *g_bound_vertex_array = nullptr;
}

VulkanTextureBackend *GetBoundVulkanTextureBackend() { return g_bound_texture; }
VulkanShaderBackend *GetBoundVulkanShaderBackend() { return g_bound_shader; }
VulkanVertexArrayBackend *GetBoundVulkanVertexArrayBackend() { return g_bound_vertex_array; }

VulkanTextureBackend::VulkanTextureBackend() : id_(g_next_id++) {}

void VulkanTextureBackend::SetData(unsigned char *data, int width, int height, int channels) {
  width_    = width;
  height_   = height;
  channels_ = channels;
  if (data && width > 0 && height > 0 && channels > 0) {
    pixels_.assign(data, data + static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels));
  } else {
    pixels_.clear();
  }
}

void VulkanTextureBackend::Bind(unsigned int slot) const {
  (void)slot;
  g_bound_texture = const_cast<VulkanTextureBackend *>(this);
}

void VulkanTextureBackend::Unbind() const {
  if (g_bound_texture == this) {
    g_bound_texture = nullptr;
  }
}

void VulkanTextureBackend::SetSubTexture(int frame, int h_frames, int v_frames, int width, int height) {
  (void)frame;
  (void)h_frames;
  (void)v_frames;
  (void)width;
  (void)height;
}

VulkanShaderBackend::VulkanShaderBackend(const std::string &vert_path, const std::string &frag_path) {
  (void)vert_path;
  (void)frag_path;
  LOG_WARN("VulkanShader") << "Shader module/pipeline path is not implemented yet.";
}

void VulkanShaderBackend::Bind() const { g_bound_shader = const_cast<VulkanShaderBackend *>(this); }

void VulkanShaderBackend::Unbind() const {
  if (g_bound_shader == this) {
    g_bound_shader = nullptr;
  }
}

void VulkanShaderBackend::SetUniformInt(const std::string &name, int value) const {
  if (name == "texture1") {
    const_cast<VulkanShaderBackend *>(this)->texture_slot_ = value;
  }
}

void VulkanShaderBackend::SetUniformFloat(const std::string &name, float value) const {
  (void)name;
  (void)value;
}

void VulkanShaderBackend::SetUniformVec2(const std::string &name, const glm::vec2 &value) const {
  (void)name;
  (void)value;
}

void VulkanShaderBackend::SetUniformVec3(const std::string &name, const glm::vec3 &value) const {
  (void)name;
  (void)value;
}

void VulkanShaderBackend::SetUniformVec4(const std::string &name, const glm::vec4 &value) const {
  (void)name;
  (void)value;
}

void VulkanShaderBackend::SetUniformMat4(const std::string &name, const glm::mat4 &value) const {
  if (name == "model") {
    const_cast<VulkanShaderBackend *>(this)->model_matrix_ = value;
  } else if (name == "proj_view") {
    const_cast<VulkanShaderBackend *>(this)->projection_view_matrix_ = value;
  }
}

VulkanFrameBufferBackend::VulkanFrameBufferBackend(int width, int height) {
  (void)width;
  (void)height;
}

void VulkanFrameBufferBackend::Bind() const {}

void VulkanFrameBufferBackend::Unbind() const {}

void VulkanFrameBufferBackend::AttachTexture() {}

void VulkanFrameBufferBackend::AttachRenderBuffer() {}

void VulkanFrameBufferBackend::CheckStatus() const {}

void VulkanFrameBufferBackend::Clear() const {}

void VulkanFrameBufferBackend::Resize(int width, int height) {
  (void)width;
  (void)height;
}

VulkanVertexArrayBackend::VulkanVertexArrayBackend() = default;

void VulkanVertexArrayBackend::SetVertexBuffer(const void *data, size_t size, const std::vector<VertexAttribute> &layouts) {
  layouts_ = layouts;
  stride_  = 0;
  for (const auto &layout : layouts_) {
    switch (layout.type) {
      case VertexAttributeType::Float:
        stride_ += sizeof(float);
        break;
      case VertexAttributeType::Float2:
        stride_ += 2 * sizeof(float);
        break;
      case VertexAttributeType::Float3:
        stride_ += 3 * sizeof(float);
        break;
      case VertexAttributeType::Float4:
        stride_ += 4 * sizeof(float);
        break;
      case VertexAttributeType::Mat3:
        stride_ += 9 * sizeof(float);
        break;
      case VertexAttributeType::Mat4:
        stride_ += 16 * sizeof(float);
        break;
      case VertexAttributeType::Int:
        stride_ += sizeof(int);
        break;
      case VertexAttributeType::Int2:
        stride_ += 2 * sizeof(int);
        break;
      case VertexAttributeType::Int3:
        stride_ += 3 * sizeof(int);
        break;
      case VertexAttributeType::Int4:
        stride_ += 4 * sizeof(int);
        break;
      case VertexAttributeType::Bool:
        stride_ += sizeof(bool);
        break;
    }
  }
  vertex_bytes_.assign(static_cast<const unsigned char *>(data), static_cast<const unsigned char *>(data) + size);
}

void VulkanVertexArrayBackend::SetIndexBuffer(const unsigned int *data, int count) {
  if (data && count > 0) {
    indices_.assign(data, data + count);
  } else {
    indices_.clear();
  }
  count_ = count;
}

void VulkanVertexArrayBackend::Bind() const { g_bound_vertex_array = const_cast<VulkanVertexArrayBackend *>(this); }

void VulkanVertexArrayBackend::Unbind() const {
  if (g_bound_vertex_array == this) {
    g_bound_vertex_array = nullptr;
  }
}

}  // namespace MEngine
