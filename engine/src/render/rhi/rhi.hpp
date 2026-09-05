#pragma once

#include "core/common.hpp"

namespace MEngine {

enum class GraphicsAPI { OpenGL, Vulkan };

class IRHI {
 public:
  virtual ~IRHI() = default;

  [[nodiscard]] virtual GraphicsAPI GetAPI() const = 0;

  virtual void SetupWindowHints() const = 0;

  virtual bool Initialize(GLFWwindow *window) = 0;

  virtual void BeginFrame(const glm::vec4 &clear_color) const = 0;

  virtual void EndFrame(GLFWwindow *window) const = 0;

  virtual bool InitializeImGuiBackend(GLFWwindow *window) = 0;

  virtual void ShutdownImGuiBackend() const = 0;

  virtual void BeginImGuiFrame() const = 0;

  virtual void RenderImGuiDrawData(ImDrawData *draw_data) const = 0;

  virtual void DrawIndexedTriangles(int index_count) const = 0;

  virtual unsigned int CreateFramebuffer() const = 0;

  virtual void DestroyFramebuffer(unsigned int framebuffer) const = 0;

  virtual void BindFramebuffer(unsigned int framebuffer) const = 0;

  virtual void ClearBoundFramebufferColor(const glm::vec4 &clear_color) const = 0;
};

Ref<IRHI> CreateRHI(GraphicsAPI api);

void SetActiveRHI(const Ref<IRHI> &rhi);

IRHI *GetActiveRHI();

}  // namespace MEngine