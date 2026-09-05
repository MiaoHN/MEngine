#include <imgui.h>
#include <imgui_internal.h>

#include "editor.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>

#include "render/asset_manager.hpp"
#include "render/model_loader.hpp"
#include "utils/profiler.h"

// Native scene open/save dialogs. Windows only; other platforms currently fall
// back to a no-op (returning false == "cancelled"). NOMINMAX keeps windows.h
// from #defining min/max and breaking std algorithms below.
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#ifdef ERROR
#undef ERROR
#endif
#endif

namespace {

#if defined(_WIN32)
/// @brief Shows the native "Open" dialog; returns true and fills `out_path`
/// when the user picks a file.
bool NativeOpenFileDialog(std::string &out_path) {
  char file_buffer[MAX_PATH] = {};
  OPENFILENAMEA ofn{};
  ofn.lStructSize     = sizeof(ofn);
  ofn.lpstrFilter     = "MEngine Scene (*.scene)\0*.scene\0All Files (*.*)\0*.*\0\0";
  ofn.lpstrFile       = file_buffer;
  ofn.nMaxFile        = static_cast<DWORD>(sizeof(file_buffer));
  ofn.lpstrTitle      = "Open Scene";
  ofn.lpstrInitialDir = nullptr;
  ofn.Flags           = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
  if (GetOpenFileNameA(&ofn)) {
    out_path = file_buffer;
    return true;
  }
  return false;
}

/// @brief Shows the native "Save As" dialog; returns true and fills `out_path`
/// when the user confirms a file name.
bool NativeSaveFileDialog(std::string &out_path) {
  char file_buffer[MAX_PATH] = {};
  OPENFILENAMEA ofn{};
  ofn.lStructSize     = sizeof(ofn);
  ofn.lpstrFilter     = "MEngine Scene (*.scene)\0*.scene\0All Files (*.*)\0*.*\0\0";
  ofn.lpstrFile       = file_buffer;
  ofn.nMaxFile        = static_cast<DWORD>(sizeof(file_buffer));
  ofn.lpstrTitle      = "Save Scene As";
  ofn.lpstrInitialDir = nullptr;
  ofn.lpstrDefExt     = "scene";
  ofn.Flags           = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
  if (GetSaveFileNameA(&ofn)) {
    out_path = file_buffer;
    return true;
  }
  return false;
}
#else
bool NativeOpenFileDialog(std::string &) { return false; }
bool NativeSaveFileDialog(std::string &) { return false; }
#endif

/// @brief Portable environment-variable read. Uses _dupenv_s on Windows where
/// std::getenv is marked deprecated under clang-cl.
std::string GetEnvVar(const char *name) {
#if defined(_WIN32)
  char  *buffer = nullptr;
  size_t length = 0;
  if (_dupenv_s(&buffer, &length, name) == 0 && buffer) {
    std::string value = buffer;
    free(buffer);
    return value;
  }
  return std::string();
#else
  const char *value = std::getenv(name);
  return value ? std::string(value) : std::string();
#endif
}

Ref<Material> CreateDefaultMaterial() {
  auto material = CreateRef<Material>();
  material->SetShader(AssetManager::Instance().GetShader("pbr"));
  material->SetBaseColorFactor(glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
  material->SetMetallicFactor(0.0f);
  material->SetRoughnessFactor(0.8f);
  material->SetSpecularFactor(0.5f);
  return material;
}

bool IsImageFile(const std::filesystem::path &path) {
  const std::string ext = path.extension().string();
  for (const char *e : {".png", ".jpg", ".jpeg", ".bmp", ".tga"}) {
    if (ext == e) return true;
  }
  return false;
}

/// @brief Auto-assigns common OBJ texture names found next to the model file.
///
/// The classic "backpack"-style assets ship `diffuse.jpg`, `normal.png`,
/// `roughness.jpg` and `ao.jpg`; `specular.jpg` belongs to the legacy
/// specular-glossiness workflow and is intentionally ignored by our
/// metallic-roughness PBR shader.
void AutoAssignObjTextures(const Ref<Material> &material, const std::filesystem::path &obj_path) {
  const auto dir = obj_path.parent_path();

  const auto find_texture = [&](std::initializer_list<const char *> names) -> Ref<Texture> {
    for (const char *name : names) {
      const std::filesystem::path candidate = dir / name;
      if (std::filesystem::exists(candidate)) {
        return Texture::Create(candidate.string());
      }
    }
    return nullptr;
  };

  if (auto t = find_texture({"diffuse.jpg", "diffuse.png", "albedo.jpg", "albedo.png"})) {
    material->SetAlbedoMap(t);
  }
  if (auto t = find_texture({"normal.png", "normal.jpg"})) {
    material->SetNormalMap(t);
  }
  if (auto t = find_texture({"roughness.jpg", "roughness.png"})) {
    material->SetMetallicRoughnessMap(t);
    material->SetRoughnessFactor(1.0f);  // let the roughness map control it
  }
  if (auto t = find_texture({"ao.jpg", "ao.png", "occlusion.jpg", "occlusion.png"})) {
    material->SetAOMap(t);
  }
}

/// @brief Loads a model asset into `mesh`/`material`. OBJ assets get their
/// sibling textures auto-assigned; glTF assets use their own PBR material.
/// Returns false if the file could not be loaded.
bool LoadModelAsset(const std::filesystem::path &path, Ref<Mesh> &mesh, Ref<Material> &material) {
  const std::string ext = path.extension().string();

  // Default to a matte, white, non-metallic material so textured assets
  // (multiplied by white) and bare meshes alike read correctly.
  material = CreateRef<Material>();
  material->SetShader(AssetManager::Instance().GetShader("pbr"));
  material->SetBaseColorFactor(glm::vec4(1.0f));
  material->SetMetallicFactor(0.0f);
  material->SetRoughnessFactor(1.0f);
  material->SetSpecularFactor(0.3f);  // fabric: keep reflections low

  if (ext == ".obj") {
    mesh = ModelLoader::LoadObj(path.string());
    if (mesh) {
      AutoAssignObjTextures(material, path);
    }
  } else if (ext == ".gltf" || ext == ".glb") {
    mesh = ModelLoader::LoadGltf(path.string());
    if (auto mat = ModelLoader::LoadGltfMaterial(path.string())) {
      mat->SetShader(AssetManager::Instance().GetShader("pbr"));
      material = mat;
    }
  }

  if (mesh) {
    mesh->SetSource(path.string());
  }

  return mesh != nullptr;
}

/// @brief Converts an absolute path to one relative to the asset root (falls
/// back to the original string when the path lies outside the asset root).
std::string ToAssetRelativePath(const std::filesystem::path &abs_path) {
  const std::filesystem::path root = std::filesystem::absolute(AssetManager::Instance().GetAssetRoot());
  std::error_code             ec;
  const std::filesystem::path rel = std::filesystem::relative(abs_path, root, ec);
  return ec ? abs_path.string() : rel.generic_string();
}

/// @brief Relative paths (e.g. "scripts/spin.lua") of every .lua file under
/// the asset root's `scripts/` directory.
std::vector<std::string> ListLuaScriptPaths() {
  std::vector<std::string> out;
  const std::filesystem::path scripts_dir =
      std::filesystem::absolute(AssetManager::Instance().GetAssetRoot()) / "scripts";
  if (!std::filesystem::exists(scripts_dir)) return out;

  for (const auto &entry : std::filesystem::directory_iterator(scripts_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".lua") continue;
    out.push_back(ToAssetRelativePath(entry.path()));
  }
  std::sort(out.begin(), out.end());
  return out;
}

/// @brief Starter source written when a new script is created in the editor.
constexpr const char *kLuaScriptTemplate = R"(-- New Lua entity script.
-- Attach it to an entity (Lua Script component) or use it as the scene main
-- script. `self` is the owning entity; hooks only run when they are defined.

function OnStart()
  MEngine.log("attached to: " .. self:get_name())
end

function OnUpdate(dt)
  -- dt = seconds since the last frame
end

function OnFixedUpdate(dt)
  -- dt = fixed step (1/60 s)
end

function OnCollisionEnter(other)
  MEngine.log("collision enter: '" .. self:get_name() .. "' vs '" .. other:get_name() .. "'")
end

function OnCollisionExit(other)
  MEngine.log("collision exit: '" .. self:get_name() .. "' vs '" .. other:get_name() .. "'")
end

function OnDestroy()
end
)";

enum class LogLevel { Trace, Debug, Info, Warn, Error, Fatal, Unknown };

/// @brief Extracts the `[LEVEL]` token from a log line ("[time] [INFO] [name] msg").
LogLevel ParseLogLevel(const std::string &line) {
  const size_t first = line.find('[');
  if (first == std::string::npos) return LogLevel::Unknown;
  const size_t second = line.find('[', first + 1);
  if (second == std::string::npos) return LogLevel::Unknown;
  const size_t end = line.find(']', second);
  if (end == std::string::npos) return LogLevel::Unknown;

  const std::string level = line.substr(second + 1, end - second - 1);
  if (level == "TRACE") return LogLevel::Trace;
  if (level == "DEBUG") return LogLevel::Debug;
  if (level == "INFO") return LogLevel::Info;
  if (level == "WARN") return LogLevel::Warn;
  if (level == "ERROR") return LogLevel::Error;
  if (level == "FATAL") return LogLevel::Fatal;
  return LogLevel::Unknown;
}

ImVec4 LogLevelColor(LogLevel level) {
  switch (level) {
    case LogLevel::Trace:
      return {0.52f, 0.54f, 0.57f, 1.0f};
    case LogLevel::Debug:
      return {0.10f, 0.45f, 0.78f, 1.0f};
    case LogLevel::Info:
      return {0.16f, 0.18f, 0.22f, 1.0f};
    case LogLevel::Warn:
      return {0.80f, 0.55f, 0.05f, 1.0f};
    case LogLevel::Error:
      return {0.80f, 0.15f, 0.15f, 1.0f};
    case LogLevel::Fatal:
      return {0.65f, 0.05f, 0.05f, 1.0f};
    default:
      return {0.30f, 0.32f, 0.36f, 1.0f};
  }
}

bool ContainsIgnoreCase(const std::string &haystack, const std::string &needle) {
  if (needle.empty()) return true;
  const auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(), [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
  });
  return it != haystack.end();
}

/// @brief Applies a polished light theme to the ImGui style.
void SetupImGuiStyle() {
  ImGui::StyleColorsLight();

  ImGuiStyle &style = ImGui::GetStyle();

  // Sharp, flat shapes (no rounded corners).
  style.WindowRounding    = 0.0f;
  style.ChildRounding     = 0.0f;
  style.FrameRounding     = 0.0f;
  style.PopupRounding     = 0.0f;
  style.ScrollbarRounding = 0.0f;
  style.GrabRounding      = 0.0f;
  style.TabRounding       = 0.0f;

  // Left-aligned titles read more like a native/modern editor.
  style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

  // Comfortable, less cramped spacing.
  style.WindowPadding    = ImVec2(10.0f, 10.0f);
  style.FramePadding     = ImVec2(6.0f, 4.0f);
  style.ItemSpacing      = ImVec2(8.0f, 5.0f);
  style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
  style.CellPadding      = ImVec2(6.0f, 4.0f);
  style.IndentSpacing    = 22.0f;
  style.ScrollbarSize    = 14.0f;
  style.GrabMinSize      = 10.0f;

  // Subtle borders give widgets definition without being harsh.
  style.WindowBorderSize = 1.0f;
  style.ChildBorderSize  = 1.0f;
  style.FrameBorderSize  = 1.0f;
  style.PopupBorderSize  = 1.0f;
  style.TabBorderSize    = 1.0f;

  // Calmer blue-gray accent palette on top of the light base.
  ImVec4 *colors                      = style.Colors;
  colors[ImGuiCol_WindowBg]           = ImVec4(0.96f, 0.96f, 0.97f, 1.00f);
  colors[ImGuiCol_ChildBg]            = ImVec4(0.93f, 0.93f, 0.95f, 1.00f);
  colors[ImGuiCol_PopupBg]            = ImVec4(0.98f, 0.98f, 0.99f, 1.00f);
  colors[ImGuiCol_Text]               = ImVec4(0.15f, 0.17f, 0.20f, 1.00f);
  colors[ImGuiCol_TextDisabled]       = ImVec4(0.55f, 0.57f, 0.62f, 1.00f);
  colors[ImGuiCol_FrameBg]            = ImVec4(0.86f, 0.87f, 0.90f, 1.00f);
  colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.80f, 0.82f, 0.86f, 1.00f);
  colors[ImGuiCol_FrameBgActive]      = ImVec4(0.74f, 0.77f, 0.83f, 1.00f);
  colors[ImGuiCol_TitleBg]            = ImVec4(0.90f, 0.91f, 0.94f, 1.00f);
  colors[ImGuiCol_TitleBgActive]      = ImVec4(0.70f, 0.78f, 0.92f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed]   = ImVec4(0.90f, 0.91f, 0.94f, 1.00f);
  colors[ImGuiCol_MenuBarBg]          = ImVec4(0.93f, 0.94f, 0.96f, 1.00f);
  colors[ImGuiCol_Header]             = ImVec4(0.83f, 0.86f, 0.91f, 0.80f);
  colors[ImGuiCol_HeaderHovered]      = ImVec4(0.76f, 0.80f, 0.87f, 0.80f);
  colors[ImGuiCol_HeaderActive]       = ImVec4(0.68f, 0.74f, 0.83f, 1.00f);
  colors[ImGuiCol_Button]             = ImVec4(0.82f, 0.84f, 0.89f, 1.00f);
  colors[ImGuiCol_ButtonHovered]      = ImVec4(0.73f, 0.77f, 0.84f, 1.00f);
  colors[ImGuiCol_ButtonActive]       = ImVec4(0.63f, 0.68f, 0.78f, 1.00f);
  colors[ImGuiCol_CheckMark]          = ImVec4(0.16f, 0.35f, 0.62f, 1.00f);
  colors[ImGuiCol_SliderGrab]         = ImVec4(0.40f, 0.52f, 0.72f, 1.00f);
  colors[ImGuiCol_SliderGrabActive]   = ImVec4(0.26f, 0.40f, 0.62f, 1.00f);
  colors[ImGuiCol_Separator]          = ImVec4(0.60f, 0.62f, 0.67f, 0.50f);
  colors[ImGuiCol_SeparatorHovered]   = ImVec4(0.40f, 0.50f, 0.66f, 0.80f);
  colors[ImGuiCol_SeparatorActive]    = ImVec4(0.30f, 0.42f, 0.60f, 1.00f);
  colors[ImGuiCol_Tab]                = ImVec4(0.86f, 0.88f, 0.92f, 1.00f);
  colors[ImGuiCol_TabHovered]         = ImVec4(0.73f, 0.77f, 0.84f, 1.00f);
  colors[ImGuiCol_TabActive]          = ImVec4(0.62f, 0.70f, 0.86f, 1.00f);
  colors[ImGuiCol_TabUnfocused]       = ImVec4(0.88f, 0.89f, 0.92f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.73f, 0.77f, 0.85f, 1.00f);
  colors[ImGuiCol_DockingPreview]     = ImVec4(0.30f, 0.50f, 0.78f, 0.70f);
}

