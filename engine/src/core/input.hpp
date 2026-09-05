/**
 * @file input.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-25
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include "core/application.hpp"
#include "core/common.hpp"

namespace MEngine {

class Input {
 public:
  static bool IsKeyPressed(int keycode) {
    GLFWwindow *window = Application::GetInstance()->GetWindow();
    int         state  = glfwGetKey(window, keycode);

    return state == GLFW_PRESS || state == GLFW_REPEAT;
  }

  static bool IsMouseButtonPressed(int button) {
    GLFWwindow *window = Application::GetInstance()->GetWindow();
    return glfwGetMouseButton(window, button) == GLFW_PRESS;
  }

  /// @brief Cursor delta in pixels since the previous call (first call = zero).
  static glm::vec2 GetMouseDelta() {
    static glm::vec2 last_pos{0.0f, 0.0f};
    static bool      first = true;

    GLFWwindow *window = Application::GetInstance()->GetWindow();
    double      x = 0.0;
    double      y = 0.0;
    glfwGetCursorPos(window, &x, &y);

    const glm::vec2 pos(static_cast<float>(x), static_cast<float>(y));
    const glm::vec2 delta = first ? glm::vec2(0.0f) : (pos - last_pos);
    last_pos              = pos;
    first                 = false;
    return delta;
  }
};

}  // namespace MEngine
