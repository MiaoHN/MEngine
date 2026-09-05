#include "render/rhi/vulkan_rhi.hpp"
#include "render/rhi/vulkan_resource_backend.hpp"

#include <imgui_impl_glfw.h>
#if defined(MENGINE_HAS_VULKAN)
#include <imgui_impl_vulkan.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <vector>

#include "core/logger.hpp"

#if defined(MENGINE_HAS_VULKAN)
#include <vulkan/vulkan.h>

extern "C" {
int glfwVulkanSupported(void);
const char **glfwGetRequiredInstanceExtensions(uint32_t *count);
VkResult glfwCreateWindowSurface(VkInstance instance, GLFWwindow *window, const VkAllocationCallbacks *allocator,
                                 VkSurfaceKHR *surface);
}
#endif

namespace MEngine {

VulkanRHI::VulkanRHI() = default;

VulkanRHI::~VulkanRHI() {
#if defined(MENGINE_HAS_VULKAN)
  if (device_) {
    vkDeviceWaitIdle(static_cast<VkDevice>(device_));

    ShutdownImGuiBackend();

    DestroySwapchain();
    DestroyDescriptorPool();

    for (size_t i = 0; i < image_available_semaphores_.size(); ++i) {
      vkDestroySemaphore(static_cast<VkDevice>(device_), static_cast<VkSemaphore>(image_available_semaphores_[i]), nullptr);
      vkDestroySemaphore(static_cast<VkDevice>(device_), static_cast<VkSemaphore>(render_finished_semaphores_[i]), nullptr);
      vkDestroyFence(static_cast<VkDevice>(device_), static_cast<VkFence>(in_flight_fences_[i]), nullptr);
    }

    if (command_pool_) {
      vkDestroyCommandPool(static_cast<VkDevice>(device_), static_cast<VkCommandPool>(command_pool_), nullptr);
    }

    vkDestroyDevice(static_cast<VkDevice>(device_), nullptr);
    device_ = nullptr;
  }

  if (surface_ && instance_) {
    vkDestroySurfaceKHR(static_cast<VkInstance>(instance_), static_cast<VkSurfaceKHR>(surface_), nullptr);
    surface_ = nullptr;
  }

  if (instance_) {
    vkDestroyInstance(static_cast<VkInstance>(instance_), nullptr);
    instance_ = nullptr;
  }
#endif
}

void VulkanRHI::SetupWindowHints() const { glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); }

bool VulkanRHI::Initialize(GLFWwindow *window) {
#if defined(MENGINE_HAS_VULKAN)
  window_ = window;
  if (!CreateInstance() || !CreateSurface(window) || !PickPhysicalDevice() || !CreateLogicalDevice() ||
      !CreateSwapchain(window) || !CreateImageViews() || !CreateRenderPass() || !CreateFramebuffers() ||
      !CreateCommandPool() || !CreateCommandBuffers() || !CreateSyncObjects() || !CreateFramebufferUploadResources()) {
    return false;
  }

  LOG_INFO("RHI") << "Vulkan backend initialized (swapchain path ready).";
  return true;
#else
  (void)window;
  LOG_ERROR("RHI") << "Vulkan backend was requested but Vulkan SDK was not found during CMake configure.";
  return false;
#endif
}

void VulkanRHI::BeginFrame(const glm::vec4 &clear_color) const {
#if defined(MENGINE_HAS_VULKAN)
  auto *self = const_cast<VulkanRHI *>(this);
  self->clear_color_ = clear_color;
  self->frame_begun_ = false;

  if (window_) {
    int width  = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    if (width == 0 || height == 0) {
      return;
    }

    if ((swapchain_width_ != width || swapchain_height_ != height) && !const_cast<VulkanRHI *>(this)->RecreateSwapchain()) {
      return;
    }
  }

  auto device = static_cast<VkDevice>(device_);
  VkFence in_flight_fence = static_cast<VkFence>(in_flight_fences_[current_frame_]);
  vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
  vkResetFences(device, 1, &in_flight_fence);

  auto acquire_result = vkAcquireNextImageKHR(
      device, static_cast<VkSwapchainKHR>(swapchain_), std::numeric_limits<uint64_t>::max(),
      static_cast<VkSemaphore>(image_available_semaphores_[current_frame_]), VK_NULL_HANDLE, &self->current_image_index_);

  if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
    const_cast<VulkanRHI *>(this)->RecreateSwapchain();
    return;
  }

  if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
    LOG_ERROR("RHI") << "Failed to acquire Vulkan swapchain image.";
    return;
  }

  if (cpu_framebuffer_width_ != static_cast<uint32_t>(swapchain_width_) ||
      cpu_framebuffer_height_ != static_cast<uint32_t>(swapchain_height_)) {
    if (!const_cast<VulkanRHI *>(this)->CreateFramebufferUploadResources()) {
      return;
    }
  }

  const uint32_t clear_r = static_cast<uint32_t>(std::clamp(clear_color_.r, 0.0f, 1.0f) * 255.0f);
  const uint32_t clear_g = static_cast<uint32_t>(std::clamp(clear_color_.g, 0.0f, 1.0f) * 255.0f);
  const uint32_t clear_b = static_cast<uint32_t>(std::clamp(clear_color_.b, 0.0f, 1.0f) * 255.0f);
  const uint32_t clear_a = static_cast<uint32_t>(std::clamp(clear_color_.a, 0.0f, 1.0f) * 255.0f);
  const uint32_t clear_pixel = (clear_a << 24) | (clear_b << 16) | (clear_g << 8) | clear_r;
  std::fill(cpu_framebuffer_.begin(), cpu_framebuffer_.end(), clear_pixel);

  self->frame_begun_ = true;