/// @brief Draws a crisp amber folder icon with the draw list (clearly visible
/// on the light theme, unlike the old near-white folder texture).
void DrawFolderIcon(ImDrawList *draw, const ImVec2 &min, const ImVec2 &max) {
  const ImU32 body  = ImGui::GetColorU32(ImVec4(0.96f, 0.78f, 0.30f, 1.00f));
  const ImU32 tab   = ImGui::GetColorU32(ImVec4(0.86f, 0.60f, 0.12f, 1.00f));
  const float w     = max.x - min.x;
  const float h     = max.y - min.y;
  const float pad_x = w * 0.06f;
  const float tab_h = h * 0.10f;
  const float tab_w = w * 0.42f;

  const ImVec2 tab_min(min.x + pad_x, min.y);
  const ImVec2 tab_max(tab_min.x + tab_w, tab_min.y + tab_h);
  draw->AddRectFilled(tab_min, tab_max, tab);

  const ImVec2 body_min(min.x + pad_x, min.y + tab_h);
  const ImVec2 body_max(max.x - pad_x, max.y - h * 0.04f);
  draw->AddRectFilled(body_min, body_max, body);
}

}  // namespace

Editor::Editor() : Application(Application::GetStartupApi()) {}

Editor::~Editor() {
  if (rhi_) {
    rhi_->ShutdownImGuiBackend();
  }
  if (ImGui::GetCurrentContext()) {
    ImGui::DestroyContext();
  }
}

void Editor::Initialize() {
  PROFILER_FUNCTION();

active_scene_ = std::make_shared<Scene>();

  // Tune lighting to match the sandbox's validated look: lower IBL ambient so
  // meshes don't read as self-emissive, softer shadows, and gentler bloom.
  active_scene_->SetIblIntensity(0.4f);
  active_scene_->SetExposure(1.1f);
  active_scene_->SetBloomStrength(0.015f);
  active_scene_->SetShadowPcfRadius(4.0f);
  active_scene_->SetGodRaysStrength(0.06f);

  // ImGUI setup
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // DPI-aware font sizing so the UI stays crisp on high-DPI displays.
  int window_w = 0;
  int window_h = 0;
  glfwGetWindowSize(window_, &window_w, &window_h);
  int fb_w = 0;
  int fb_h = 0;
  glfwGetFramebufferSize(window_, &fb_w, &fb_h);
  float dpi_scale = (window_w > 0 && fb_w > 0) ? static_cast<float>(fb_w) / static_cast<float>(window_w) : 1.0f;
  if (dpi_scale < 1.0f || dpi_scale > 4.0f) dpi_scale = 1.0f;

  // Modern sans-serif for the UI, monospace for the Log panel.
  ImFontConfig font_cfg{};
  font_cfg.OversampleH = 3;
  font_cfg.OversampleV = 3;
  io.Fonts->Clear();
  if (!io.Fonts->AddFontFromFileTTF("res/fonts/Roboto-Medium.ttf", 17.0f * dpi_scale, &font_cfg)) {
    io.Fonts->AddFontDefault();
  }
  mono_font_ = io.Fonts->AddFontFromFileTTF("res/fonts/Cousine-Regular.ttf", 15.0f * dpi_scale, &font_cfg);

SetupImGuiStyle();

  if (rhi_ && !rhi_->InitializeImGuiBackend(window_)) {
    LOG_FATAL("Editor") << "Failed to initialize ImGui backend for selected RHI";
    exit(-1);
  }

  // Viewport framebuffer: match the window framebuffer on startup; it is
  // resized to the viewport image area afterwards.
  frame_buffer_ = std::make_shared<FrameBuffer>();
  int fb_width  = 0;
  int fb_height = 0;
  glfwGetFramebufferSize(window_, &fb_width, &fb_height);
  if (fb_width > 0 && fb_height > 0) {
    frame_buffer_->Resize(fb_width, fb_height);
    frame_buffer_->CheckStatus();
    viewport_width_  = fb_width;
    viewport_height_ = fb_height;
  }

  editor_camera_.Reset();
  editor_camera_.aspect = static_cast<float>(fb_width) / static_cast<float>(std::max(1, fb_height));

default_material_ = CreateDefaultMaterial();

  // Ground grid: a large XZ plane with a procedural grid shader. The grid is
  // an editor-only overlay, hidden while Play mode is simulating.
  grid_material_ = CreateRef<Material>();
  grid_material_->SetShader(AssetManager::Instance().GetShader("grid"));
  grid_mesh_   = Mesh::CreatePlane(500.0f);
  grid_entity_ = active_scene_->CreateEntity("Grid");
  grid_entity_.GetComponent<Tag>().editor_only = true;
  grid_entity_.AddComponent<Transform>();
  grid_entity_.AddComponent<MeshComponent>(grid_mesh_, grid_material_);

  // Physics demo: a static ground box plus a stack of dynamic boxes topped
  // with a sphere. Everything is at rest until Play is pressed.
  CreatePhysicsDemo();

  // Scene-level Lua main script (optional GameManager).
  active_scene_->SetMainScript("scripts/main.lua");

  base_directory_    = std::filesystem::absolute(AssetManager::Instance().GetAssetRoot());
  current_directory_ = base_directory_;
  directory_icon_    = AssetManager::Instance().GetTexture("icons/DirectoryIcon.png");
  file_icon_         = AssetManager::Instance().GetTexture("icons/FileIcon.png");

  LOG_INFO("Editor") << "Editor initialized (scene + ImGui + viewport framebuffer)";
}

void Editor::OnUpdate(float dt) {
  PROFILER_FUNCTION();

  // Unattended verification of the File-menu scene ops. Enabled with
  // MENGINE_EDITOR_SELFTEST_SCENE=<path>; runs once on the first frame and
  // restores the default demo scene afterwards so interactive use is unaffected.
  static bool        scene_selftest_done = false;
  const std::string  selftest_scene      = GetEnvVar("MENGINE_EDITOR_SELFTEST_SCENE");
  if (!selftest_scene.empty() && !scene_selftest_done) {
    scene_selftest_done = true;
    RunSceneFileSelftest(selftest_scene);
  }

  if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) {
    glfwSetWindowShouldClose(window_, true);
  }

  if (viewport_resized_) {
    LOG_TRACE("Editor") << "Viewport resized to " << viewport_width_ << "x" << viewport_height_;
    frame_buffer_->Resize(viewport_width_, viewport_height_);
    frame_buffer_->CheckStatus();
    viewport_resized_ = false;
  }

  // Sync the editor's point lights into the scene renderer.
  active_scene_->ClearPointLights();
  for (const auto &light : point_lights_) {
    active_scene_->AddPointLight(light);
  }

  // Advance the physics simulation and Lua scripts while in Play mode.
  // StepSimulation runs physics, collision dispatch and OnFixedUpdate together
  // on a fixed step; Update drives per-frame OnStart/OnUpdate afterwards.
  if (game_mode_ == GameMode::Play) {
    active_scene_->StepSimulation(dt);
    active_scene_->GetScriptEngine().Update(dt);
  }

  // Render the 3D scene into the viewport framebuffer (Edit = editor camera,
  // Play = the scene's primary camera, falling back to the editor camera when
  // no primary camera has been placed).
  editor_camera_.aspect = static_cast<float>(viewport_width_) / static_cast<float>(std::max(1, viewport_height_));
  if (game_mode_ == GameMode::Play && active_scene_->HasPrimaryCamera()) {
    active_scene_->RenderFromPrimaryCamera(frame_buffer_->GetFrameBufferId(), viewport_width_, viewport_height_);
  } else {
    active_scene_->RenderMeshes(editor_camera_.GetViewMatrix(), editor_camera_.GetProjectionMatrix(),
                                editor_camera_.GetPosition(), frame_buffer_->GetFrameBufferId(), viewport_width_,
                                viewport_height_);
  }

  // The scene composite leaves the viewport framebuffer bound; unbind it so
  // ImGui draws to the window instead of into the offscreen texture.
  frame_buffer_->Unbind();

  BeginImGui();

  // Editor shortcuts (ignored while typing in a text field).
  if (!ImGui::GetIO().WantTextInput) {
    if (!editor_camera_.IsFlyMode() && ImGui::IsKeyPressed(ImGuiKey_W)) gizmo_operation_ = ImGuizmo::TRANSLATE;
    if (!editor_camera_.IsFlyMode() && ImGui::IsKeyPressed(ImGuiKey_E)) gizmo_operation_ = ImGuizmo::ROTATE;
    if (!editor_camera_.IsFlyMode() && ImGui::IsKeyPressed(ImGuiKey_R)) gizmo_operation_ = ImGuizmo::SCALE;
    if (ImGui::IsKeyPressed(ImGuiKey_F) && selected_entity_.GetHandle() != entt::null &&
        selected_entity_.HasComponent<Transform>()) {
      editor_camera_.target = selected_entity_.GetComponent<Transform>().translation;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_D) && ImGui::GetIO().KeyCtrl) {
      DuplicateSelectedEntity();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_N) && ImGui::GetIO().KeyCtrl) {
      NewScene();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_O) && ImGui::GetIO().KeyCtrl) {
      OpenSceneDialog();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_S) && ImGui::GetIO().KeyCtrl) {
      SaveCurrentScene();
    }
  }

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
        NewScene();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
        OpenSceneDialog();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Save", "Ctrl+S")) {
        SaveCurrentScene();
      }
      if (ImGui::MenuItem("Save Scene As...", nullptr)) {
        SaveSceneAsDialog();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Close Scene")) {
        ExitGameModeForFileOp();
        NewScene();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit")) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem("Content Browser", nullptr, &show_content_browser_);
      ImGui::MenuItem("Scene", nullptr, &show_scene_);
      ImGui::MenuItem("Viewport", nullptr, &show_viewport_);
      ImGui::MenuItem("Properties", nullptr, &show_properties_);
      ImGui::MenuItem("Lighting", nullptr, &show_lighting_);
      ImGui::MenuItem("Rendering", nullptr, &show_rendering_);
      ImGui::MenuItem("Log", nullptr, &show_log_);
      ImGui::MenuItem("Information", nullptr, &show_information_);
      ImGui::MenuItem("Script Editor", nullptr, &show_script_editor_);
      ImGui::Separator();
      if (ImGui::MenuItem("Reset Layout")) {
        ApplyDefaultLayout(dockspace_id_);
      }
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }

  if (show_content_browser_) ShowImGuiContentBrowser();
  if (show_viewport_) ShowImGuiViewport();
  if (show_scene_) ShowImGuiScene();
  if (show_properties_) ShowImGuiProperties();
  if (show_lighting_) ShowImGuiLighting();
  if (show_rendering_) ShowImGuiRendering();
  if (show_log_) ShowImGuiLog();

  if (show_information_) ShowImGuiInformation();

  if (show_script_editor_) ShowImGuiScriptEditor();

  // Close the "DockSpace Demo" host window opened in BeginImGui().
  ImGui::End();

  EndImGui();
}

