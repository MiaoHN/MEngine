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
  void ShowImGuiScriptEditor();

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
  bool show_script_editor_   = true;

  ImGuiID dockspace_id_ = 0;

  ImFont *mono_font_ = nullptr;

  // --- Script editor state -------------------------------------------------
  static constexpr size_t kScriptBufferSize = 128 * 1024;

  std::string current_script_path_;  ///< open script, relative to the asset root
  bool        script_dirty_ = false; ///< buffer differs from the file on disk
  char        script_code_[kScriptBufferSize] = {};
  std::string script_pending_path_;  ///< file awaiting the unsaved-changes prompt
  bool        script_pending_create_ = false;  ///< pending file must be created first

  /// @brief Loads `relative` into the editor buffer (marks it clean).
  void LoadScriptIntoBuffer(const std::string &relative);

  /// @brief Writes the current buffer to disk and hot-reloads running scripts.
  /// Returns false when the write fails.
  bool SaveCurrentScript();

  /// @brief Opens `relative` in the Script Editor, showing an unsaved-changes
  /// prompt first if the current buffer is dirty.
  void OpenScriptInEditor(const std::string &relative);

  /// @brief Applies the pending open/create after the user resolves the prompt.
  void ApplyScriptPending();

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
  void LaunchStandalone();

  // --- Scene file management (File menu) ------------------------------------
  std::string current_scene_path_;  ///< absolute path of the open `.scene` file, empty for an unsaved new scene

  /// @brief Stops any running simulation and clears script / selection state so
  /// a scene-file operation can safely replace the content. Mirror of the
  /// Play-mode Stop handler.
  void ExitGameModeForFileOp();

  /// @brief Starts a brand-new, empty scene.
  void NewScene();

  /// @brief Shows the native "Open Scene" dialog and loads the chosen file.
  void OpenSceneDialog();

  /// @brief Loads `path` (stops Play first if running).
  void OpenScenePath(const std::string &path);

  /// @brief Saves to current_scene_path_, or shows "Save As" when none is set.
  void SaveCurrentScene();

  /// @brief Shows the native "Save Scene As" dialog and saves.
  void SaveSceneAsDialog();

  /// @brief Unattended verification of the File-menu scene ops: new / open /
  /// save round-trip. Driven by MENGINE_EDITOR_SELFTEST_SCENE=<path>; logs a
  /// PASS/FAIL summary and restores the default demo scene afterwards.
  void RunSceneFileSelftest(const std::string &path);
};

::MEngine::Application *CreateApplication();
