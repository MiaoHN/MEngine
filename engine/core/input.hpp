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

#include <GLFW/glfw3.h>

#include "core/application.hpp"

namespace MEngine {

class Input {
 public:
  static bool IsKeyPressed(int keycode) {
    GLFWwindow* window = Application::GetInstance()->GetWindow();
    int         state  = glfwGetKey(window, keycode);

    return state == GLFW_PRESS || state == GLFW_REPEAT;
  }
};

}  // namespace MEngine