void Editor::BeginImGui() {
  PROFILER_FUNCTION();
  // ImGui test
  if (rhi_) {
    rhi_->BeginImGuiFrame();
  }
  ImGui::NewFrame();

  // Note: Switch this to true to enable dockspace
  static bool               dockspaceOpen             = true;
  static bool               opt_fullscreen_persistant = true;
  const bool                opt_fullscreen            = opt_fullscreen_persistant;
  static ImGuiDockNodeFlags dockspace_flags           = ImGuiDockNodeFlags_None;

  // We are using the ImGuiWindowFlags_NoDocking flag to make the parent
  // window not dockable into, because it would be confusing to have two
  // docking targets within each others.
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  if (opt_fullscreen) {
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |=
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
  }

  // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will
  // render our background and handle the pass-thru hole, so we ask Begin() to
  // not render a background.
  if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) window_flags |= ImGuiWindowFlags_NoBackground;

  // Important: note that we proceed even if Begin() returns false (aka window
  // is collapsed). This is because we want to keep our DockSpace() active. If
  // a DockSpace() is inactive, all active windows docked into it will lose
  // their parent and become undocked. We cannot preserve the docking
  // relationship between an active window and an inactive docking, otherwise
  // any change of dockspace/settings would lead to windows being stuck in
  // limbo and never being visible.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
  ImGui::PopStyleVar();

  if (opt_fullscreen) ImGui::PopStyleVar(2);

  // DockSpace
  ImGuiIO    &io        = ImGui::GetIO();
  ImGuiStyle &style     = ImGui::GetStyle();
  style.WindowMinSize.x = 370.0f;
  if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    dockspace_id_        = dockspace_id;

    // Build a sensible default layout the first time (no saved layout yet).
    static bool first_layout = true;
    if (first_layout) {
      first_layout = false;
      if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        ApplyDefaultLayout(dockspace_id);
      }
    }

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
  }
}

void Editor::EndImGui() {
  PROFILER_FUNCTION();
  ImGui::Render();
  if (rhi_) {
    rhi_->RenderImGuiDrawData(ImGui::GetDrawData());
  }
}

template <typename T, typename UIFunction>
static void DrawComponent(const std::string &name, Entity entity, UIFunction uiFunction) {
  PROFILER_FUNCTION();
  constexpr ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
                                               ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap |
                                               ImGuiTreeNodeFlags_FramePadding;
  if (entity.HasComponent<T>()) {
    auto        &component              = entity.GetComponent<T>();
    const ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4, 4});
    const float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
    ImGui::Separator();
    const bool open =
        ImGui::TreeNodeEx(reinterpret_cast<void *>(typeid(T).hash_code()), treeNodeFlags, "%s", name.c_str());
    ImGui::PopStyleVar();
    ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
    if (ImGui::Button("+", ImVec2{lineHeight, lineHeight})) {
      ImGui::OpenPopup("ComponentSettings");
    }

    bool removeComponent = false;
    if (ImGui::BeginPopup("ComponentSettings")) {
      if (ImGui::MenuItem("Remove component")) removeComponent = true;

      ImGui::EndPopup();
    }

    if (open) {
      uiFunction(component);
      ImGui::TreePop();
    }

    if (removeComponent) entity.RemoveComponent<T>();
  }
}

static void DrawVec3Control(const std::string &label, glm::vec3 &values, float resetValue = 0.0f,
                            float columnWidth = 100.0f) {
  PROFILER_FUNCTION();
  const ImGuiIO &io       = ImGui::GetIO();
  const auto     boldFont = io.Fonts->Fonts[0];

  ImGui::PushID(label.c_str());

  ImGui::Columns(2);
  ImGui::SetColumnWidth(0, columnWidth);
  ImGui::Text("%s", label.c_str());
  ImGui::NextColumn();

  ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});

  const float  lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
  const ImVec2 buttonSize = {lineHeight + 3.0f, lineHeight};

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.9f, 0.2f, 0.2f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
  ImGui::PushFont(boldFont);
  if (ImGui::Button("X", buttonSize)) values.x = resetValue;
  ImGui::PopFont();
  ImGui::PopStyleColor(3);

  ImGui::SameLine();
  ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
  ImGui::PopItemWidth();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.3f, 0.8f, 0.3f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
  ImGui::PushFont(boldFont);
  if (ImGui::Button("Y", buttonSize)) values.y = resetValue;
  ImGui::PopFont();
  ImGui::PopStyleColor(3);

  ImGui::SameLine();
  ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
  ImGui::PopItemWidth();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.2f, 0.35f, 0.9f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
  ImGui::PushFont(boldFont);
  if (ImGui::Button("Z", buttonSize)) values.z = resetValue;
  ImGui::PopFont();
  ImGui::PopStyleColor(3);

  ImGui::SameLine();
  ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
  ImGui::PopItemWidth();

  ImGui::PopStyleVar();

  ImGui::Columns(1);

  ImGui::PopID();
}

template <typename T>
void Editor::DisplayAddComponentEntry(const std::string &entryName) {
  PROFILER_FUNCTION();
  if (!selected_entity_.HasComponent<T>()) {
    if (ImGui::MenuItem(entryName.c_str())) {
      selected_entity_.AddComponent<T>();
      ImGui::CloseCurrentPopup();
    }
  }
}

void Editor::ShowImGuiScene() {
  PROFILER_FUNCTION();
  ImGui::Begin("Scene");

  // Scene-level main Lua script (optional GameManager-style script).
  {
    char main_script[512] = {};
    std::snprintf(main_script, sizeof(main_script), "%s", active_scene_->GetMainScript().c_str());
    if (ImGui::InputText("Main Script", main_script, sizeof(main_script))) {
      active_scene_->SetMainScript(std::string(main_script));
    }
  }

  if (ImGui::Button("Create")) {
    ImGui::OpenPopup("CreateEntity");
  }

  if (ImGui::BeginPopup("CreateEntity")) {
    if (ImGui::MenuItem("Empty Entity")) CreatePrimitive("Empty", nullptr);
    if (ImGui::MenuItem("Cube")) CreatePrimitive("Cube", AssetManager::Instance().GetMesh("cube"));
    if (ImGui::MenuItem("Plane")) CreatePrimitive("Plane", AssetManager::Instance().GetMesh("plane"));
    if (ImGui::MenuItem("Sphere")) CreatePrimitive("Sphere", AssetManager::Instance().GetMesh("sphere"));
    ImGui::Separator();
    if (ImGui::MenuItem("Camera")) CreateCameraEntity();
    ImGui::EndPopup();
  }

  const bool has_selection = selected_entity_.GetHandle() != entt::null && selected_entity_ != grid_entity_;

  ImGui::SameLine();
  if (!has_selection) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Delete")) {
    active_scene_->DestroyEntity(selected_entity_);
    selected_entity_ = Entity();
  }
  ImGui::SameLine();
  if (ImGui::Button("Duplicate")) {
    DuplicateSelectedEntity();
  }
  if (!has_selection) {
    ImGui::EndDisabled();
  }

  // Filter the hierarchy by name.
  static char search[128] = {};
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##SceneSearch", "Search...", search, sizeof(search));
  ImGui::Separator();

  for (auto &entity : active_scene_->GetAllEntities()) {
    if (entity == grid_entity_) {
      continue;
    }

    const std::string &tag = entity.GetComponent<Tag>().tag;
    if (search[0] != '\0' && !ContainsIgnoreCase(tag, search)) {
      continue;
    }

    const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                     (entity == selected_entity_ ? ImGuiTreeNodeFlags_Selected : 0);
    ImGui::TreeNodeEx(tag.c_str(), flags);
    if (ImGui::IsItemClicked()) {
      selected_entity_ = entity;
    }
  }

  ImGui::End();
}

void Editor::ShowImGuiViewport() {
  PROFILER_FUNCTION();
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("Viewport");

  const ImVec2 avail = ImGui::GetContentRegionAvail();

  // Toolbar: fly/orbit toggle + play/stop. Gizmo operations live in an
  // icon-button overlay at the bottom-left of the viewport image.
  constexpr float toolbar_height = 26.0f;
  ImGui::BeginChild("##ViewportToolbar", ImVec2(avail.x, toolbar_height));
  if (ImGui::Button(editor_camera_.IsFlyMode() ? "Orbit" : "Fly")) {
    editor_camera_.SetFlyMode(!editor_camera_.IsFlyMode());
  }
  ImGui::SameLine();
  const char *play_label = game_mode_ == GameMode::Edit ? "Play" : "Stop";
  if (ImGui::Button(play_label)) {
    if (game_mode_ == GameMode::Edit) {
      game_mode_ = GameMode::Play;
      active_scene_->StartSimulation();
      active_scene_->GetScriptEngine().LoadMainScript(active_scene_->GetMainScript());
      // Start OnStart on every script before the first physics step so
      // collisions on frame one are delivered to already-started scripts.
      active_scene_->GetScriptEngine().StartAll();
      SetGridVisible(false);
    } else {
      game_mode_ = GameMode::Edit;
      // Fire OnDestroy hooks first (while the entities are still alive), then
      // restore the authoring scene captured at Play start.
      active_scene_->GetScriptEngine().Clear();
      active_scene_->StopSimulation();
      SetGridVisible(true);

      // The snapshot restore re-creates entities; drop a selection that no
      // longer exists so the Properties panel never dereferences a stale handle.
      if (selected_entity_.GetHandle() != entt::null &&
          !active_scene_->GetRegistry().valid(selected_entity_.GetHandle())) {
        selected_entity_ = Entity();
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Launch")) {
    LaunchStandalone();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Run the scene standalone in a new window");
  }
  ImGui::EndChild();

  // Image fills the remaining region; clamp to a minimum so a degenerate
  // (collapsed) window never renders a 0/1-pixel framebuffer.
  ImVec2 image_size(avail.x, avail.y - toolbar_height - 4.0f);
  if (image_size.x < 64.0f) image_size.x = 64.0f;
  if (image_size.y < 64.0f) image_size.y = 64.0f;

  // Resize the viewport framebuffer when the image area changes.
  const int w = static_cast<int>(image_size.x);
  const int h = static_cast<int>(image_size.y);
  if (w != viewport_width_ || h != viewport_height_) {
    viewport_width_   = w;
    viewport_height_  = h;
    viewport_resized_ = true;
  }

  ImGui::Image(reinterpret_cast<void *>(static_cast<intptr_t>(frame_buffer_->GetTextureId())), image_size, ImVec2(0, 1),
               ImVec2(1, 0));

  // Drop a model (OBJ / glTF) from the Content Browser to import it.
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
      if (payload->Data != nullptr) {
        const std::filesystem::path file_path(static_cast<const wchar_t *>(payload->Data));
        const std::string           ext = file_path.extension().string();
        if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
          CreateModelEntity(file_path);
        }
      }
    }
    ImGui::EndDragDropTarget();
  }

  const ImVec2 image_pos  = ImGui::GetItemRectMin();
  const ImVec2 image_area = ImGui::GetItemRectSize();

  // Editor camera input while hovering the viewport (Edit mode only).
  const bool hovered     = ImGui::IsItemHovered();
  const bool using_gizmo = ImGuizmo::IsUsing();
  if (hovered && !using_gizmo && game_mode_ == GameMode::Edit) {
    const ImGuiIO &io = ImGui::GetIO();
    if (editor_camera_.IsFlyMode()) {
      if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        editor_camera_.LookAround(io.MouseDelta.x, io.MouseDelta.y);
      }
      float forward = 0.0f;
      float right   = 0.0f;
      float up      = 0.0f;
      if (ImGui::IsKeyDown(ImGuiKey_W)) forward += 1.0f;
      if (ImGui::IsKeyDown(ImGuiKey_S)) forward -= 1.0f;
      if (ImGui::IsKeyDown(ImGuiKey_D)) right += 1.0f;
      if (ImGui::IsKeyDown(ImGuiKey_A)) right -= 1.0f;
      if (ImGui::IsKeyDown(ImGuiKey_E)) up += 1.0f;
      if (ImGui::IsKeyDown(ImGuiKey_Q)) up -= 1.0f;
      editor_camera_.MoveLocal(forward, right, up, io.DeltaTime);
    } else {
      if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        editor_camera_.Orbit(io.MouseDelta.x, io.MouseDelta.y);
      }
      if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        editor_camera_.Pan(io.MouseDelta.x, io.MouseDelta.y);
      }
      if (io.MouseWheel != 0.0f) {
        editor_camera_.Zoom(io.MouseWheel);
      }
    }
  }

  // Scene camera controllers while hovering the viewport (Play mode only).
  if (hovered && game_mode_ == GameMode::Play) {
    const ImGuiIO &io = ImGui::GetIO();
    active_scene_->UpdateCameraControllers(io.DeltaTime, glm::vec2(io.MouseDelta.x, io.MouseDelta.y),
                                           io.MouseDown[ImGuiMouseButton_Right]);
  }

  if (game_mode_ == GameMode::Edit) {
    ShowGizmo(image_pos, image_area);
    DrawCameraGizmos(image_pos, image_area);
    if (show_colliders_) {
      DrawColliderGizmos(image_pos, image_area);
    }

    // Bottom-left overlay: small icon-like buttons for the gizmo operation.
    constexpr float btn_size = 24.0f;
    constexpr float padding  = 8.0f;
    ImGui::SetCursorScreenPos(ImVec2(image_pos.x + padding, image_pos.y + image_area.y - btn_size - padding));

    struct GizmoButton {
      const char         *label;
      const char         *tooltip;
      ImGuizmo::OPERATION operation;
    };
    const GizmoButton buttons[] = {
        {"T", "Translate (W)", ImGuizmo::TRANSLATE},
        {"R", "Rotate (E)", ImGuizmo::ROTATE},
        {"S", "Scale (R)", ImGuizmo::SCALE},
    };
    for (const GizmoButton &button : buttons) {
      ImGui::PushID(button.label);
      const bool active = gizmo_operation_ == button.operation;
      if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.54f, 0.92f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.60f, 0.98f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.48f, 0.86f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
      }
      if (ImGui::Button(button.label, ImVec2(btn_size, btn_size))) {
        gizmo_operation_ = button.operation;
      }
      if (active) {
        ImGui::PopStyleColor(4);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", button.tooltip);
      }
      ImGui::PopID();
      ImGui::SameLine();
    }
  }

  ImGui::End();
  ImGui::PopStyleVar();
}