#else
  (void)clear_color;
#endif
}

void VulkanRHI::EndFrame(GLFWwindow *window) const {
#if defined(MENGINE_HAS_VULKAN)
  (void)window;
  if (!frame_begun_) {
    return;
  }

  auto *self = const_cast<VulkanRHI *>(this);
  auto  cmd  = static_cast<VkCommandBuffer>(command_buffers_[current_image_index_]);

  if (framebuffer_upload_mapped_ && !cpu_framebuffer_.empty()) {
    std::memcpy(framebuffer_upload_mapped_, cpu_framebuffer_.data(), cpu_framebuffer_.size() * sizeof(uint32_t));
  }

  vkResetCommandBuffer(cmd, 0);
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(cmd, &begin_info);

  VkImage swapchain_image = static_cast<VkImage>(swapchain_images_[current_image_index_]);

  VkImageMemoryBarrier to_transfer{};
  to_transfer.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  to_transfer.oldLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  to_transfer.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  to_transfer.image               = swapchain_image;
  to_transfer.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  to_transfer.subresourceRange.baseMipLevel   = 0;
  to_transfer.subresourceRange.levelCount     = 1;
  to_transfer.subresourceRange.baseArrayLayer  = 0;
  to_transfer.subresourceRange.layerCount      = 1;
  to_transfer.srcAccessMask = 0;
  to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                       1, &to_transfer);

  VkBufferImageCopy copy_region{};
  copy_region.bufferOffset                    = 0;
  copy_region.bufferRowLength                 = 0;
  copy_region.bufferImageHeight               = 0;
  copy_region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_region.imageSubresource.mipLevel       = 0;
  copy_region.imageSubresource.baseArrayLayer = 0;
  copy_region.imageSubresource.layerCount     = 1;
  copy_region.imageOffset                     = {0, 0, 0};
  copy_region.imageExtent                     = {static_cast<uint32_t>(swapchain_width_), static_cast<uint32_t>(swapchain_height_), 1};

  vkCmdCopyBufferToImage(cmd, static_cast<VkBuffer>(framebuffer_upload_buffer_), swapchain_image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

  VkImageMemoryBarrier to_present{};
  to_present.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  to_present.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  to_present.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  to_present.image               = swapchain_image;
  to_present.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  to_present.subresourceRange.baseMipLevel   = 0;
  to_present.subresourceRange.levelCount     = 1;
  to_present.subresourceRange.baseArrayLayer = 0;
  to_present.subresourceRange.layerCount     = 1;
  to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  to_present.dstAccessMask = 0;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr,
                       1, &to_present);

  vkEndCommandBuffer(cmd);

  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkSemaphore           wait_semaphore   = static_cast<VkSemaphore>(image_available_semaphores_[current_frame_]);
  VkSemaphore           signal_semaphore = static_cast<VkSemaphore>(render_finished_semaphores_[current_frame_]);

  VkSubmitInfo submit_info{};
  submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores    = &wait_semaphore;
  submit_info.pWaitDstStageMask  = &wait_stage;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers    = &cmd;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores    = &signal_semaphore;

  vkQueueSubmit(static_cast<VkQueue>(graphics_queue_), 1, &submit_info,
                static_cast<VkFence>(in_flight_fences_[current_frame_]));

  VkPresentInfoKHR present_info{};
  present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores    = &signal_semaphore;
  present_info.swapchainCount     = 1;
  auto swapchain_handle           = static_cast<VkSwapchainKHR>(swapchain_);
  present_info.pSwapchains        = &swapchain_handle;
  present_info.pImageIndices      = &current_image_index_;

  const auto present_result = vkQueuePresentKHR(static_cast<VkQueue>(present_queue_), &present_info);
  if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
    const_cast<VulkanRHI *>(this)->RecreateSwapchain();
  } else if (present_result != VK_SUCCESS) {
    LOG_ERROR("RHI") << "Failed to present Vulkan swapchain image.";
  }

  const uint32_t max_frames = static_cast<uint32_t>(image_available_semaphores_.size());
  self->current_frame_      = (self->current_frame_ + 1) % max_frames;
  self->frame_begun_        = false;
#else
  (void)window;
#endif
}

