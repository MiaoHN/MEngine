#pragma once

#include "render/rhi/rhi.hpp"

namespace MEngine {

class VulkanRHI final : public IRHI {
 public:
  VulkanRHI();
  ~VulkanRHI() override;

  [[nodiscard]] GraphicsAPI GetAPI() const override { return GraphicsAPI::Vulkan; }

  void SetupWindowHints() const override;

  bool Initialize(GLFWwindow *window) override;

  void BeginFrame(const glm::vec4 &clear_color) const override;

  void EndFrame(GLFWwindow *window) const override;

  bool InitializeImGuiBackend(GLFWwindow *window) override;

  void ShutdownImGuiBackend() const override;

  void BeginImGuiFrame() const override;

  void RenderImGuiDrawData(ImDrawData *draw_data) const override;

  void DrawIndexedTriangles(int index_count) const override;
  void DrawIndexedInstanced(int index_count, int instance_count) const override;
  bool ReadBackBuffer(int width, int height, std::vector<unsigned char> &out_rgb) const override;

  void SetWireframe(bool wireframe) const override;
  void SetCullMode(CullMode mode) const override;

  unsigned int CreateFramebuffer() const override;

  void DestroyFramebuffer(unsigned int framebuffer) const override;

  void BindFramebuffer(unsigned int framebuffer) const override;

  void ClearBoundFramebufferColor(const glm::vec4 &clear_color) const override;

 private:
#if defined(MENGINE_HAS_VULKAN)
  bool CreateInstance();
  bool CreateSurface(GLFWwindow *window);
  bool PickPhysicalDevice();
  bool CreateLogicalDevice();
  bool CreateSwapchain(GLFWwindow *window);
  bool CreateImageViews();
  bool CreateRenderPass();
  bool CreateFramebuffers();
  bool CreateCommandPool();
  bool CreateCommandBuffers();
  bool CreateSyncObjects();
  bool CreateFramebufferUploadResources();
  bool CreateDescriptorPool();
  bool RecreateSwapchain();
  void DestroySwapchain();
  void DestroyFramebufferUploadResources();
  void DestroyDescriptorPool();

  void *instance_        = nullptr;
  void *surface_         = nullptr;
  void *physical_device_ = nullptr;
  void *device_          = nullptr;
  void *graphics_queue_  = nullptr;
  void *present_queue_   = nullptr;
  void *swapchain_       = nullptr;
  void *render_pass_     = nullptr;
  void *command_pool_    = nullptr;
  void *imgui_descriptor_pool_ = nullptr;
  void *framebuffer_upload_buffer_ = nullptr;
  void *framebuffer_upload_memory_  = nullptr;
  void *framebuffer_upload_mapped_  = nullptr;

  std::vector<void *> swapchain_images_;
  std::vector<void *> swapchain_image_views_;
  std::vector<void *> framebuffers_;
  std::vector<void *> command_buffers_;
  std::vector<void *> image_available_semaphores_;
  std::vector<void *> render_finished_semaphores_;
  std::vector<void *> in_flight_fences_;

  unsigned int swapchain_image_format_ = 0;
  int          swapchain_width_        = 0;
  int          swapchain_height_       = 0;

  glm::vec4 clear_color_ = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
  mutable std::vector<uint32_t> cpu_framebuffer_;
  uint32_t cpu_framebuffer_width_  = 0;
  uint32_t cpu_framebuffer_height_ = 0;

  uint32_t current_frame_       = 0;
  uint32_t current_image_index_ = 0;
  uint32_t graphics_queue_family_index_ = 0;
  uint32_t present_queue_family_index_   = 0;

  GLFWwindow *window_            = nullptr;
  bool        frame_begun_        = false;
  bool        imgui_initialized_  = false;
#endif
};

}  // namespace MEngine