void Editor::ShowImGuiProperties() {
  PROFILER_FUNCTION();
  ImGui::Begin("Properties");

  if (selected_entity_.GetHandle() != entt::null) {
    auto &tag         = selected_entity_.GetComponent<Tag>().tag;
    char  buffer[256] = {};
    std::snprintf(buffer, sizeof(buffer), "%s", tag.c_str());
    if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
      tag = std::string(buffer);
    }

    if (ImGui::Button("Add Component")) {
      ImGui::OpenPopup("AddComponent");
    }

    if (ImGui::BeginPopup("AddComponent")) {
      DisplayAddComponentEntry<Transform>("Transform");
      DisplayAddComponentEntry<MeshComponent>("Mesh");
      DisplayAddComponentEntry<CameraComponent>("Camera");
      DisplayAddComponentEntry<CameraController>("Camera Controller");
      DisplayAddComponentEntry<LuaScriptComponent>("Lua Script");
      DisplayAddComponentEntry<RigidBodyComponent>("Rigid Body");
      DisplayAddComponentEntry<ColliderComponent>("Collider");
      DisplayAddComponentEntry<ColliderGroupComponent>("Collider Group");

      ImGui::EndPopup();
    }

    ImGui::Separator();

    DrawComponent<Transform>("Transform", selected_entity_, [](auto &component) {
      DrawVec3Control("Translation", component.translation);
      glm::vec3 rotation = glm::degrees(component.rotation);
      DrawVec3Control("Rotation", rotation);
      component.rotation = glm::radians(rotation);
      DrawVec3Control("Scale", component.scale, 1.0f);
    });

    DrawComponent<MeshComponent>("Mesh", selected_entity_, [](auto &component) {
      // Drop zone: drag a model file (.obj/.gltf/.glb) here to (re)assign the mesh.
      ImGui::InvisibleButton("##MeshDropZone", ImVec2(ImGui::GetContentRegionAvail().x, 24.0f));
      const ImVec2 zone_min = ImGui::GetItemRectMin();
      const ImVec2 zone_max = ImGui::GetItemRectMax();
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
          const std::filesystem::path file_path(static_cast<const wchar_t *>(payload->Data));
          const std::string           ext = file_path.extension().string();
          if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
            Ref<Mesh>     mesh;
            Ref<Material> material;
            if (LoadModelAsset(file_path, mesh, material)) {
              component.mesh     = mesh;
              component.material = material;
              LOG_INFO("Editor") << "Assigned model '" << file_path.filename().string() << "' to entity";
            } else {
              LOG_WARN("Editor") << "Failed to load model: " << file_path;
            }
          }
        }
        ImGui::EndDragDropTarget();
      }
      ImGui::GetWindowDrawList()->AddRect(zone_min, zone_max, ImGui::GetColorU32(ImGuiCol_Separator));
      ImGui::SetCursorScreenPos(ImVec2(zone_min.x + 6.0f, zone_min.y + 5.0f));
      ImGui::TextDisabled(
          "%s", component.mesh ? "Drop a model here to replace the mesh" : "Drop a model here to assign a mesh");

      if (!component.mesh) {
        if (ImGui::Button("Cube")) component.mesh = Mesh::CreateCube();
        ImGui::SameLine();
        if (ImGui::Button("Plane")) component.mesh = Mesh::CreatePlane();
        ImGui::SameLine();
        if (ImGui::Button("Sphere")) component.mesh = Mesh::CreateSphere();
      } else {
        ImGui::Text("Triangles: %d", component.mesh->GetIndexCount() / 3);
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove Mesh")) component.mesh = nullptr;
      }

      if (!component.material) {
        component.material = CreateDefaultMaterial();
      }

      {
        Material *material = component.material.get();

        // Texture maps: thumbnails in a row, label underneath. Drag an image
        // from the Content Browser to assign; right-click to clear.
        const float thumb    = 64.0f;
        auto        draw_map = [&](const char *label, const Ref<Texture> &get, auto &&set) {
          ImGui::BeginGroup();
          if (get) {
            ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(get->GetID())), {thumb, thumb},
                         ImVec2(0, 1), ImVec2(1, 0));
          } else {
            ImGui::Button("None", {thumb, thumb});
          }
          if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
              const auto *path = static_cast<const wchar_t *>(payload->Data);
              set(Texture::Create(std::filesystem::path(path).string()));
            }
            ImGui::EndDragDropTarget();
          }
          if (get && ImGui::BeginPopupContextItem(label)) {
            if (ImGui::MenuItem("Clear")) {
              set(nullptr);
            }
            ImGui::EndPopup();
          }
          const float text_w = ImGui::CalcTextSize(label).x;
          ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (thumb - text_w) * 0.5f);
          ImGui::Text("%s", label);
          ImGui::EndGroup();
        };

        draw_map("Albedo", material->GetAlbedoMap(), [&](Ref<Texture> t) { material->SetAlbedoMap(t); });
        ImGui::SameLine();
        draw_map("Normal", material->GetNormalMap(), [&](Ref<Texture> t) { material->SetNormalMap(t); });
        ImGui::SameLine();
        draw_map("Roughness", material->GetMetallicRoughnessMap(),
                 [&](Ref<Texture> t) { material->SetMetallicRoughnessMap(t); });
        ImGui::SameLine();
        draw_map("AO", material->GetAOMap(), [&](Ref<Texture> t) { material->SetAOMap(t); });

        ImGui::Separator();
        ImGui::Text("Properties");

        glm::vec4 base_color = material->GetBaseColorFactor();
        if (ImGui::ColorEdit4("Base Color", glm::value_ptr(base_color))) {
          material->SetBaseColorFactor(base_color);
        }

        float metallic = material->GetMetallicFactor();
        if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
          material->SetMetallicFactor(metallic);
        }

        float roughness = material->GetRoughnessFactor();
        if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
          material->SetRoughnessFactor(roughness);
        }

        float specular = material->GetSpecularFactor();
        if (ImGui::SliderFloat("Specular", &specular, 0.0f, 1.0f)) {
          material->SetSpecularFactor(specular);
        }
      }
    });

    DrawComponent<CameraComponent>("Camera", selected_entity_, [&](auto &component) {
      bool primary = component.primary;
      if (ImGui::Checkbox("Primary", &primary)) {
        component.primary = primary;
        if (primary) {
          // A scene has at most one primary camera.
          for (auto &entity : active_scene_->GetAllEntities()) {
            if (entity != selected_entity_ && entity.HasComponent<CameraComponent>()) {
              entity.GetComponent<CameraComponent>().primary = false;
            }
          }
        }
      }

      DrawVec3Control("Position", component.camera.position);
      DrawVec3Control("Rotation", component.camera.rotation);

      const char *items[] = {"Perspective", "Orthographic"};
      int         current = component.camera.projection_type == ProjectionType::Perspective ? 0 : 1;
      if (ImGui::Combo("Projection", &current, items, 2)) {
        component.camera.projection_type = current == 0 ? ProjectionType::Perspective : ProjectionType::Orthographic;
      }

      ImGui::DragFloat("FOV", &component.camera.fov_degrees, 0.5f, 1.0f, 179.0f);
      ImGui::DragFloat("Ortho Size", &component.camera.ortho_size, 0.1f, 0.1f, 1000.0f);
      ImGui::DragFloat("Near", &component.camera.near_plane, 0.01f, 0.001f, 1000.0f);
      ImGui::DragFloat("Far", &component.camera.far_plane, 1.0f, 0.1f, 10000.0f);
    });

    DrawComponent<RigidBodyComponent>("Rigid Body", selected_entity_, [](auto &component) {
      const char *types[] = {"Static", "Dynamic"};
      int         current = component.type == RigidBodyComponent::Type::Static ? 0 : 1;
      if (ImGui::Combo("Type", &current, types, 2)) {
        component.type = current == 0 ? RigidBodyComponent::Type::Static : RigidBodyComponent::Type::Dynamic;
      }
      ImGui::SliderFloat("Friction", &component.friction, 0.0f, 1.0f);
      ImGui::SliderFloat("Restitution", &component.restitution, 0.0f, 1.0f);
      ImGui::Checkbox("Continuous (CCD)", &component.continuous_collision);
      ImGui::Checkbox("Sensor (trigger)", &component.is_sensor);
    });

    DrawComponent<ColliderComponent>("Collider", selected_entity_, [](auto &component) {
      const char *shapes[] = {"Box", "Sphere", "Capsule", "Cylinder"};
      int         current  = static_cast<int>(component.shape);
      if (ImGui::Combo("Shape", &current, shapes, 4)) {
        component.shape = static_cast<ColliderComponent::Shape>(current);
      }
      switch (component.shape) {
        case ColliderComponent::Shape::Sphere:
          ImGui::DragFloat("Radius", &component.sphere_radius, 0.01f, 0.01f, 100.0f);
          break;
        case ColliderComponent::Shape::Capsule:
          ImGui::DragFloat("Radius", &component.capsule_radius, 0.01f, 0.01f, 100.0f);
          ImGui::DragFloat("Half Height", &component.capsule_half_height, 0.01f, 0.01f, 100.0f);
          break;
        case ColliderComponent::Shape::Cylinder:
          ImGui::DragFloat("Radius", &component.cylinder_radius, 0.01f, 0.01f, 100.0f);
          ImGui::DragFloat("Half Height", &component.cylinder_half_height, 0.01f, 0.01f, 100.0f);
          break;
        case ColliderComponent::Shape::Box:
        default:
          DrawVec3Control("Half Extents", component.box_half_extents, 1.0f);
          break;
      }
      DrawVec3Control("Offset", component.offset, 1.0f);
    });

    // Compound collider group: extra shapes merged with the primary collider.
    DrawComponent<ColliderGroupComponent>("Collider Group", selected_entity_, [](auto &group) {
      constexpr const char *kShapeNames[] = {"Box", "Sphere", "Capsule", "Cylinder"};

      const auto draw_shape = [&kShapeNames](ColliderShapeData &s) {
        int current = static_cast<int>(s.shape);
        if (ImGui::Combo("Type", &current, kShapeNames, 4)) {
          s.shape = static_cast<ColliderShapeData::Shape>(current);
        }
        switch (s.shape) {
          case ColliderShapeData::Shape::Sphere:
            ImGui::DragFloat("Radius", &s.sphere_radius, 0.01f, 0.01f, 100.0f);
            break;
          case ColliderShapeData::Shape::Capsule:
            ImGui::DragFloat("Radius", &s.capsule_radius, 0.01f, 0.01f, 100.0f);
            ImGui::DragFloat("Half Height", &s.capsule_half_height, 0.01f, 0.01f, 100.0f);
            break;
          case ColliderShapeData::Shape::Cylinder:
            ImGui::DragFloat("Radius", &s.cylinder_radius, 0.01f, 0.01f, 100.0f);
            ImGui::DragFloat("Half Height", &s.cylinder_half_height, 0.01f, 0.01f, 100.0f);
            break;
          case ColliderShapeData::Shape::Box:
          default:
            DrawVec3Control("Half Extents", s.box_half_extents, 1.0f);
            break;
        }
        DrawVec3Control("Offset", s.offset, 1.0f);
      };

      for (size_t i = 0; i < group.shapes.size();) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::Text("Shape %zu", i + 1);
        draw_shape(group.shapes[i]);
        if (ImGui::Button("Remove")) {
          group.shapes.erase(group.shapes.begin() + static_cast<ptrdiff_t>(i));
          ImGui::PopID();
          ImGui::Separator();
          continue;  // don't advance i
        }
        ImGui::PopID();
        ImGui::Separator();
        ++i;
      }
      if (ImGui::Button("Add Shape")) {
        group.shapes.push_back(ColliderShapeData{});
      }
      ImGui::TextDisabled("Extra shapes are merged with the primary collider into one body.");
    });

    DrawComponent<CameraController>("Camera Controller", selected_entity_, [](auto &component) {
      ImGui::DragFloat("Move Speed", &component.move_speed, 0.1f, 0.0f, 100.0f);
      ImGui::DragFloat("Look Sensitivity", &component.look_sensitivity, 0.01f, 0.0f, 2.0f);
    });

    DrawComponent<LuaScriptComponent>("Lua Script", selected_entity_, [&](auto &component) {
      char buffer[512] = {};
      std::snprintf(buffer, sizeof(buffer), "%s", component.path.c_str());
      if (ImGui::InputText("Path", buffer, sizeof(buffer))) {
        component.path = std::string(buffer);
      }
      ImGui::TextDisabled("Relative to the asset root, e.g. scripts/enemy.lua");

      // Quick picker listing the scripts in assets/scripts/.
      const char *preview = component.path.empty() ? "<select a script>" : component.path.c_str();
      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
      if (ImGui::BeginCombo("##LuaScriptPicker", preview)) {
        for (const std::string &relative : ListLuaScriptPaths()) {
          if (ImGui::Selectable(relative.c_str(), relative == component.path)) {
            component.path = relative;
          }
        }
        ImGui::EndCombo();
      }

      // Drop a .lua from the Content Browser here to assign it.
      ImGui::InvisibleButton("##LuaScriptDropZone", ImVec2(ImGui::GetContentRegionAvail().x, 20.0f));
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
          const std::filesystem::path file_path(static_cast<const wchar_t *>(payload->Data));
          if (file_path.extension() == ".lua") {
            component.path = ToAssetRelativePath(file_path);
          }
        }
        ImGui::EndDragDropTarget();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Drop a .lua file from the Content Browser to assign it");
      }

      if (!component.path.empty()) {
        if (ImGui::Button("Open in Script Editor")) {
          OpenScriptInEditor(component.path);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload")) {
          active_scene_->GetScriptEngine().ReloadScript(component.path);
        }
      }
    });
  } else {
    ImGui::TextDisabled("Select an entity in the Scene panel to edit its properties.");
  }

  ImGui::End();
}