bool VulkanRHI::InitializeImGuiBackend(GLFWwindow *window) {
#if defined(MENGINE_HAS_VULKAN)
  if (imgui_initialized_) {
    return true;
  }

  if (!CreateDescriptorPool()) {
    return false;
  }

  if (!ImGui_ImplGlfw_InitForVulkan(window, true)) {
    DestroyDescriptorPool();
    return false;
  }

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance         = static_cast<VkInstance>(instance_);
  init_info.PhysicalDevice   = static_cast<VkPhysicalDevice>(physical_device_);
  init_info.Device           = static_cast<VkDevice>(device_);
  init_info.QueueFamily      = graphics_queue_family_index_;
  init_info.Queue            = static_cast<VkQueue>(graphics_queue_);
  init_info.DescriptorPool   = static_cast<VkDescriptorPool>(imgui_descriptor_pool_);
  init_info.MinImageCount    = 2;
  init_info.ImageCount       = static_cast<uint32_t>(swapchain_images_.size());
  init_info.MSAASamples      = VK_SAMPLE_COUNT_1_BIT;
  init_info.RenderPass       = static_cast<VkRenderPass>(render_pass_);
  init_info.Subpass          = 0;
  init_info.Allocator        = nullptr;
  init_info.CheckVkResultFn  = nullptr;
  init_info.MinAllocationSize = 1024 * 1024;

  if (!ImGui_ImplVulkan_Init(&init_info)) {
    ImGui_ImplGlfw_Shutdown();
    DestroyDescriptorPool();
    return false;
  }

  ImGui_ImplVulkan_CreateFontsTexture();
  imgui_initialized_ = true;
  return true;
#else
  (void)window;
  return false;
#endif
}

void VulkanRHI::ShutdownImGuiBackend() const {
#if defined(MENGINE_HAS_VULKAN)
  if (!imgui_initialized_) {
    return;
  }

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  const_cast<VulkanRHI *>(this)->DestroyDescriptorPool();
  const_cast<VulkanRHI *>(this)->imgui_initialized_ = false;
#endif
}

void VulkanRHI::BeginImGuiFrame() const {
#if defined(MENGINE_HAS_VULKAN)
  if (!imgui_initialized_) {
    return;
  }

  ImGui_ImplGlfw_NewFrame();
  ImGui_ImplVulkan_NewFrame();
#endif
}

void VulkanRHI::RenderImGuiDrawData(ImDrawData *draw_data) const {
#if defined(MENGINE_HAS_VULKAN)
  if (!imgui_initialized_ || !frame_begun_ || !draw_data) {
    return;
  }

  auto cmd = static_cast<VkCommandBuffer>(command_buffers_[current_image_index_]);
  ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
#else
  (void)draw_data;
#endif
}

void VulkanRHI::SetWireframe(bool wireframe) const {
  (void)wireframe;  // Wireframe rasterization is not implemented for the Vulkan path yet.
}

