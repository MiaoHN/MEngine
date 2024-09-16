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

#include "mengine.hpp"

using namespace MEngine;

class Editor : public Application {
 public:
  Editor();
  ~Editor();

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

  std::shared_ptr<ScriptEngine> script_engine_;

  std::shared_ptr<Camera2D> editor_camera_info_;

  std::filesystem::path m_BaseDirectory;
  std::filesystem::path m_CurrentDirectory;

  std::shared_ptr<Texture> m_DirectoryIcon;
  std::shared_ptr<Texture> m_FileIcon;

  ShaderLibrary  shader_library_;
  TextureLibrary texture_library_;
};

::MEngine::Application *CreateApplication();
