#pragma once

#include "render/rhi/rhi.hpp"

namespace MEngine {

class OpenGLRHI final : public IRHI {
 public:
  [[nodiscard]] GraphicsAPI GetAPI() const override { return GraphicsAPI::OpenGL; }

  void SetupWindowHints() const override;

  bool Initialize(GLFWwindow *window) override;

  void BeginFrame(const glm::vec4 &clear_color) const override;

  void EndFrame(GLFWwindow *window) const override;

  bool InitializeImGuiBackend(GLFWwindow *window) override;

  void ShutdownImGuiBackend() const override;

  void BeginImGuiFrame() const override;

  void RenderImGuiDrawData(ImDrawData *draw_data) const override;

  void DrawIndexedTriangles(int index_count) const override;

  void SetWireframe(bool wireframe) const override;

  unsigned int CreateFramebuffer() const override;

  void DestroyFramebuffer(unsigned int framebuffer) const override;

  void BindFramebuffer(unsigned int framebuffer) const override;

  void ClearBoundFramebufferColor(const glm::vec4 &clear_color) const override;
};

}  // namespace MEngine