void VulkanRHI::DrawIndexedInstanced(int index_count, int instance_count) const {
  // CPU placeholder backend: instancing arrives with the real Vulkan draw path.
  (void)index_count;
  (void)instance_count;
}
void VulkanRHI::DrawIndexedTriangles(int index_count) const {
#if defined(MENGINE_HAS_VULKAN)
  if (!frame_begun_ || cpu_framebuffer_.empty()) {
    return;
  }

  auto *vao    = GetBoundVulkanVertexArrayBackend();
  auto *shader = GetBoundVulkanShaderBackend();
  auto *texture = GetBoundVulkanTextureBackend();
  if (!vao || !shader || !texture || !texture->HasPixels()) {
    return;
  }

  const auto &vertex_bytes = vao->GetVertexBytes();
  const auto &indices      = vao->GetIndices();
  const auto &layouts      = vao->GetLayouts();
  if (vertex_bytes.empty() || indices.empty() || layouts.empty()) {
    return;
  }

  const size_t stride = 5 * sizeof(float);

  struct VertexData {
    glm::vec4 clip;
    glm::vec2 uv;
  };

  auto read_vertex = [&](unsigned int index) -> VertexData {
    const unsigned char *base = vertex_bytes.data() + static_cast<size_t>(index) * stride;
    const float *floats = reinterpret_cast<const float *>(base);
    glm::vec3 position(floats[0], floats[1], floats[2]);
    glm::vec2 uv(floats[3], floats[4]);
    glm::vec4 clip = shader->GetProjectionViewMatrix() * shader->GetModelMatrix() * glm::vec4(position, 1.0f);
    return {clip, uv};
  };

  auto to_screen = [&](const glm::vec4 &clip) -> glm::vec2 {
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return {
        (ndc.x * 0.5f + 0.5f) * static_cast<float>(cpu_framebuffer_width_ - 1),
        (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(cpu_framebuffer_height_ - 1),
    };
  };

  auto edge = [](const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
  };

  auto sample_texture = [&](const glm::vec2 &uv) -> uint32_t {
    const int width = texture->GetWidth();
    const int height = texture->GetHeight();
    const int channels = texture->GetChannels();
    const auto &pixels = texture->GetPixels();
    if (width <= 0 || height <= 0 || pixels.empty()) {
      return 0xffffffffu;
    }

    const float u = std::clamp(uv.x, 0.0f, 1.0f);
    const float v = std::clamp(uv.y, 0.0f, 1.0f);
    const int x = std::clamp(static_cast<int>(u * static_cast<float>(width - 1)), 0, width - 1);
    const int y = std::clamp(static_cast<int>(v * static_cast<float>(height - 1)), 0, height - 1);
    const size_t offset = static_cast<size_t>(y) * static_cast<size_t>(width) * static_cast<size_t>(channels) +
                          static_cast<size_t>(x) * static_cast<size_t>(channels);

    unsigned char r = 255, g = 255, b = 255, a = 255;
    if (channels == 1) {
      r = g = b = pixels[offset];
    } else if (channels == 3) {
      r = pixels[offset + 0];
      g = pixels[offset + 1];
      b = pixels[offset + 2];
    } else if (channels >= 4) {
      r = pixels[offset + 0];
      g = pixels[offset + 1];
      b = pixels[offset + 2];
      a = pixels[offset + 3];
    }
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(r);
  };

  const int max_index = std::min(index_count, static_cast<int>(indices.size()));
  for (int i = 0; i + 2 < max_index; i += 3) {
    const auto v0 = read_vertex(indices[i + 0]);
    const auto v1 = read_vertex(indices[i + 1]);
    const auto v2 = read_vertex(indices[i + 2]);

    if (v0.clip.w == 0.0f || v1.clip.w == 0.0f || v2.clip.w == 0.0f) {
      continue;
    }

    const glm::vec2 p0 = to_screen(v0.clip);
    const glm::vec2 p1 = to_screen(v1.clip);
    const glm::vec2 p2 = to_screen(v2.clip);

    const float min_x = std::floor(std::min({p0.x, p1.x, p2.x}));
    const float max_x = std::ceil(std::max({p0.x, p1.x, p2.x}));
    const float min_y = std::floor(std::min({p0.y, p1.y, p2.y}));
    const float max_y = std::ceil(std::max({p0.y, p1.y, p2.y}));

    const int x0 = std::max(0, static_cast<int>(min_x));
    const int x1 = std::min(static_cast<int>(cpu_framebuffer_width_) - 1, static_cast<int>(max_x));
    const int y0 = std::max(0, static_cast<int>(min_y));
    const int y1 = std::min(static_cast<int>(cpu_framebuffer_height_) - 1, static_cast<int>(max_y));

    const float area = edge(p0, p1, p2);
    if (area == 0.0f) {
      continue;
    }

    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const glm::vec2 p(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
        const float w0 = edge(p1, p2, p) / area;
        const float w1 = edge(p2, p0, p) / area;
        const float w2 = edge(p0, p1, p) / area;
        if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
          continue;
        }

        const glm::vec2 uv = v0.uv * w0 + v1.uv * w1 + v2.uv * w2;
        const uint32_t src = sample_texture(uv);
        auto &framebuffer = const_cast<std::vector<uint32_t> &>(cpu_framebuffer_);
        uint32_t &dst = framebuffer[static_cast<size_t>(y) * cpu_framebuffer_width_ + static_cast<size_t>(x)];

        const uint32_t src_r = src & 0xffu;
        const uint32_t src_g = (src >> 8) & 0xffu;
        const uint32_t src_b = (src >> 16) & 0xffu;
        const uint32_t src_a = (src >> 24) & 0xffu;

        const uint32_t dst_r = dst & 0xffu;
        const uint32_t dst_g = (dst >> 8) & 0xffu;
        const uint32_t dst_b = (dst >> 16) & 0xffu;
        const uint32_t dst_a = (dst >> 24) & 0xffu;

        const float alpha = static_cast<float>(src_a) / 255.0f;
        const uint32_t out_r = static_cast<uint32_t>(src_r * alpha + dst_r * (1.0f - alpha));
        const uint32_t out_g = static_cast<uint32_t>(src_g * alpha + dst_g * (1.0f - alpha));
        const uint32_t out_b = static_cast<uint32_t>(src_b * alpha + dst_b * (1.0f - alpha));
        const uint32_t out_a = std::max(src_a, dst_a);
        dst = (out_a << 24) | (out_b << 16) | (out_g << 8) | out_r;
      }
    }
  }
#else
  (void)index_count;
#endif
}

bool VulkanRHI::ReadBackBuffer(int width, int height, std::vector<unsigned char> &out_rgb) const {
  // The Vulkan path is a CPU-side placeholder; real readback needs a transfer
  // queue and staging buffer.
  (void)width;
  (void)height;
  out_rgb.clear();
  return false;
}

unsigned int VulkanRHI::CreateFramebuffer() const { return 0; }

void VulkanRHI::DestroyFramebuffer(unsigned int framebuffer) const { (void)framebuffer; }

void VulkanRHI::BindFramebuffer(unsigned int framebuffer) const { (void)framebuffer; }

void VulkanRHI::ClearBoundFramebufferColor(const glm::vec4 &clear_color) const { (void)clear_color; }

#if defined(MENGINE_HAS_VULKAN)
namespace {

struct QueueFamilyIndices {
  std::optional<uint32_t> graphics_family;
  std::optional<uint32_t> present_family;
  [[nodiscard]] bool IsComplete() const { return graphics_family.has_value() && present_family.has_value(); }
};

QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
  QueueFamilyIndices indices;

  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
  std::vector<VkQueueFamilyProperties> properties(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, properties.data());

  for (uint32_t i = 0; i < queue_family_count; ++i) {
    if (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphics_family = i;
    }
    VkBool32 present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
    if (present_support == VK_TRUE) {
      indices.present_family = i;
    }
    if (indices.IsComplete()) {
      break;
    }
  }

  return indices;
}