void Editor::ShowImGuiLighting() {
  PROFILER_FUNCTION();
  ImGui::Begin("Lighting");

  DirectionalLight &dir_light = active_scene_->GetLight();
  ImGui::Text("Directional Light");
  DrawVec3Control("Direction", dir_light.direction);
  ImGui::ColorEdit3("Color", glm::value_ptr(dir_light.color));

  ImGui::Separator();
  ImGui::Text("Point Lights");
  if (ImGui::Button("Add Point Light")) {
    PointLight light;
    light.position     = glm::vec3(0.0f, 2.0f, 0.0f);
    light.color        = glm::vec3(1.0f);
    light.intensity    = 2.0f;
    light.radius       = 6.0f;
    light.casts_shadow = false;
    point_lights_.push_back(light);
  }

  for (size_t i = 0; i < point_lights_.size(); ++i) {
    PointLight       &light = point_lights_[i];
    const std::string label = "Point Light " + std::to_string(i);
    if (ImGui::CollapsingHeader(label.c_str())) {
      DrawVec3Control("Position", light.position);
      ImGui::ColorEdit3("Color", glm::value_ptr(light.color));
      ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 100.0f);
      ImGui::DragFloat("Radius", &light.radius, 0.1f, 0.1f, 100.0f);
      ImGui::Checkbox("Cast Shadow", &light.casts_shadow);
      if (ImGui::Button("Remove")) {
        point_lights_.erase(point_lights_.begin() + static_cast<std::ptrdiff_t>(i));
        --i;
      }
    }
  }

  ImGui::End();
}

void Editor::ShowImGuiRendering() {
  PROFILER_FUNCTION();
  ImGui::Begin("Rendering");

  int         render_mode = static_cast<int>(active_scene_->GetRenderMode());
  const char *items[]     = {"Lit", "Unlit", "Wireframe"};
  if (ImGui::Combo("Render Mode", &render_mode, items, 3)) {
    active_scene_->SetRenderMode(static_cast<RenderMode>(render_mode));
  }

  ImGui::Checkbox("Show Colliders", &show_colliders_);

  bool bloom = active_scene_->IsBloomEnabled();
  if (ImGui::Checkbox("HDR (Bloom)", &bloom)) {
    active_scene_->SetBloomEnabled(bloom);
  }

  float exposure = active_scene_->GetExposure();
  if (ImGui::SliderFloat("Exposure", &exposure, 0.0f, 5.0f)) {
    active_scene_->SetExposure(exposure);
  }

  float bloom_strength = active_scene_->GetBloomStrength();
  if (ImGui::SliderFloat("Bloom Strength", &bloom_strength, 0.0f, 1.0f)) {
    active_scene_->SetBloomStrength(bloom_strength);
  }

  float bloom_threshold = active_scene_->GetBloomThreshold();
  if (ImGui::SliderFloat("Bloom Threshold", &bloom_threshold, 0.0f, 5.0f)) {
    active_scene_->SetBloomThreshold(bloom_threshold);
  }

  float god_rays = active_scene_->GetGodRaysStrength();
  if (ImGui::SliderFloat("God Rays", &god_rays, 0.0f, 1.0f)) {
    active_scene_->SetGodRaysStrength(god_rays);
  }

  ImGui::Separator();

  bool ssao = active_scene_->IsSSAOEnabled();
  if (ImGui::Checkbox("SSAO", &ssao)) {
    active_scene_->SetSSAOEnabled(ssao);
  }

  bool taa = active_scene_->IsTAAEnabled();
  if (ImGui::Checkbox("TAA", &taa)) {
    active_scene_->SetTAAEnabled(taa);
  }

  ImGui::Separator();

  float ibl = active_scene_->GetIblIntensity();
  if (ImGui::SliderFloat("IBL Intensity", &ibl, 0.0f, 2.0f)) {
    active_scene_->SetIblIntensity(ibl);
  }

  float pcf = active_scene_->GetShadowPcfRadius();
  if (ImGui::SliderFloat("Shadow PCF Radius", &pcf, 0.0f, 8.0f)) {
    active_scene_->SetShadowPcfRadius(pcf);
  }

  ImGui::End();
}

void Editor::ShowImGuiLog() {
  PROFILER_FUNCTION();

  // State persists across frames.
  static char                     search[128] = {};
  static bool                     auto_scroll = true;
  static bool                     show_trace  = false;
  static bool                     show_debug  = false;
  static bool                     show_info   = true;
  static bool                     show_warn   = true;
  static bool                     show_error  = true;
  static bool                     show_fatal  = true;
  static std::vector<std::string> lines;
  static std::uintmax_t           cached_size = 0;

  ImGui::Begin("Log");

  // Toolbar: Clear | Auto-scroll | Search.
  if (ImGui::Button("Clear")) {
    std::ofstream(std::string(kLogFileName), std::ios::trunc).close();
    lines.clear();
    cached_size = 0;
  }
  ImGui::SameLine();
  ImGui::Checkbox("Auto-scroll", &auto_scroll);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(220.0f);
  ImGui::InputTextWithHint("##LogSearch", "Search...", search, sizeof(search));

  // Level filter toggles (colored buttons).
  struct LevelToggle {
    const char *name;
    bool       *flag;
    ImVec4      color;
  };
  static const LevelToggle toggles[] = {
      {"TRACE", &show_trace, {0.76f, 0.77f, 0.80f, 1.0f}}, {"DEBUG", &show_debug, {0.72f, 0.83f, 0.96f, 1.0f}},
      {"INFO", &show_info, {0.80f, 0.88f, 0.96f, 1.0f}},   {"WARN", &show_warn, {0.98f, 0.90f, 0.66f, 1.0f}},
      {"ERROR", &show_error, {0.98f, 0.78f, 0.78f, 1.0f}}, {"FATAL", &show_fatal, {0.94f, 0.66f, 0.66f, 1.0f}},
  };
  for (const auto &toggle : toggles) {
    const ImVec4 c = *toggle.flag ? toggle.color : ImVec4(0.88f, 0.89f, 0.92f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, c);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, c);
    if (ImGui::SmallButton(toggle.name)) {
      *toggle.flag = !*toggle.flag;
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
  }
  ImGui::NewLine();

  ImGui::Separator();

  // Reload the file only when its size changed (or after Clear).
  const std::string    log_path(kLogFileName);
  const std::uintmax_t size = std::filesystem::exists(log_path) ? std::filesystem::file_size(log_path) : 0;
  if (size != cached_size) {
    cached_size = size;
    lines.clear();
    std::ifstream file(log_path);
    std::string   line;
    while (std::getline(file, line)) {
      lines.push_back(line);
    }
  }

  const bool auto_scroll_this_frame = auto_scroll;
  if (mono_font_) ImGui::PushFont(mono_font_);
  ImGui::BeginChild("##LogText", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
  const bool at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();

  for (const std::string &line : lines) {
    const LogLevel level    = ParseLogLevel(line);
    const bool     level_ok = (level == LogLevel::Trace && show_trace) || (level == LogLevel::Debug && show_debug) ||
                              (level == LogLevel::Info && show_info) || (level == LogLevel::Warn && show_warn) ||
                              (level == LogLevel::Error && show_error) || (level == LogLevel::Fatal && show_fatal) ||
                              (level == LogLevel::Unknown);
    if (!level_ok) {
      continue;
    }
    if (search[0] != '\0' && !ContainsIgnoreCase(line, search)) {
      continue;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, LogLevelColor(level));
    ImGui::TextUnformatted(line.c_str());
    ImGui::PopStyleColor();
  }

  if (auto_scroll_this_frame && at_bottom) {
    ImGui::SetScrollHereY(1.0f);
  }

  ImGui::EndChild();
  if (mono_font_) ImGui::PopFont();
  ImGui::End();
}

void Editor::ShowImGuiInformation() {
  PROFILER_FUNCTION();
  ImGui::Begin("Information");
  ImGui::Text("FPS: %d", GetFPS());

  // Live render statistics (draw calls / triangles / culling / pass times).
  ImGui::Separator();
  ImGui::TextUnformatted("Render Stats");
  if (active_scene_) {
    const auto &stats = active_scene_->GetRenderStats();
    ImGui::Text("Draw calls:   %llu", static_cast<unsigned long long>(stats.draw_calls));
    ImGui::Text("Instanced:    %llu", static_cast<unsigned long long>(stats.instanced_draws));
    ImGui::Text("Triangles:    %llu", static_cast<unsigned long long>(stats.triangles));
    ImGui::Text("Culled:       %llu", static_cast<unsigned long long>(stats.culled_entities));
    float pass_ms[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    active_scene_->GetLastPassTimes(pass_ms);
    ImGui::Text("Shadow/Point/SSAO/Main/Post: %.2f / %.2f / %.2f / %.2f / %.2f ms", pass_ms[0], pass_ms[1],
                pass_ms[2], pass_ms[3], pass_ms[5]);
  }
  ImGui::Separator();

  ImGui::Text("Editor Camera");
  DrawVec3Control("Target", editor_camera_.target);
  ImGui::DragFloat("Yaw", &editor_camera_.yaw, 0.5f);
  ImGui::DragFloat("Pitch", &editor_camera_.pitch, 0.5f, -89.0f, 89.0f);
  ImGui::DragFloat("Distance", &editor_camera_.distance, 0.1f, 0.1f, 10000.0f);
  ImGui::DragFloat("FOV", &editor_camera_.fov, 0.5f, 1.0f, 179.0f);
  if (ImGui::Button("Reset Camera")) {
    editor_camera_.Reset();
  }
  ImGui::End();
}

void Editor::ShowImGuiContentBrowser() {
  PROFILER_FUNCTION();
  ImGui::Begin("Content Browser");

  // ---- Toolbar: back, current path, search, thumbnail size ----
  const bool at_root = current_directory_ == std::filesystem::path(base_directory_);
  if (at_root) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("<-")) {
    current_directory_ = current_directory_.parent_path();
  }
  if (at_root) {
    ImGui::EndDisabled();
  }
  ImGui::SameLine();

  std::string rel = std::filesystem::relative(current_directory_, base_directory_).string();
  if (rel.empty()) {
    rel = ".";
  }
  ImGui::TextDisabled("%s", ("assets/" + rel).c_str());
  ImGui::SameLine();

  static char search[256] = {};
  ImGui::SetNextItemWidth(200.0f);
  ImGui::InputTextWithHint("##ContentBrowserSearch", "Search...", search, sizeof(search));
  ImGui::SameLine();

  static float thumbnailSize = 96.0f;
  ImGui::SetNextItemWidth(140.0f);
  ImGui::SliderFloat("Size", &thumbnailSize, 32.0f, 256.0f, "%.0f");
  ImGui::Separator();

  // ---- Grid of assets ----
  constexpr float kPad    = 12.0f;
  const float     cell_w  = thumbnailSize + kPad;
  const float     label_h = ImGui::GetTextLineHeightWithSpacing() * 2.0f + 4.0f;
  const float     cell_h  = thumbnailSize + label_h + kPad;
  const float     hit_h   = thumbnailSize + label_h;

  ImGui::BeginChild("##ContentBrowserGrid", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
  const float avail_x = ImGui::GetContentRegionAvail().x;
  const int   cols    = std::max(1, static_cast<int>(avail_x / cell_w));

  ImDrawList      *draw = ImGui::GetWindowDrawList();
  constexpr ImVec2 grid_origin(kPad, kPad);  // window-local grid origin

  int index = 0;
  for (const auto &entry : std::filesystem::directory_iterator(current_directory_)) {
    const auto       &path   = entry.path();
    const std::string name   = path.filename().string();
    const bool        is_dir = entry.is_directory();

    if (search[0] != '\0' && !ContainsIgnoreCase(name, search)) {
      continue;
    }

    const int col = index % cols;
    const int row = index / cols;

    ImGui::PushID(name.c_str());

    // Hit target covering the thumbnail + label. Window-local coords so the
    // child window's scrolling is handled by ImGui automatically.
    ImGui::SetCursorPos(ImVec2(grid_origin.x + col * cell_w, grid_origin.y + row * cell_h));
    ImGui::InvisibleButton("##Cell", ImVec2(thumbnailSize, hit_h));
    const bool   hovered   = ImGui::IsItemHovered();
    const ImVec2 thumb_min = ImGui::GetItemRectMin();  // screen, scroll-adjusted
    const ImVec2 thumb_max(thumb_min.x + thumbnailSize, thumb_min.y + thumbnailSize);

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
      std::filesystem::path abs_path(path);
      const wchar_t        *item_path = abs_path.c_str();
      ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", item_path, (wcslen(item_path) + 1) * sizeof(wchar_t));
      ImGui::EndDragDropSource();
    }

    // Hover highlight behind the thumbnail.
    if (hovered) {
      draw->AddRectFilled(thumb_min, thumb_max, ImGui::GetColorU32(ImVec4(0.55f, 0.66f, 0.86f, 0.45f)));
    }

    // Thumbnail: folder icon / image preview / file icon.
    if (is_dir) {
      DrawFolderIcon(draw, thumb_min, thumb_max);
    } else if (IsImageFile(path)) {
      const std::string key = path.string();
      auto              it  = thumbnail_cache_.find(key);
      if (it == thumbnail_cache_.end()) {
        std::shared_ptr<Texture> thumb = Texture::Create(key);
        if (thumb) {
          it = thumbnail_cache_.emplace(key, thumb).first;
        }
      }
      const bool have_thumb = it != thumbnail_cache_.end() && it->second && it->second->GetID() != 0;
      if (have_thumb) {
        draw->AddImage(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(it->second->GetID())), thumb_min, thumb_max,
                       ImVec2(0, 1), ImVec2(1, 0));
      } else if (file_icon_ && file_icon_->GetID() != 0) {
        draw->AddImage(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(file_icon_->GetID())), thumb_min, thumb_max,
                       ImVec2(0, 1), ImVec2(1, 0));
      }
    } else if (file_icon_ && file_icon_->GetID() != 0) {
      draw->AddImage(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(file_icon_->GetID())), thumb_min, thumb_max,
                     ImVec2(0, 1), ImVec2(1, 0));
    }

    // Label centered under the thumbnail (window-local coordinates).
    const ImVec2 text_size = ImGui::CalcTextSize(name.c_str(), nullptr, false, thumbnailSize - 4.0f);
    const float  label_x   = grid_origin.x + col * cell_w + std::max(0.0f, (thumbnailSize - text_size.x)) * 0.5f;
    const float  label_y   = grid_origin.y + row * cell_h + thumbnailSize + 4.0f;
    ImGui::SetCursorPos(ImVec2(label_x, label_y));
    ImGui::PushTextWrapPos(grid_origin.x + col * cell_w + thumbnailSize);
    ImGui::TextUnformatted(name.c_str());
    ImGui::PopTextWrapPos();

    // Double-click a folder to enter it.
    if (is_dir && hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      current_directory_ /= path.filename();
      ImGui::PopID();
      break;
    }

    // Double-click a .lua script to open it in the Script Editor.
    if (!is_dir && path.extension() == ".lua" && hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      OpenScriptInEditor(ToAssetRelativePath(path));
    }

    ImGui::PopID();
    ++index;
  }

  ImGui::EndChild();
  ImGui::End();
}

