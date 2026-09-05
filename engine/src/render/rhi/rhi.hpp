#pragma once

#include <vector>

#include "core/common.hpp"

// ImDrawData (from imgui.h) is only ever used behind a pointer in this
// interface; concrete backends include <imgui_impl_*.h> (which pull in imgui.h)
// themselves. A forward declaration keeps core/render independent of imgui.
struct ImDrawData;

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

  /// @brief Instanced indexed draw: `instance_count` copies of the bound mesh,
  /// per-instance data coming from the bound vertex array's instance buffer.
  virtual void DrawIndexedInstanced(int index_count, int instance_count) const = 0;

  /// @brief Toggles wireframe rasterization for subsequent draw calls.
  virtual void SetWireframe(bool wireframe) const = 0;

  /// @brief Reads the currently bound (default) framebuffer back to CPU as RGB.
  /// Returns false when the backend cannot read pixels (e.g. the Vulkan stub).
  virtual bool ReadBackBuffer(int width, int height, std::vector<unsigned char> &out_rgb) const = 0;

  virtual unsigned int CreateFramebuffer() const = 0;

  virtual void DestroyFramebuffer(unsigned int framebuffer) const = 0;

  virtual void BindFramebuffer(unsigned int framebuffer) const = 0;

  virtual void ClearBoundFramebufferColor(const glm::vec4 &clear_color) const = 0;
};

Ref<IRHI> CreateRHI(GraphicsAPI api);

void SetActiveRHI(const Ref<IRHI> &rhi);

IRHI *GetActiveRHI();

}  // namespace MEngine