uint32_t FindMemoryType(VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memory_properties{};
  vkGetPhysicalDeviceMemoryProperties(device, &memory_properties);

  for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
    if ((type_filter & (1u << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  return UINT32_MAX;
}

}  // namespace

bool VulkanRHI::CreateInstance() {
  if (!glfwVulkanSupported()) {
    LOG_ERROR("RHI") << "GLFW reports Vulkan is not supported on this machine.";
    return false;
  }

  uint32_t           extension_count = 0;
  const char *const *required_exts   = glfwGetRequiredInstanceExtensions(&extension_count);
  if (!required_exts || extension_count == 0) {
    LOG_ERROR("RHI") << "Failed to query required Vulkan extensions from GLFW.";
    return false;
  }

  VkApplicationInfo app_info{};
  app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName   = "MEngine";
  app_info.applicationVersion = VK_MAKE_API_VERSION(0, 0, 0, 1);
  app_info.pEngineName        = "MEngine";
  app_info.engineVersion      = VK_MAKE_API_VERSION(0, 0, 0, 1);
  app_info.apiVersion         = VK_API_VERSION_1_2;

  VkInstanceCreateInfo create_info{};
  create_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo        = &app_info;
  create_info.enabledExtensionCount   = extension_count;
  create_info.ppEnabledExtensionNames = required_exts;

  VkInstance instance = VK_NULL_HANDLE;
  if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
    LOG_ERROR("RHI") << "Failed to create Vulkan instance.";
    return false;
  }

  instance_ = instance;
  return true;
}

bool VulkanRHI::CreateSurface(GLFWwindow *window) {
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (glfwCreateWindowSurface(static_cast<VkInstance>(instance_), window, nullptr, &surface) != VK_SUCCESS) {
    LOG_ERROR("RHI") << "Failed to create Vulkan surface.";
    return false;
  }
  surface_ = surface;
  return true;
}

bool VulkanRHI::PickPhysicalDevice() {
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(static_cast<VkInstance>(instance_), &device_count, nullptr);
  if (device_count == 0) {
    LOG_ERROR("RHI") << "No Vulkan physical device found.";
    return false;
  }

  std::vector<VkPhysicalDevice> devices(device_count);
  vkEnumeratePhysicalDevices(static_cast<VkInstance>(instance_), &device_count, devices.data());

  for (auto device : devices) {
    auto indices = FindQueueFamilies(device, static_cast<VkSurfaceKHR>(surface_));
    if (indices.IsComplete()) {
      physical_device_ = device;
      return true;
    }
  }

  LOG_ERROR("RHI") << "No suitable Vulkan physical device found.";
  return false;
}

bool VulkanRHI::CreateLogicalDevice() {
  auto indices = FindQueueFamilies(static_cast<VkPhysicalDevice>(physical_device_), static_cast<VkSurfaceKHR>(surface_));
  if (!indices.IsComplete()) {
    return false;
  }

  graphics_queue_family_index_ = indices.graphics_family.value();
  present_queue_family_index_   = indices.present_family.value();

  std::vector<uint32_t> unique_indices = {indices.graphics_family.value()};
  if (indices.present_family.value() != indices.graphics_family.value()) {
    unique_indices.push_back(indices.present_family.value());
  }

  std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
  float                                queue_priority = 1.0f;
  for (auto queue_family : unique_indices) {
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = queue_family;
    queue_info.queueCount       = 1;
    queue_info.pQueuePriorities = &queue_priority;
    queue_create_infos.push_back(queue_info);
  }

  const std::array<const char *, 1> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  VkDeviceCreateInfo create_info{};
  create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.queueCreateInfoCount    = static_cast<uint32_t>(queue_create_infos.size());
  create_info.pQueueCreateInfos       = queue_create_infos.data();
  create_info.enabledExtensionCount   = static_cast<uint32_t>(device_extensions.size());
  create_info.ppEnabledExtensionNames = device_extensions.data();

  VkDevice device = VK_NULL_HANDLE;
  if (vkCreateDevice(static_cast<VkPhysicalDevice>(physical_device_), &create_info, nullptr, &device) != VK_SUCCESS) {
    LOG_ERROR("RHI") << "Failed to create Vulkan logical device.";
    return false;
  }

  device_ = device;

  VkQueue graphics_queue = VK_NULL_HANDLE;
  vkGetDeviceQueue(device, indices.graphics_family.value(), 0, &graphics_queue);
  graphics_queue_ = graphics_queue;

  VkQueue present_queue = VK_NULL_HANDLE;
  vkGetDeviceQueue(device, indices.present_family.value(), 0, &present_queue);
  present_queue_ = present_queue;

  return true;
}

bool VulkanRHI::CreateSwapchain(GLFWwindow *window) {
  VkSurfaceCapabilitiesKHR capabilities{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(static_cast<VkPhysicalDevice>(physical_device_),
                                            static_cast<VkSurfaceKHR>(surface_), &capabilities);

  uint32_t format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(static_cast<VkPhysicalDevice>(physical_device_), static_cast<VkSurfaceKHR>(surface_),
                                       &format_count, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(static_cast<VkPhysicalDevice>(physical_device_), static_cast<VkSurfaceKHR>(surface_),
                                       &format_count, formats.data());

  uint32_t present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(static_cast<VkPhysicalDevice>(physical_device_),
                                            static_cast<VkSurfaceKHR>(surface_), &present_mode_count, nullptr);
  std::vector<VkPresentModeKHR> present_modes(present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(static_cast<VkPhysicalDevice>(physical_device_),
                                            static_cast<VkSurfaceKHR>(surface_), &present_mode_count, present_modes.data());

  VkSurfaceFormatKHR surface_format = formats[0];
  for (const auto &format : formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surface_format = format;
      break;
    }
  }

  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
  for (auto mode : present_modes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      present_mode = mode;
      break;
    }
  }

  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(window, &width, &height);

  VkExtent2D extent{};
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    extent = capabilities.currentExtent;
  } else {
    extent.width  = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width,
                               capabilities.maxImageExtent.width);
    extent.height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);
  }

  uint32_t image_count = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
    image_count = capabilities.maxImageCount;
  }

  auto indices = FindQueueFamilies(static_cast<VkPhysicalDevice>(physical_device_), static_cast<VkSurfaceKHR>(surface_));
  std::array<uint32_t, 2> queue_family_indices = {indices.graphics_family.value(), indices.present_family.value()};

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface          = static_cast<VkSurfaceKHR>(surface_);
  create_info.minImageCount    = image_count;
  create_info.imageFormat      = surface_format.format;
  create_info.imageColorSpace  = surface_format.colorSpace;
  create_info.imageExtent      = extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  if (indices.graphics_family.value() != indices.present_family.value()) {
    create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices   = queue_family_indices.data();
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  create_info.preTransform   = capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode    = present_mode;
  create_info.clipped        = VK_TRUE;

  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  if (vkCreateSwapchainKHR(static_cast<VkDevice>(device_), &create_info, nullptr, &swapchain) != VK_SUCCESS) {
    LOG_ERROR("RHI") << "Failed to create Vulkan swapchain.";
    return false;
  }

  swapchain_              = swapchain;
  swapchain_image_format_ = static_cast<unsigned int>(surface_format.format);
  swapchain_width_        = static_cast<int>(extent.width);
  swapchain_height_       = static_cast<int>(extent.height);

  vkGetSwapchainImagesKHR(static_cast<VkDevice>(device_), swapchain, &image_count, nullptr);
  std::vector<VkImage> images(image_count);
  vkGetSwapchainImagesKHR(static_cast<VkDevice>(device_), swapchain, &image_count, images.data());

  swapchain_images_.clear();
  swapchain_images_.reserve(images.size());
  for (auto image : images) {
    swapchain_images_.push_back(image);
  }

  return true;
}