void Editor::LoadScriptIntoBuffer(const std::string &relative) {
  std::memset(script_code_, 0, sizeof(script_code_));
  std::ifstream in(AssetManager::Instance().Resolve(relative));
  if (in) {
    in.read(script_code_, sizeof(script_code_) - 1);
  }
  current_script_path_ = relative;
  script_dirty_        = false;
}

bool Editor::SaveCurrentScript() {
  if (current_script_path_.empty()) return false;
  if (!script_dirty_) return true;  // nothing to save (avoids pointless reloads)

  std::ofstream out(AssetManager::Instance().Resolve(current_script_path_));
  if (!out) {
    LOG_ERROR("Editor") << "Failed to open script for writing: " << current_script_path_;
    return false;
  }
  out << script_code_;
  script_dirty_ = false;
  LOG_INFO("Editor") << "Saved script " << current_script_path_;

  // Hot-reload running instances (re-runs OnStart) when in Play mode.
  active_scene_->GetScriptEngine().ReloadScript(current_script_path_);
  return true;
}

void Editor::OpenScriptInEditor(const std::string &relative) {
  if (relative.empty()) return;
  show_script_editor_ = true;
  if (relative == current_script_path_) return;  // already open (dirty or not)
  if (script_dirty_ && script_pending_path_.empty()) {
    script_pending_path_   = relative;
    script_pending_create_ = false;
    ImGui::OpenPopup("ScriptEditorUnsavedChanges");
    return;
  }
  if (!script_dirty_) {
    LoadScriptIntoBuffer(relative);
  }
}

void Editor::ApplyScriptPending() {
  if (script_pending_path_.empty()) return;
  if (script_pending_create_) {
    const std::filesystem::path resolved = AssetManager::Instance().Resolve(script_pending_path_);
    const std::filesystem::path dir      = resolved.parent_path();
    if (!std::filesystem::exists(dir)) {
      std::filesystem::create_directories(dir);
    }
    if (!std::filesystem::exists(resolved)) {
      std::ofstream out(resolved);
      if (out) {
        out << kLuaScriptTemplate;
        LOG_INFO("Editor") << "Created script " << script_pending_path_;
      } else {
        LOG_ERROR("Editor") << "Failed to create script " << script_pending_path_;
      }
    }
  }
  LoadScriptIntoBuffer(script_pending_path_);
  script_pending_path_.clear();
  script_pending_create_ = false;
}

