#include "editor.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <limits>

#include "render/asset_manager.hpp"
#include "render/model_loader.hpp"
#include "utils/profiler.h"

namespace {

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

  return mesh != nullptr;
}

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

Editor::Editor() = default;

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

  script_engine_ = std::make_shared<ScriptEngine>();
  script_engine_->LoadScript(AssetManager::Instance().Resolve("scripts/test.lua"));

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
  grid_entity_.AddComponent<Transform>();
  grid_entity_.AddComponent<MeshComponent>(grid_mesh_, grid_material_);

  // Physics demo: a static ground box plus a stack of dynamic boxes topped
  // with a sphere. Everything is at rest until Play is pressed.
  CreatePhysicsDemo();

  base_directory_    = std::filesystem::absolute(AssetManager::Instance().GetAssetRoot());
  current_directory_ = base_directory_;
  directory_icon_    = AssetManager::Instance().GetTexture("icons/DirectoryIcon.png");
  file_icon_         = AssetManager::Instance().GetTexture("icons/FileIcon.png");

  LOG_INFO("Editor") << "Editor initialized (scene + ImGui + viewport framebuffer)";
}

void Editor::OnUpdate(float dt) {
  PROFILER_FUNCTION();

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

  // Advance the physics simulation while in Play mode (the physics world
  // writes body transforms back into the matching Transform components).
  if (game_mode_ == GameMode::Play) {
    active_scene_->StepSimulation(dt);
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
  }

  bool open = false;
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      ImGui::MenuItem("Open", nullptr, &open);
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

  if (ImGui::Button("Create")) {
    ImGui::OpenPopup("CreateEntity");
  }

  if (ImGui::BeginPopup("CreateEntity")) {
    if (ImGui::MenuItem("Empty Entity")) CreatePrimitive("Empty", nullptr);
    if (ImGui::MenuItem("Cube")) CreatePrimitive("Cube", Mesh::CreateCube());
    if (ImGui::MenuItem("Plane")) CreatePrimitive("Plane", Mesh::CreatePlane());
    if (ImGui::MenuItem("Sphere")) CreatePrimitive("Sphere", Mesh::CreateSphere());
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
      SetGridVisible(false);
    } else {
      game_mode_ = GameMode::Edit;
      active_scene_->StopSimulation();
      SetGridVisible(true);
    }
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
      DisplayAddComponentEntry<RigidBodyComponent>("Rigid Body");
      DisplayAddComponentEntry<ColliderComponent>("Collider");

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
    });

    DrawComponent<ColliderComponent>("Collider", selected_entity_, [](auto &component) {
      const char *shapes[] = {"Box", "Sphere"};
      int         current  = component.shape == ColliderComponent::Shape::Box ? 0 : 1;
      if (ImGui::Combo("Shape", &current, shapes, 2)) {
        component.shape = current == 0 ? ColliderComponent::Shape::Box : ColliderComponent::Shape::Sphere;
      }
      if (component.shape == ColliderComponent::Shape::Sphere) {
        ImGui::DragFloat("Radius", &component.sphere_radius, 0.01f, 0.01f, 100.0f);
      } else {
        DrawVec3Control("Half Extents", component.box_half_extents, 1.0f);
      }
      DrawVec3Control("Offset", component.offset, 1.0f);
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

    ImGui::PopID();
    ++index;
  }

  ImGui::EndChild();
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
  // Ground: a static box. Colliders are world-space (the transform's scale is
  // intentionally ignored by the physics world), so its half extents match
  // the visible, scaled box.
  Entity ground = active_scene_->CreateEntity("Ground");
  ground.AddComponent<Transform>(glm::vec3(0.0f, -0.5f, 0.0f));
  ground.GetComponent<Transform>().scale = glm::vec3(8.0f, 1.0f, 8.0f);
  ground.AddComponent<MeshComponent>(Mesh::CreateCube(), CreateRef<Material>(*default_material_));
  ground.AddComponent<RigidBodyComponent>(RigidBodyComponent::Type::Static);
  {
    ColliderComponent collider;
    collider.shape            = ColliderComponent::Shape::Box;
    collider.box_half_extents = glm::vec3(4.0f, 0.5f, 4.0f);
    ground.AddComponent<ColliderComponent>(collider);
  }

  // A stack of dynamic boxes topped with a sphere. Everything is at rest until
  // Play is pressed and StartSimulation builds the Jolt bodies.
  for (int i = 0; i < 5; ++i) {
    Entity box = active_scene_->CreateEntity("Box " + std::to_string(i + 1));
    const float jitter_x = static_cast<float>((i % 3) - 1) * 0.04f;
    const float jitter_z = (static_cast<float>(i % 2) - 0.5f) * 0.06f;
    box.AddComponent<Transform>(glm::vec3(jitter_x, 0.6f + static_cast<float>(i) * 1.05f, jitter_z));
    box.AddComponent<MeshComponent>(Mesh::CreateCube(), CreateRef<Material>(*default_material_));
    box.AddComponent<RigidBodyComponent>();
    box.AddComponent<ColliderComponent>();
  }

  Entity sphere = active_scene_->CreateEntity("Sphere");
  const float stack_top = 0.6f + 4.0f * 1.05f + 0.5f;  // top surface of the 5th box
  sphere.AddComponent<Transform>(glm::vec3(0.0f, stack_top + 0.55f, 0.0f));
  sphere.AddComponent<MeshComponent>(Mesh::CreateSphere(), CreateRef<Material>(*default_material_));
  sphere.AddComponent<RigidBodyComponent>();
  {
    ColliderComponent collider;
    collider.shape         = ColliderComponent::Shape::Sphere;
    collider.sphere_radius = 0.5f;
    sphere.AddComponent<ColliderComponent>(collider);
  }

  LOG_INFO("Editor") << "Physics demo scene created (ground + box stack + sphere)";
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
  ImGui::DockBuilderDockWindow("Viewport", dockspace_id);

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
      const glm::vec3 he = collider.box_half_extents;
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