bool VulkanRHI::CreateImageViews() {
  swapchain_image_views_.clear();
  swapchain_image_views_.reserve(swapchain_images_.size());

  for (auto image_handle : swapchain_images_) {
    VkImageViewCreateInfo create_info{};
    create_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image                           = static_cast<VkImage>(image_handle);
    create_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format                          = static_cast<VkFormat>(swapchain_image_format_);
    create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel   = 0;
    create_info.subresourceRange.levelCount     = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount     = 1;

    VkImageView image_view = VK_NULL_HANDLE;
    if (vkCreateImageView(static_cast<VkDevice>(device_), &create_info, nullptr, &image_view) != VK_SUCCESS) {
      LOG_ERROR("RHI") << "Failed to create Vulkan image view.";
      return false;
    }
    swapchain_image_views_.push_back(image_view);
  }

  return true;
}

bool VulkanRHI::CreateRenderPass() {
  VkAttachmentDescription color_attachment{};
  color_attachment.format         = static_cast<VkFormat>(swapchain_image_format_);
  color_attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_ref{};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments    = &color_attachment_ref;

  VkSubpassDependency dependency{};
  dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass    = 0;
  dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments    = &color_attachment;
  render_pass_info.subpassCount    = 1;
  render_pass_info.pSubpasses      = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies   = &dependency;

  VkRenderPass render_pass = VK_NULL_HANDLE;
  if (vkCreateRenderPass(static_cast<VkDevice>(device_), &render_pass_info, nullptr, &render_pass) != VK_SUCCESS) {
    LOG_ERROR("RHI") << "Failed to create Vulkan render pass.";
    return false;
  }
  render_pass_ = render_pass;
  return true;
}

bool VulkanRHI::CreateFramebuffers() {
  framebuffers_.clear();
  framebuffers_.reserve(swapchain_image_views_.size());

  for (auto image_view_handle : swapchain_image_views_) {
    VkImageView attachments[] = {static_cast<VkImageView>(image_view_handle)};

    VkFramebufferCreateInfo framebuffer_info{};
    framebuffer_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass      = static_cast<VkRenderPass>(render_pass_);
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.pAttachments    = attachments;
    framebuffer_info.width           = static_cast<uint32_t>(swapchain_width_);
    framebuffer_info.height          = static_cast<uint32_t>(swapchain_height_);
    framebuffer_info.layers          = 1;

    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    if (vkCreateFramebuffer(static_cast<VkDevice>(device_), &framebuffer_info, nullptr, &framebuffer) != VK_SUCCESS) {
      LOG_ERROR("RHI") << "Failed to create Vulkan framebuffer.";
      return false;
    }
    framebuffers_.push_back(framebuffer);
  }

  return true;
}

