#pragma once

#include "core/application.hpp"
#include "core/script_engine.hpp"
#include "render/frame_buffer.hpp"
#include "scene/camera.hpp"
#include "scene/entity.hpp"
#include "scene/scene.hpp"

using namespace MEngine;

class RotatingCube : public Application {
 public:
  RotatingCube();
  ~RotatingCube();

  void Initialize() override;

  void OnUpdate(float dt) override;

  void BeginImGui();
  void EndImGui();

  void ShowImGuiScene();
  void ShowImGuiViewport();
  void ShowImGuiProperties();

  template <typename T>
  void DisplayAddComponentEntry(const std::string &entryName);

 private:
  enum class GameMode { Play, Edit };

  int  viewport_width_   = 1280;
  int  viewport_height_  = 720;
  bool viewport_resized_ = false;

  GameMode game_mode_ = GameMode::Edit;

  std::shared_ptr<Scene> active_scene_;

  std::shared_ptr<FrameBuffer> frame_buffer_;
};