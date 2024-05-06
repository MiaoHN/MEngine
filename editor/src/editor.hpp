/**
 * @file editor.hpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-05-06
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include "core/application.hpp"
#include "core/entry_point.hpp"

using namespace MEngine;

class Editor : public Application {
 public:
  Editor();
  ~Editor();

  void Initialize() override;
};

::MEngine::Application* CreateApplication();