bool VulkanRHI::CreateCommandPool() {
  auto indices = FindQueueFamilies(static_cast<VkPhysicalDevice>(physical_device_), static_cast<VkSurfaceKHR>(surface_));

  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = indices.graphics_family.value();

  VkCommandPool pool = VK_NULL_HANDLE;
  if (vkCreateCommandPool(static_cast<VkDevice>(device_), &pool_info, nullptr, &pool) != VK_SUCCESS) {
    LOG_ERROR("RHI") << "Failed to create Vulkan command pool.";
    return false;
  }

  command_pool_ = pool;
  return true;
}

bool VulkanRHI::CreateCommandBuffers() {
  if (!command_buffers_.empty()) {
    vkFreeCommandBuffers(static_cast<VkDevice>(device_), static_cast<VkCommandPool>(command_pool_),
                         static_cast<uint32_t>(command_buffers_.size()),
                         reinterpret_cast<VkCommandBuffer *>(command_buffers_.data()));
    command_buffers_.clear();
  }

  command_buffers_.resize(framebuffers_.size());

  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool        = static_cast<VkCommandPool>(command_pool_);
  alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = static_cast<uint32_t>(command_buffers_.size());

  if (vkAllocateCommandBuffers(static_cast<VkDevice>(device_), &alloc_info,
                               reinterpret_cast<VkCommandBuffer *>(command_buffers_.data())) != VK_SUCCESS) {
    LOG_ERROR("RHI") << "Failed to allocate Vulkan command buffers.";
    return false;
  }

  return true;
}

bool VulkanRHI::CreateSyncObjects() {
  constexpr uint32_t kMaxFramesInFlight = 2;
  image_available_semaphores_.resize(kMaxFramesInFlight);
  render_finished_semaphores_.resize(kMaxFramesInFlight);
  in_flight_fences_.resize(kMaxFramesInFlight);

  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkSemaphore render_finished = VK_NULL_HANDLE;
    VkFence     in_flight       = VK_NULL_HANDLE;

    if (vkCreateSemaphore(static_cast<VkDevice>(device_), &semaphore_info, nullptr, &image_available) != VK_SUCCESS ||
        vkCreateSemaphore(static_cast<VkDevice>(device_), &semaphore_info, nullptr, &render_finished) != VK_SUCCESS ||
        vkCreateFence(static_cast<VkDevice>(device_), &fence_info, nullptr, &in_flight) != VK_SUCCESS) {
      LOG_ERROR("RHI") << "Failed to create Vulkan synchronization primitives.";
      return false;
    }

    image_available_semaphores_[i] = image_available;
    render_finished_semaphores_[i] = render_finished;
    in_flight_fences_[i]           = in_flight;
  }

  return true;
}

void VulkanRHI::DestroySwapchain() {
  if (!device_) {
    return;
  }

  if (!command_buffers_.empty() && command_pool_) {
    vkFreeCommandBuffers(static_cast<VkDevice>(device_), static_cast<VkCommandPool>(command_pool_),
                         static_cast<uint32_t>(command_buffers_.size()),
                         reinterpret_cast<VkCommandBuffer *>(command_buffers_.data()));
    command_buffers_.clear();
  }

  for (auto framebuffer : framebuffers_) {
    vkDestroyFramebuffer(static_cast<VkDevice>(device_), static_cast<VkFramebuffer>(framebuffer), nullptr);
  }
  framebuffers_.clear();

  if (render_pass_) {
    vkDestroyRenderPass(static_cast<VkDevice>(device_), static_cast<VkRenderPass>(render_pass_), nullptr);
    render_pass_ = nullptr;
  }

  for (auto image_view : swapchain_image_views_) {
    vkDestroyImageView(static_cast<VkDevice>(device_), static_cast<VkImageView>(image_view), nullptr);
  }
  swapchain_image_views_.clear();
  swapchain_images_.clear();

  if (swapchain_) {
    vkDestroySwapchainKHR(static_cast<VkDevice>(device_), static_cast<VkSwapchainKHR>(swapchain_), nullptr);
    swapchain_ = nullptr;
  }
}