void Editor::ShowImGuiScriptEditor() {
  PROFILER_FUNCTION();

  const bool dirty = script_dirty_ && !current_script_path_.empty();
  ImGui::Begin("Script Editor");

  // Ctrl+S saves the open script while this window (or its text field) has focus.
  const ImGuiIO &io = ImGui::GetIO();
  if (dirty && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) &&
      ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
    SaveCurrentScript();
  }

  // ---- Left: script list ----
  ImGui::BeginChild("##ScriptList", ImVec2(210.0f, 0.0f), true);
  if (ImGui::Button("New Script", ImVec2(-1.0f, 0.0f))) {
    ImGui::OpenPopup("NewScriptName");
  }
  if (ImGui::BeginPopup("NewScriptName")) {
    static char name[128] = "new_script.lua";
    ImGui::Text("Name (optional .lua extension)");
    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputText("##NewScriptNameField", name, sizeof(name));
    ImGui::TextDisabled("Created in assets/scripts/");
    const bool create_pressed = ImGui::Button("Create", ImVec2(120.0f, 0.0f));
    if (create_pressed) {
      std::string file_name = name;
      // Trim surrounding whitespace.
      const auto not_space = [](unsigned char c) { return !std::isspace(c); };
      const auto first     = std::find_if(file_name.begin(), file_name.end(), not_space);
      const auto last      = std::find_if(file_name.rbegin(), file_name.rend(), not_space).base();
      file_name            = (first < last) ? std::string(first, last) : std::string();
      if (file_name.find_first_of("/\\") != std::string::npos) {
        LOG_WARN("Editor") << "Script name must not contain path separators";
      } else if (!file_name.empty()) {
        if (file_name.rfind(".lua") == std::string::npos) file_name += ".lua";
        script_pending_path_   = "scripts/" + file_name;
        script_pending_create_ = true;
        if (dirty) {
          ImGui::OpenPopup("ScriptEditorUnsavedChanges");
        } else {
          ApplyScriptPending();
        }
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  ImGui::Separator();
  ImGui::TextDisabled("Scripts (assets/scripts/)");
  ImGui::Separator();

  const std::vector<std::string> scripts = ListLuaScriptPaths();
  for (const std::string &relative : scripts) {
    const std::string file_name = std::filesystem::path(relative).filename().string();
    const bool        is_open   = relative == current_script_path_;
    char              label[512];
    std::snprintf(label, sizeof(label), "%s%s", file_name.c_str(), (is_open && dirty) ? " *" : "");
    if (ImGui::Selectable(label, is_open)) {
      OpenScriptInEditor(relative);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", relative.c_str());
    }
  }
  if (scripts.empty()) {
    ImGui::TextDisabled("No .lua scripts yet.");
  }
  ImGui::EndChild();

  ImGui::SameLine();

  // ---- Right: code editor + actions ----
  ImGui::BeginChild("##ScriptEditorBody", ImVec2(0.0f, 0.0f), true);
  if (current_script_path_.empty()) {
    ImGui::TextWrapped("Select or create a Lua script on the left, or double-click a .lua file in the Content Browser.");
  } else {
    // Header row: path, dirty badge, Save / Revert / Attach.
    ImGui::TextUnformatted(current_script_path_.c_str());
    if (dirty) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.85f, 0.45f, 0.05f, 1.0f), "* unsaved");
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
      SaveCurrentScript();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Save and hot-reload in the running scene (Ctrl+S)");
    }
    if (dirty) {
      ImGui::SameLine();
      if (ImGui::Button("Revert")) {
        LoadScriptIntoBuffer(current_script_path_);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Discard edits and reload from disk");
      }
    }

    const bool can_attach = selected_entity_.GetHandle() != entt::null;
    if (can_attach) {
      ImGui::SameLine();
      if (ImGui::Button("Attach to Selected")) {
        if (!selected_entity_.HasComponent<LuaScriptComponent>()) {
          selected_entity_.AddComponent<LuaScriptComponent>(current_script_path_);
        } else {
          selected_entity_.GetComponent<LuaScriptComponent>().path = current_script_path_;
        }
        LOG_INFO("Editor") << "Attached script " << current_script_path_ << " to entity '"
                           << selected_entity_.GetComponent<Tag>().tag << "'";
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add/update the Lua Script component on the entity selected in the Scene panel");
      }
    }
    ImGui::Separator();

    if (mono_font_) {
      ImGui::PushFont(mono_font_);
    }
    const bool edited = ImGui::InputTextMultiline("##ScriptCode", script_code_, kScriptBufferSize, ImVec2(-1.0f, -1.0f),
                                                  ImGuiInputTextFlags_AllowTabInput);
    if (edited) {
      script_dirty_ = true;
    }
    if (mono_font_) {
      ImGui::PopFont();
    }
  }
  ImGui::EndChild();

  // ---- Unsaved-changes prompt (opened from list / New / Content Browser) ----
  if (ImGui::BeginPopupModal("ScriptEditorUnsavedChanges", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("Save changes to '%s'?", current_script_path_.c_str());
    ImGui::Separator();
    const ImVec2 btn_size(150.0f, 0.0f);
    bool         resolved = false;
    if (ImGui::Button("Save", btn_size)) {
      resolved = SaveCurrentScript();
    }
    ImGui::SameLine();
    if (ImGui::Button("Don't Save", btn_size)) {
      resolved = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", btn_size)) {
      script_pending_path_.clear();
      script_pending_create_ = false;
      ImGui::CloseCurrentPopup();
    }
    if (resolved) {
      ApplyScriptPending();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::End();
}

Entity Editor::CreateEntityWithUniqueName(const std::string &base_name) {
  const auto name_taken = [&](const std::string &candidate) {
    for (auto &entity : active_scene_->GetAllEntities()) {
      if (entity.HasComponent<Tag>() && entity.GetComponent<Tag>().tag == candidate) {
        return true;
      }
    }
    return false;
  };

  std::string name   = base_name;
  int         suffix = 1;
  while (name_taken(name)) {
    name = base_name + " (" + std::to_string(suffix++) + ")";
  }
  return active_scene_->CreateEntity(name);
}

void Editor::CreatePrimitive(const std::string &name, const Ref<Mesh> &mesh) {
  Entity entity = CreateEntityWithUniqueName(name);
  entity.AddComponent<Transform>();

  if (mesh) {
    entity.AddComponent<MeshComponent>(mesh, CreateRef<Material>(*default_material_));
    // Rest solid primitives on the ground grid.
    entity.GetComponent<Transform>().translation.y = 0.5f;
  }

  selected_entity_ = entity;
  LOG_DEBUG("Editor") << "Created primitive '" << entity.GetComponent<Tag>().tag << "'";
}

void Editor::CreateCameraEntity() {
  Entity entity = CreateEntityWithUniqueName("Camera");

  CameraComponent component;
  component.camera.position = editor_camera_.GetPosition();
  component.camera.LookAt(editor_camera_.target);
  entity.AddComponent<CameraComponent>(component);

  selected_entity_ = entity;
  LOG_DEBUG("Editor") << "Created camera '" << entity.GetComponent<Tag>().tag << "'";
}

void Editor::CreatePhysicsDemo() {
  // Large static floor platform. Colliders are world-space (the transform's
  // scale is intentionally ignored), so the half extents match the visible box.
  Entity ground = active_scene_->CreateEntity("Ground");
  ground.AddComponent<Transform>(glm::vec3(0.0f, -0.5f, 0.0f));
  ground.GetComponent<Transform>().scale = glm::vec3(20.0f, 1.0f, 20.0f);
  ground.AddComponent<MeshComponent>(Mesh::CreateCube(), CreateRef<Material>(*default_material_));
  ground.AddComponent<RigidBodyComponent>(RigidBodyComponent::Type::Static);
  {
    ColliderComponent collider;
    collider.shape            = ColliderComponent::Shape::Box;
    collider.box_half_extents = glm::vec3(10.0f, 0.5f, 10.0f);
    ground.AddComponent<ColliderComponent>(collider);
  }

  // A free-fly player camera for Play mode: WASD/QE move, right-drag looks.
  Entity camera = active_scene_->CreateEntity("Player Camera");
  CameraComponent camera_component;
  camera_component.camera.position = glm::vec3(11.0f, 7.0f, 11.0f);
  camera_component.camera.LookAt(glm::vec3(0.0f, 1.5f, 0.0f));
  camera_component.primary = true;
  camera.AddComponent<CameraComponent>(camera_component);
  camera.AddComponent<CameraController>();

  // A script-driven cube (no physics) off to the side, just to keep the plain
  // LuaScriptComponent demo around.
  Entity spinner = active_scene_->CreateEntity("Spinner");
  spinner.AddComponent<Transform>(glm::vec3(4.0f, 3.0f, 4.5f));
  spinner.AddComponent<MeshComponent>(Mesh::CreateCube(), CreateRef<Material>(*default_material_));
  spinner.AddComponent<LuaScriptComponent>("scripts/spin.lua");

  // ---- Collision playground (press Play) --------------------------------
  // main.lua periodically fires "Ball"s (+X, along z = 0) from the left.
  // Static brick targets sit in the lane: target.lua turns them red as they
  // take hits and destroys them at 0 HP. The Bouncer (far side, z = -4) hops
  // on its own and reacts to Space via OnFixedUpdate.

  const struct {
    float     x;
    glm::vec3 color;
  } kTargets[] = {
      {-3.0f, {0.25f, 0.55f, 0.95f}},
      {0.0f, {0.25f, 0.78f, 0.42f}},
      {3.0f, {0.95f, 0.62f, 0.18f}},
  };
  for (const auto &t : kTargets) {
    Entity target = active_scene_->CreateEntity("Target");
    target.AddComponent<Transform>(glm::vec3(t.x, 0.5f, 0.0f));
    auto material      = CreateRef<Material>(*default_material_);
    material->SetBaseColorFactor(glm::vec4(t.color, 1.0f));
    target.AddComponent<MeshComponent>(Mesh::CreateCube(), material);
    target.AddComponent<RigidBodyComponent>(RigidBodyComponent::Type::Static);
    {
      ColliderComponent collider;
      collider.shape            = ColliderComponent::Shape::Box;
      collider.box_half_extents = glm::vec3(0.5f);
      target.AddComponent<ColliderComponent>(collider);
    }
    target.AddComponent<LuaScriptComponent>("scripts/target.lua");
  }

  // A bouncy dynamic sphere driven entirely by Lua (impulse + collision hooks).
  Entity bouncer = active_scene_->CreateEntity("Bouncer");
  bouncer.AddComponent<Transform>(glm::vec3(0.0f, 3.0f, -4.0f));
  bouncer.AddComponent<MeshComponent>(Mesh::CreateSphere(), CreateRef<Material>(*default_material_));
  {
    RigidBodyComponent &rigid = bouncer.AddComponent<RigidBodyComponent>();
    rigid.friction            = 0.2f;
    rigid.restitution         = 0.75f;
  }
  {
    ColliderComponent collider;
    collider.shape         = ColliderComponent::Shape::Sphere;
    collider.sphere_radius = 0.5f;
    bouncer.AddComponent<ColliderComponent>(collider);
  }
  bouncer.AddComponent<LuaScriptComponent>("scripts/bounce.lua");

  LOG_INFO("Editor") << "Collision playground created (ground + targets + bouncer + camera)";
}

void Editor::SetGridVisible(bool visible) {
  if (grid_entity_.GetHandle() == entt::null) {
    return;
  }

  const bool has_mesh = grid_entity_.HasComponent<MeshComponent>();
  if (visible && !has_mesh) {
    grid_entity_.AddComponent<MeshComponent>(grid_mesh_, grid_material_);
  } else if (!visible && has_mesh) {
    grid_entity_.RemoveComponent<MeshComponent>();
  }
}

void Editor::LaunchStandalone() {
  // Save the scene into the shared build-tree root, next to the sandbox
  // executable, then run the sandbox in a new process (new window).
  std::error_code            ec;
  const std::filesystem::path build_root = std::filesystem::absolute("..", ec);
  if (ec) {
    LOG_ERROR("Editor") << "Failed to resolve build directory for standalone launch";
    return;
  }

  // Return to the resting Edit state first so the saved scene has the initial
  // transforms rather than mid-simulation poses.
  if (game_mode_ == GameMode::Play) {
    game_mode_ = GameMode::Edit;
    active_scene_->GetScriptEngine().Clear();
    active_scene_->StopSimulation();
    SetGridVisible(true);
    if (selected_entity_.GetHandle() != entt::null &&
        !active_scene_->GetRegistry().valid(selected_entity_.GetHandle())) {
      selected_entity_ = Entity();
    }
  }

  const std::filesystem::path scene_path  = build_root / "play_scene.json";
  const std::filesystem::path sandbox_exe = build_root / "sandbox" / "sandbox.exe";

  active_scene_->SaveScene(scene_path.string());

  const std::string command = "\"" + sandbox_exe.string() + "\" --scene \"" + scene_path.string() + "\"";
  // `cmd /c` strips the outermost quotes; wrap the whole command so the inner
  // quotes around each path survive shell parsing.
  const std::string cmd = "\"" + command + "\"";
  LOG_INFO("Editor") << "Launching standalone sandbox: " << command;

  std::thread([cmd]() { std::system(cmd.c_str()); }).detach();
}

void Editor::ExitGameModeForFileOp() {
  // Mirror the Play-mode Stop handler: fire OnDestroy hooks while entities are
  // still alive, then restore the authoring scene. No-op in Edit mode.
  if (game_mode_ != GameMode::Play) {
    return;
  }
  game_mode_ = GameMode::Edit;
  active_scene_->GetScriptEngine().Clear();
  active_scene_->StopSimulation();
  SetGridVisible(true);
  if (selected_entity_.GetHandle() != entt::null &&
      !active_scene_->GetRegistry().valid(selected_entity_.GetHandle())) {
    selected_entity_ = Entity();
  }
}

void Editor::NewScene() {
  ExitGameModeForFileOp();
  active_scene_->ClearContent();
  current_scene_path_.clear();
  selected_entity_ = Entity();
  LOG_INFO("Editor") << "Started a new (empty) scene";
}

void Editor::OpenSceneDialog() {
  std::string path;
  if (!NativeOpenFileDialog(path)) {
    return;  // cancelled
  }
  OpenScenePath(path);
}

void Editor::OpenScenePath(const std::string &path) {
  ExitGameModeForFileOp();
  if (!active_scene_->OpenSceneFile(path)) {
    LOG_ERROR("Editor") << "Failed to open scene: " << path;
    return;
  }
  current_scene_path_ = path;
  selected_entity_    = Entity();
  SetGridVisible(true);
  LOG_INFO("Editor") << "Opened scene: " << path;
}

void Editor::SaveCurrentScene() {
  if (current_scene_path_.empty()) {
    SaveSceneAsDialog();
    return;
  }
  ExitGameModeForFileOp();
  active_scene_->SaveScene(current_scene_path_);
  LOG_INFO("Editor") << "Saved scene: " << current_scene_path_;
}

void Editor::SaveSceneAsDialog() {
  std::string path;
  if (!NativeSaveFileDialog(path)) {
    return;  // cancelled
  }
  static const char *kExt = ".scene";
  if (path.size() < 6 || path.compare(path.size() - 6, 6, kExt) != 0) {
    path += kExt;
  }
  ExitGameModeForFileOp();
  active_scene_->SaveScene(path);
  current_scene_path_ = path;
  LOG_INFO("Editor") << "Saved scene as: " << path;
}

void Editor::RunSceneFileSelftest(const std::string &path) {
  // Count content entities (skip the editor-only grid helper).
  const auto content_count = [&]() {
    size_t n = 0;
    for (auto &entity : active_scene_->GetAllEntities()) {
      if (entity == grid_entity_) continue;
      const auto *tag = entity.HasComponent<Tag>() ? &entity.GetComponent<Tag>() : nullptr;
      if (tag && tag->editor_only) continue;
      ++n;
    }
    return n;
  };
  const auto has_grid = [&]() {
    return grid_entity_.GetHandle() != entt::null &&
           active_scene_->GetRegistry().valid(grid_entity_.GetHandle());
  };

  bool ok = true;
  const auto check = [&](bool cond, const char *what) {
    ok = ok && cond;
    LOG_INFO("Editor") << (cond ? "[selftest] PASS  " : "[selftest] FAIL  ") << what;
  };

  LOG_INFO("Editor") << "[selftest] scene-file ops begin (open: " << path << ")";

  const size_t base_content = content_count();

  // 1. New Scene must clear all content but keep the grid.
  NewScene();
  check(content_count() == 0, "NewScene clears content");
  check(has_grid(), "NewScene keeps the editor grid");

  // 2. Open Scene must load content (and remember the path).
  OpenScenePath(path);
  check(!current_scene_path_.empty() && current_scene_path_ == path, "OpenScenePath records the scene path");
  const size_t opened_content = content_count();
  check(opened_content > 0, "OpenSceneFile loaded content entities");

  // 3. Save + reopen round-trip must reproduce the same content count.
  const std::string tmp_path = "editor_selftest_tmp.scene";
  ExitGameModeForFileOp();
  active_scene_->SaveScene(tmp_path);
  check(std::filesystem::exists(tmp_path), "SaveScene wrote a .scene file");
  NewScene();
  OpenScenePath(tmp_path);
  check(content_count() == opened_content, "Save/reopen round-trip preserves content count");
  std::error_code ec;
  std::filesystem::remove(tmp_path, ec);

  // 4. Restore the default demo scene so interactive use still has content.
  NewScene();
  CreatePhysicsDemo();
  active_scene_->SetMainScript("scripts/main.lua");
  current_scene_path_.clear();

  LOG_INFO("Editor") << "[selftest] scene-file ops " << (ok ? "PASSED" : "FAILED") << " (base=" << base_content
                     << " opened=" << opened_content << ")";
}

void Editor::CreateModelEntity(const std::filesystem::path &path) {
  LOG_INFO("Editor") << "Importing model: " << path.filename().string();

  Ref<Mesh>     mesh;
  Ref<Material> material;
  if (!LoadModelAsset(path, mesh, material)) {
    LOG_WARN("Editor") << "Failed to load model: " << path;
    return;
  }

  Entity entity    = CreateEntityWithUniqueName(path.stem().string());
  auto  &transform = entity.AddComponent<Transform>();
  entity.AddComponent<MeshComponent>(mesh, material);

  // Auto-fit: scale so the longest axis is ~2 units and rest the bottom of the
  // model's bounds on the ground grid.
  glm::vec3 min_v(std::numeric_limits<float>::max());
  glm::vec3 max_v(std::numeric_limits<float>::lowest());
  for (const auto &v : mesh->GetVertices()) {
    min_v = glm::min(min_v, v.position);
    max_v = glm::max(max_v, v.position);
  }
  const glm::vec3 extent     = max_v - min_v;
  const float     max_extent = std::max({extent.x, extent.y, extent.z});
  if (max_extent > 0.0001f) {
    const float fit_scale   = 2.0f / max_extent;
    transform.scale         = glm::vec3(fit_scale);
    transform.translation   = -glm::vec3((min_v + max_v) * 0.5f) * fit_scale;
    transform.translation.y = -min_v.y * fit_scale;  // sit on the grid
  }

  selected_entity_ = entity;

  LOG_INFO("Editor") << "Imported model '" << path.filename().string() << "': " << mesh->GetVertices().size()
                     << " vertices, " << mesh->GetIndexCount() / 3 << " triangles";
}

void Editor::DuplicateSelectedEntity() {
  if (selected_entity_.GetHandle() == entt::null || selected_entity_ == grid_entity_) {
    return;
  }

  Entity source = selected_entity_;

  Entity duplicate = CreateEntityWithUniqueName(source.GetComponent<Tag>().tag + " (Copy)");
  if (source.HasComponent<Transform>()) {
    duplicate.AddComponent<Transform>(source.GetComponent<Transform>());
  }
  if (source.HasComponent<MeshComponent>()) {
    auto &mesh = source.GetComponent<MeshComponent>();
    duplicate.AddComponent<MeshComponent>(mesh.mesh, mesh.material);
  }
  if (source.HasComponent<CameraComponent>()) {
    duplicate.AddComponent<CameraComponent>(source.GetComponent<CameraComponent>());
  }

  selected_entity_ = duplicate;
  LOG_DEBUG("Editor") << "Duplicated entity '" << source.GetComponent<Tag>().tag << "' -> '"
                      << duplicate.GetComponent<Tag>().tag << "'";
}

void Editor::ApplyDefaultLayout(ImGuiID dockspace_id) {
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

  ImGuiID dock_left   = 0;
  ImGuiID dock_right  = 0;
  ImGuiID dock_bottom = 0;

  ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.18f, &dock_left, &dockspace_id);
  ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.22f, &dock_right, &dockspace_id);
  ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.25f, &dock_bottom, &dockspace_id);

  // Left column: Scene on top, Properties below it.
  ImGuiID dock_left_scene = 0;
  ImGuiID dock_left_props = 0;
  ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.45f, &dock_left_props, &dock_left_scene);
  ImGui::DockBuilderDockWindow("Scene", dock_left_scene);
  ImGui::DockBuilderDockWindow("Properties", dock_left_props);

  ImGui::DockBuilderDockWindow("Content Browser", dock_bottom);
  ImGui::DockBuilderDockWindow("Log", dock_bottom);

  // Central area: the Viewport keeps most of the space, with the Script Editor
  // docked beside it so code and the game view stay visible together.
  ImGuiID dock_editor   = 0;
  ImGuiID dock_viewport = 0;
  ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.32f, &dock_editor, &dock_viewport);
  ImGui::DockBuilderDockWindow("Viewport", dock_viewport);
  ImGui::DockBuilderDockWindow("Script Editor", dock_editor);

  // Right column: Information on top, Lighting + Rendering below.
  ImGuiID dock_right_info = 0;
  ImGuiID dock_right_fx   = 0;
  ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.5f, &dock_right_fx, &dock_right_info);
  ImGui::DockBuilderDockWindow("Information", dock_right_info);
  ImGui::DockBuilderDockWindow("Lighting", dock_right_fx);
  ImGui::DockBuilderDockWindow("Rendering", dock_right_fx);

  ImGui::DockBuilderFinish(dockspace_id);
}

