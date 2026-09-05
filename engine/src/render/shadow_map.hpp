#pragma once

namespace MEngine {

/**
 * @brief A depth-only framebuffer used for directional shadow mapping.
 *
 * OpenGL-specific for now; will be moved behind the RHI abstraction when the
 * Vulkan backend catches up.
 */
class ShadowMap {
 public:
  ShadowMap(int width = 2048, int height = 2048);
  ~ShadowMap();

  ShadowMap(const ShadowMap &)            = delete;
  ShadowMap &operator=(const ShadowMap &) = delete;

  /// Binds the framebuffer, sets the viewport and clears depth.
  void Bind();
  /// Restores the default framebuffer and the previously saved viewport.
  void Unbind();

  /// Binds the depth texture to the given texture unit.
  void BindTexture(unsigned int slot) const;

  [[nodiscard]] int GetWidth() const { return width_; }
  [[nodiscard]] int GetHeight() const { return height_; }

 private:
  unsigned int fbo_           = 0;
  unsigned int depth_texture_ = 0;
  int          width_         = 0;
  int          height_        = 0;
  int          saved_viewport_[4]{};
};

}  // namespace MEngine
