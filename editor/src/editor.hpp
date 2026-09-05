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

#include <ImGuizmo.h>

#include "editor_camera.hpp"

using namespace MEngine;

class Editor : public Application {
 public:
  Editor();
  ~Editor() override;

  void Initialize() override;

  void OnUpdate(float dt) override;

  void BeginImGui();
  void EndImGui();

  void ShowImGuiContentBrowser();
  void ShowImGuiScene();
  void ShowImGuiViewport();
  void ShowImGuiProperties();
  void ShowImGuiLighting();
  void ShowImGuiRendering();
  void ShowImGuiLog();
  void ShowImGuiInformation();

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

  EditorCamera editor_camera_;

  std::filesystem::path base_directory_;
  std::filesystem::path current_directory_;

  std::shared_ptr<Texture> directory_icon_;
  std::shared_ptr<Texture> file_icon_;

  std::unordered_map<std::string, Ref<Texture>> thumbnail_cache_;

  Entity        grid_entity_;
  Ref<Mesh>     grid_mesh_;
  Ref<Material> grid_material_;
  Ref<Material> default_material_;

  std::vector<PointLight> point_lights_;

  ImGuizmo::OPERATION gizmo_operation_ = ImGuizmo::TRANSLATE;

  bool show_content_browser_ = true;
  bool show_scene_           = true;
  bool show_viewport_        = true;
  bool show_properties_      = true;
  bool show_lighting_        = true;
  bool show_rendering_       = true;
  bool show_log_             = true;
  bool show_information_     = true;
  bool show_colliders_       = true;

  ImGuiID dockspace_id_ = 0;

  ImFont *mono_font_ = nullptr;

  Entity CreateEntityWithUniqueName(const std::string &base_name);
  void CreatePrimitive(const std::string &name, const Ref<Mesh> &mesh);
  void CreateCameraEntity();
  void CreateModelEntity(const std::filesystem::path &path);
  void CreatePhysicsDemo();
  void DuplicateSelectedEntity();
  void ApplyDefaultLayout(ImGuiID dockspace_id);
  void ShowGizmo(const ImVec2 &image_pos, const ImVec2 &image_size);
  void DrawCameraGizmos(const ImVec2 &image_pos, const ImVec2 &image_size);
  void DrawColliderGizmos(const ImVec2 &image_pos, const ImVec2 &image_size);
  void SetGridVisible(bool visible);
};

::MEngine::Application *CreateApplication();
