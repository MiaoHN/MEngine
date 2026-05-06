#include "render/rhi/rhi.hpp"

#include "core/logger.hpp"
#include "render/rhi/opengl_rhi.hpp"
#include "render/rhi/vulkan_rhi.hpp"

namespace MEngine {

static Ref<IRHI> s_active_rhi;

Ref<IRHI> CreateRHI(GraphicsAPI api) {
  switch (api) {
    case GraphicsAPI::OpenGL:
      return CreateRef<OpenGLRHI>();
    case GraphicsAPI::Vulkan:
#if defined(MENGINE_HAS_VULKAN)
      return CreateRef<VulkanRHI>();
#else
      LOG_WARN("RHI") << "Vulkan backend requested, but Vulkan support is not compiled in. Falling back to OpenGL.";
      return CreateRef<OpenGLRHI>();
#endif
    default:
      LOG_WARN("RHI") << "Unknown graphics API. Falling back to OpenGL.";
      return CreateRef<OpenGLRHI>();
  }
}

void SetActiveRHI(const Ref<IRHI> &rhi) { s_active_rhi = rhi; }

IRHI *GetActiveRHI() { return s_active_rhi.get(); }

}  // namespace MEngine