/**
 * @file sandbox.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief Sandbox application for testing MEngine features
 * @version 0.1
 * @date 2024-07-24
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include "mengine.hpp"

using namespace MEngine;

class Sandbox : public Application {
 public:
  Sandbox();
  ~Sandbox() override;

  void Initialize() override;
  void OnUpdate(float dt) override;

 private:
  void SetupImGui();
  void RenderImGui();
  void CreateTestEntities();

  Ref<Scene> scene_;
  Ref<Camera2D> camera_;
  Ref<FrameBuffer> frame_buffer_;

  // Test entities
  std::vector<Entity> test_entities_;
  
  // Camera control
  float camera_speed_ = 5.0f;
  float camera_zoom_speed_ = 1.0f;
  
  // Demo settings
  bool show_demo_window_ = false;
  bool animate_entities_ = true;
  float animation_time_ = 0.0f;
  
  // Viewport
  int viewport_width_ = 1280;
  int viewport_height_ = 720;
  bool viewport_resized_ = false;
};