void Editor::ShowGizmo(const ImVec2 &image_pos, const ImVec2 &image_size) {
  if (selected_entity_.GetHandle() == entt::null || !selected_entity_.HasComponent<Transform>()) {
    return;
  }

  auto     &transform = selected_entity_.GetComponent<Transform>();
  glm::mat4 model     = transform.GetTransform();

  ImGuizmo::SetDrawlist();
  ImGuizmo::SetRect(image_pos.x, image_pos.y, image_size.x, image_size.y);
  ImGuizmo::Manipulate(glm::value_ptr(editor_camera_.GetViewMatrix()),
                       glm::value_ptr(editor_camera_.GetProjectionMatrix()), gizmo_operation_, ImGuizmo::LOCAL,
                       glm::value_ptr(model));

  if (ImGuizmo::IsUsing()) {
    glm::vec3 translation;
    glm::vec3 rotation;
    glm::vec3 scale;
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), glm::value_ptr(translation), glm::value_ptr(rotation),
                                          glm::value_ptr(scale));
    transform.translation = translation;
    transform.rotation    = rotation;  // degrees (matches Transform's convention)
    transform.scale       = scale;
  }
}

void Editor::DrawCameraGizmos(const ImVec2 &image_pos, const ImVec2 &image_size) {
  if (image_size.x <= 0.0f || image_size.y <= 0.0f) {
    return;
  }

  ImDrawList      *draw_list = ImGui::GetWindowDrawList();
  const glm::mat4 view_proj = editor_camera_.GetProjectionMatrix() * editor_camera_.GetViewMatrix();

  const auto world_to_screen = [&](const glm::vec3 &world) -> glm::vec2 {
    const glm::vec4 clip = view_proj * glm::vec4(world, 1.0f);
    if (clip.w <= 0.0f) {
      return glm::vec2(std::numeric_limits<float>::max(), 0.0f);
    }
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return glm::vec2(image_pos.x + (ndc.x * 0.5f + 0.5f) * image_size.x,
                     image_pos.y + (0.5f - ndc.y * 0.5f) * image_size.y);
  };

  const auto draw_line = [&](const glm::vec3 &a, const glm::vec3 &b, ImU32 color, float thickness = 1.5f) {
    const glm::vec2 s0 = world_to_screen(a);
    const glm::vec2 s1 = world_to_screen(b);
    if (s0.x == std::numeric_limits<float>::max() || s1.x == std::numeric_limits<float>::max()) {
      return;
    }
    draw_list->AddLine(ImVec2(s0.x, s0.y), ImVec2(s1.x, s1.y), color, thickness);
  };

  for (auto &entity : active_scene_->GetAllEntitiesWith<CameraComponent>()) {
    const Camera &camera  = entity.GetComponent<CameraComponent>().camera;
    const bool    primary = entity.GetComponent<CameraComponent>().primary;
    const ImU32   color   = primary ? IM_COL32(70, 200, 110, 255) : IM_COL32(130, 150, 200, 255);

    const glm::vec3 forward = camera.GetForward();
    const glm::vec3 right   = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 up      = glm::cross(right, forward);

    const float near_d = std::max(camera.near_plane, 0.05f);
    // A short, indicative frustum length (not the full far plane) so camera
    // gizmos stay readable instead of stretching across the scene.
    const float far_d  = near_d + 6.0f;
    const float aspect = std::max(camera.aspect_ratio, 0.01f);

    float half_h_near;
    float half_h_far;
    if (camera.projection_type == ProjectionType::Perspective) {
      const float tan_half = glm::tan(glm::radians(camera.fov_degrees) * 0.5f);
      half_h_near          = tan_half * near_d;
      half_h_far           = tan_half * far_d;
    } else {
      half_h_near = camera.ortho_size;
      half_h_far  = camera.ortho_size;
    }

    const glm::vec3 near_center = camera.position + forward * near_d;
    const glm::vec3 far_center  = camera.position + forward * far_d;

    glm::vec3 near_corners[4];
    glm::vec3 far_corners[4];
    for (int i = 0; i < 4; ++i) {
      const float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
      const float sy = (i < 2) ? 1.0f : -1.0f;
      near_corners[i] = near_center + right * (sx * half_h_near * aspect) + up * (sy * half_h_near);
      far_corners[i]  = far_center + right * (sx * half_h_far * aspect) + up * (sy * half_h_far);
    }

    // View frustum: near quad, far quad, and the four connecting edges.
    for (int i = 0; i < 4; ++i) {
      draw_line(near_corners[i], near_corners[(i + 1) % 4], color);
      draw_line(far_corners[i], far_corners[(i + 1) % 4], color);
      draw_line(near_corners[i], far_corners[i], color);
    }
    // View direction.
    draw_line(camera.position, far_center, color, 2.0f);

    // Camera body: a small box sitting behind the lens.
    const glm::vec3 body_center = camera.position - forward * 0.10f;
    const glm::vec3 hx          = right * 0.12f;
    const glm::vec3 hy          = up * 0.08f;
    const glm::vec3 hz          = forward * 0.16f;
    const glm::vec3 box[8]      = {
        body_center - hx - hy - hz, body_center + hx - hy - hz, body_center + hx + hy - hz,
        body_center - hx + hy - hz, body_center - hx - hy + hz, body_center + hx - hy + hz,
        body_center + hx + hy + hz, body_center - hx + hy + hz,
    };
    const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                              {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto &edge : edges) {
      draw_line(box[edge[0]], box[edge[1]], color);
    }
  }
}

void Editor::DrawColliderGizmos(const ImVec2 &image_pos, const ImVec2 &image_size) {
  if (image_size.x <= 0.0f || image_size.y <= 0.0f) {
    return;
  }

  ImDrawList      *draw_list = ImGui::GetWindowDrawList();
  const glm::mat4 view_proj = editor_camera_.GetProjectionMatrix() * editor_camera_.GetViewMatrix();

  const auto world_to_screen = [&](const glm::vec3 &world) -> glm::vec2 {
    const glm::vec4 clip = view_proj * glm::vec4(world, 1.0f);
    if (clip.w <= 0.0f) {
      return glm::vec2(std::numeric_limits<float>::max(), 0.0f);
    }
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return glm::vec2(image_pos.x + (ndc.x * 0.5f + 0.5f) * image_size.x,
                     image_pos.y + (0.5f - ndc.y * 0.5f) * image_size.y);
  };

  const auto draw_line = [&](const glm::vec3 &a, const glm::vec3 &b, ImU32 color, float thickness = 1.5f) {
    const glm::vec2 s0 = world_to_screen(a);
    const glm::vec2 s1 = world_to_screen(b);
    if (s0.x == std::numeric_limits<float>::max() || s1.x == std::numeric_limits<float>::max()) {
      return;
    }
    draw_list->AddLine(ImVec2(s0.x, s0.y), ImVec2(s1.x, s1.y), color, thickness);
  };

  const ImU32 color = IM_COL32(240, 160, 60, 255);

  for (auto &entity : active_scene_->GetAllEntitiesWith<ColliderComponent, Transform>()) {
    const auto     &collider  = entity.GetComponent<ColliderComponent>();
    const auto     &transform = entity.GetComponent<Transform>();
    const glm::vec3 center    = transform.translation + collider.offset;
    const glm::quat rotation  = glm::quat(glm::radians(transform.rotation));

    if (collider.shape == ColliderComponent::Shape::Sphere) {
      const float radius = collider.sphere_radius;
      constexpr int segments = 48;
      constexpr float two_pi = 6.28318530718f;
      for (int axis = 0; axis < 3; ++axis) {
        for (int i = 0; i < segments; ++i) {
          const float a0 = two_pi * static_cast<float>(i) / static_cast<float>(segments);
          const float a1 = two_pi * static_cast<float>(i + 1) / static_cast<float>(segments);
          glm::vec3   p0(0.0f);
          glm::vec3   p1(0.0f);
          if (axis == 0) {
            p0 = glm::vec3(0.0f, glm::cos(a0), glm::sin(a0));
            p1 = glm::vec3(0.0f, glm::cos(a1), glm::sin(a1));
          } else if (axis == 1) {
            p0 = glm::vec3(glm::cos(a0), 0.0f, glm::sin(a0));
            p1 = glm::vec3(glm::cos(a1), 0.0f, glm::sin(a1));
          } else {
            p0 = glm::vec3(glm::cos(a0), glm::sin(a0), 0.0f);
            p1 = glm::vec3(glm::cos(a1), glm::sin(a1), 0.0f);
          }
          draw_line(center + rotation * (p0 * radius), center + rotation * (p1 * radius), color);
        }
      }
    } else {
      // Capsule/Cylinder are shown as their axis-aligned bounding box for now.
      glm::vec3 he;
      switch (collider.shape) {
        case ColliderComponent::Shape::Capsule:
          he = glm::vec3(collider.capsule_radius, collider.capsule_half_height + collider.capsule_radius,
                         collider.capsule_radius);
          break;
        case ColliderComponent::Shape::Cylinder:
          he = glm::vec3(collider.cylinder_radius, collider.cylinder_half_height, collider.cylinder_radius);
          break;
        case ColliderComponent::Shape::Box:
        default:
          he = collider.box_half_extents;
          break;
      }
      glm::vec3       corners[8];
      for (int i = 0; i < 8; ++i) {
        const glm::vec3 local((i & 1) ? he.x : -he.x, (i & 2) ? he.y : -he.y, (i & 4) ? he.z : -he.z);
        corners[i] = center + rotation * local;
      }
      const int edges[12][2] = {{0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7},
                                {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
      for (const auto &edge : edges) {
        draw_line(corners[edge[0]], corners[edge[1]], color);
      }
    }
  }
}

Application *CreateApplication() { return new Editor(); }