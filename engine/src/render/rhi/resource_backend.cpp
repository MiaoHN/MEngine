#include "render/rhi/resource_backend.hpp"

#include "render/rhi/opengl_resource_backend.hpp"
#include "render/rhi/rhi.hpp"
#include "render/rhi/vulkan_resource_backend.hpp"

namespace MEngine {

std::unique_ptr<ITextureBackend> CreateTextureBackend() {
  if (const auto *rhi = GetActiveRHI(); rhi && rhi->GetAPI() == GraphicsAPI::Vulkan) {
    return std::make_unique<VulkanTextureBackend>();
  }
  return std::make_unique<OpenGLTextureBackend>();
}

std::unique_ptr<IShaderBackend> CreateShaderBackend(const std::string &vert_path, const std::string &frag_path) {
  if (const auto *rhi = GetActiveRHI(); rhi && rhi->GetAPI() == GraphicsAPI::Vulkan) {
    return std::make_unique<VulkanShaderBackend>(vert_path, frag_path);
  }
  return std::make_unique<OpenGLShaderBackend>(vert_path, frag_path);
}

std::unique_ptr<IFrameBufferBackend> CreateFrameBufferBackend(int width, int height) {
  if (const auto *rhi = GetActiveRHI(); rhi && rhi->GetAPI() == GraphicsAPI::Vulkan) {
    return std::make_unique<VulkanFrameBufferBackend>(width, height);
  }
  return std::make_unique<OpenGLFrameBufferBackend>(width, height);
}

std::unique_ptr<IVertexArrayBackend> CreateVertexArrayBackend() {
  if (const auto *rhi = GetActiveRHI(); rhi && rhi->GetAPI() == GraphicsAPI::Vulkan) {
    return std::make_unique<VulkanVertexArrayBackend>();
  }
  return std::make_unique<OpenGLVertexArrayBackend>();
}

}  // namespace MEngine
