#pragma once

namespace MEngine {

/**
 * @brief A cube-map depth framebuffer used for omnidirectional (point light)
 * shadow mapping.
 *
 * OpenGL-specific for now; will be moved behind the RHI abstraction when the
 * Vulkan backend catches up.
 */
class CubeShadowMap {
 public:
  explicit CubeShadowMap(int size = 1024);
  ~CubeShadowMap();

  CubeShadowMap(const CubeShadowMap &)            = delete;
  CubeShadowMap &operator=(const CubeShadowMap &) = delete;

  /// Binds the framebuffer and sets the viewport (must be followed by
  /// BindFace per face, then Unbind).
  void Bind();

  /// Attaches the given cube face as the depth attachment and clears depth.
  void BindFace(int face);

  /// Restores the default framebuffer and the previously saved viewport.
  void Unbind();

  /// Binds the depth cubemap to the given texture unit.
  void BindTexture(unsigned int slot) const;

  [[nodiscard]] int GetSize() const { return size_; }

 private:
  unsigned int fbo_             = 0;
  unsigned int depth_cubemap_   = 0;
  int          size_            = 0;
  int          saved_viewport_[4]{};
};

}  // namespace MEngine
