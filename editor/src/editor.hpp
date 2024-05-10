/**
 * @file editor.hpp
 * @author MiaoHN (582418227@qq.com)
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
#include "core/script_engine.hpp"
#include "core/task_handler.hpp"
#include "render/frame_buffer.hpp"
#include "scene/camera.hpp"
#include "scene/entity.hpp"
#include "scene/scene.hpp"

using namespace MEngine;

class Editor : public Application {
 public:
  Editor();
  ~Editor();

  void Initialize() override;

  void OnUpdate(float dt) override;

 private:
  int  viewport_width_   = 1280;
  int  viewport_height_  = 720;
  bool viewport_resized_ = false;

  std::shared_ptr<Scene> active_scene_;

  std::shared_ptr<FrameBuffer> frame_buffer_;

  std::shared_ptr<TaskHandler> renderer_;

  std::shared_ptr<ScriptEngine> script_engine_;

  std::shared_ptr<OrthographicCamera> camera_ = nullptr;

  ShaderLibrary  shader_library_;
  TextureLibrary texture_library_;
};

::MEngine::Application* CreateApplication();
