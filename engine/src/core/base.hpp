#pragma once

// Lean, dependency-free core primitives used across the engine.
// Kept intentionally small and self-contained: do NOT pull in UI/GLFW/imgui
// or heavy 3rd-party headers here.

#include <cstddef>
#include <memory>
#include <utility>

namespace MEngine {

/// @brief Shared owning handle used for engine resources.
template <typename T>
using Ref = std::shared_ptr<T>;

/// @brief Creates a shared owning handle.
template <typename T, typename... Args>
constexpr Ref<T> CreateRef(Args &&...args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

}  // namespace MEngine