bool VulkanRHI::CreateFramebufferUploadResources() {
  if (!device_) {
    return false;
  }

  const uint32_t width  = static_cast<uint32_t>(std::max(1, swapchain_width_));
  const uint32_t height = static_cast<uint32_t>(std::max(1, swapchain_height_));
  const VkDeviceSize size = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * sizeof(uint32_t);

  DestroyFramebufferUploadResources();

  VkBufferCreateInfo buffer_info{};
  buffer_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size        = size;
  buffer_info.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VkBuffer buffer = VK_NULL_HANDLE;
  if (vkCreateBuffer(static_cast<VkDevice>(device_), &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
    LOG_ERROR("RHI") << "Failed to create Vulkan framebuffer upload buffer.";
    return false;
  }

  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(static_cast<VkDevice>(device_), buffer, &requirements);

  const uint32_t memory_type = FindMemoryType(static_cast<VkPhysicalDevice>(physical_device_), requirements.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (memory_type == UINT32_MAX) {
    LOG_ERROR("RHI") << "Failed to find host visible memory for framebuffer upload buffer.";
    vkDestroyBuffer(static_cast<VkDevice>(device_), buffer, nullptr);
    return false;
  }

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize  = requirements.size;
  alloc_info.memoryTypeIndex = memory_type;

  VkDeviceMemory memory = VK_NULL_HANDLE;
  if (vkAllocateMemory(static_cast<VkDevice>(device_), &alloc_info, nullptr, &memory) != VK_SUCCESS) {
    LOG_ERROR("RHI") << "Failed to allocate Vulkan framebuffer upload memory.";
    vkDestroyBuffer(static_cast<VkDevice>(device_), buffer, nullptr);
    return false;
  }

  if (vkBindBufferMemory(static_cast<VkDevice>(device_), buffer, memory, 0) != VK_SUCCESS) {
    LOG_ERROR("RHI") << "Failed to bind Vulkan framebuffer upload memory.";
    vkFreeMemory(static_cast<VkDevice>(device_), memory, nullptr);
    vkDestroyBuffer(static_cast<VkDevice>(device_), buffer, nullptr);
    return false;
  }

  void *mapped = nullptr;
  if (vkMapMemory(static_cast<VkDevice>(device_), memory, 0, size, 0, &mapped) != VK_SUCCESS) {
    LOG_ERROR("RHI") << "Failed to map Vulkan framebuffer upload memory.";
    vkFreeMemory(static_cast<VkDevice>(device_), memory, nullptr);
    vkDestroyBuffer(static_cast<VkDevice>(device_), buffer, nullptr);
    return false;
  }

  framebuffer_upload_buffer_ = buffer;
  framebuffer_upload_memory_ = memory;
  framebuffer_upload_mapped_ = mapped;
  cpu_framebuffer_width_ = width;
  cpu_framebuffer_height_ = height;
  cpu_framebuffer_.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
  return true;
}

void VulkanRHI::DestroyFramebufferUploadResources() {
  if (!device_) {
    framebuffer_upload_buffer_ = nullptr;
    framebuffer_upload_memory_ = nullptr;
    framebuffer_upload_mapped_ = nullptr;
    cpu_framebuffer_.clear();
    cpu_framebuffer_width_ = 0;
    cpu_framebuffer_height_ = 0;
    return;
  }

  if (framebuffer_upload_mapped_) {
    vkUnmapMemory(static_cast<VkDevice>(device_), static_cast<VkDeviceMemory>(framebuffer_upload_memory_));
    framebuffer_upload_mapped_ = nullptr;
  }

  if (framebuffer_upload_memory_) {
    vkFreeMemory(static_cast<VkDevice>(device_), static_cast<VkDeviceMemory>(framebuffer_upload_memory_), nullptr);
    framebuffer_upload_memory_ = nullptr;
  }

  if (framebuffer_upload_buffer_) {
    vkDestroyBuffer(static_cast<VkDevice>(device_), static_cast<VkBuffer>(framebuffer_upload_buffer_), nullptr);
    framebuffer_upload_buffer_ = nullptr;
  }

  cpu_framebuffer_.clear();
  cpu_framebuffer_width_ = 0;
  cpu_framebuffer_height_ = 0;
}

bool VulkanRHI::CreateDescriptorPool() {
  if (imgui_descriptor_pool_) {
    return true;
  }

  const std::array<VkDescriptorPoolSize, 11> pool_sizes = {{
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
  }};

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets       = 1000 * static_cast<uint32_t>(pool_sizes.size());
  pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
  pool_info.pPoolSizes    = pool_sizes.data();

  VkDescriptorPool pool = VK_NULL_HANDLE;
  if (vkCreateDescriptorPool(static_cast<VkDevice>(device_), &pool_info, nullptr, &pool) != VK_SUCCESS) {
    LOG_ERROR("RHI") << "Failed to create ImGui Vulkan descriptor pool.";
    return false;
  }

  imgui_descriptor_pool_ = pool;
  return true;
}

void VulkanRHI::DestroyDescriptorPool() {
  if (device_ && imgui_descriptor_pool_) {
    vkDestroyDescriptorPool(static_cast<VkDevice>(device_), static_cast<VkDescriptorPool>(imgui_descriptor_pool_), nullptr);
    imgui_descriptor_pool_ = nullptr;
  }
}

bool VulkanRHI::RecreateSwapchain() {
  if (!window_ || !device_) {
    return false;
  }

  int width  = 0;
  int height = 0;
  glfwGetFramebufferSize(window_, &width, &height);
  if (width == 0 || height == 0) {
    return false;
  }

  vkDeviceWaitIdle(static_cast<VkDevice>(device_));
  DestroyFramebufferUploadResources();
  DestroySwapchain();

  if (!CreateSwapchain(window_) || !CreateImageViews() || !CreateRenderPass() || !CreateFramebuffers() ||
      !CreateCommandBuffers() || !CreateFramebufferUploadResources()) {
    return false;
  }

  if (imgui_initialized_) {
    ImGui_ImplVulkan_SetMinImageCount(std::max<uint32_t>(2, static_cast<uint32_t>(swapchain_images_.size())));
  }

  return true;
}
#endif

}  // namespace MEngine
