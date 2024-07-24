#include "editor.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>

// clang-format off
// #include <ImGuizmo.h>
// clang-format on

#include "core/input.hpp"
#include "render/renderer.hpp"

Editor::Editor() {}

Editor::~Editor() {}

void Editor::Initialize() {
  active_scene_ = std::make_shared<Scene>();

  editor_camera_info_ = std::make_shared<Camera2D>(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, true);

  editor_camera_info_->SetZoomLevel(100.0f);

  script_engine_ = std::make_shared<ScriptEngine>();

  script_engine_->LoadScript("res/scripts/test.lua");

  // ImGUI setup
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  frame_buffer_ = std::make_shared<FrameBuffer>();

  // TODO
  m_BaseDirectory    = std::filesystem::current_path();
  m_CurrentDirectory = m_BaseDirectory;
  m_DirectoryIcon    = Texture::Create("res/icon/DirectoryIcon.png");
  m_FileIcon         = Texture::Create("res/icon/FileIcon.png");
}

void Editor::OnUpdate(float dt) {
  if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) {
    glfwSetWindowShouldClose(window_, true);
  }

  if (viewport_resized_) {
    editor_camera_info_->OnWindowResize(viewport_width_, viewport_height_);
  }

  frame_buffer_->Bind();
  frame_buffer_->Clear();
  if (viewport_resized_) {
    frame_buffer_->Resize(viewport_width_, viewport_height_);
    frame_buffer_->CheckStatus();
    frame_buffer_->AttachTexture();
    frame_buffer_->AttachRenderBuffer();
  }

  switch (game_mode_) {
    case GameMode::Edit: {
      active_scene_->OnUpdateEditor(*editor_camera_info_);
      break;
    }
    case GameMode::Play: {
      active_scene_->OnUpdateRuntime(dt, viewport_width_, viewport_height_);
      break;
    }
  }

  frame_buffer_->Unbind();

  BeginImGui();

  bool open = false;
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      ImGui::MenuItem("Open", NULL, &open);

      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }

  // Content Browser
  ImGui::Begin("Content Browser");

  if (m_CurrentDirectory != std::filesystem::path(m_BaseDirectory)) {
    if (ImGui::Button("<-")) {
      m_CurrentDirectory = m_CurrentDirectory.parent_path();
    }
  }

  static float padding       = 16.0f;
  static float thumbnailSize = 128.0f;
  float        cellSize      = thumbnailSize + padding;

  float panelWidth  = ImGui::GetContentRegionAvail().x;
  int   columnCount = (int)(panelWidth / cellSize);
  if (columnCount < 1) columnCount = 1;

  ImGui::Columns(columnCount, 0, false);

  for (auto &directoryEntry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
    const auto &path           = directoryEntry.path();
    std::string filenameString = path.filename().string();

    ImGui::PushID(filenameString.c_str());
    std::shared_ptr<Texture> icon = directoryEntry.is_directory() ? m_DirectoryIcon : m_FileIcon;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::ImageButton((ImTextureID)icon->GetID(), {thumbnailSize, thumbnailSize}, {0, 1}, {1, 0});

    if (ImGui::BeginDragDropSource()) {
      std::filesystem::path relativePath(path);
      const wchar_t        *itemPath = relativePath.c_str();
      ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath, (wcslen(itemPath) + 1) * sizeof(wchar_t));
      ImGui::EndDragDropSource();
    }

    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      if (directoryEntry.is_directory()) m_CurrentDirectory /= path.filename();
    }
    ImGui::TextWrapped("%s", filenameString.c_str());

    ImGui::NextColumn();

    ImGui::PopID();
  }

  ImGui::Columns(1);

  ImGui::SliderFloat("Thumbnail Size", &thumbnailSize, 16, 512);
  ImGui::SliderFloat("Padding", &padding, 0, 32);

  // TODO: status bar
  ImGui::End();

  ShowImGuiViewport();

  ShowImGuiScene();

  ShowImGuiProperties();

  ImGui::Begin("Log");
  std::ifstream     file("MEngine.log");
  std::stringstream ss;
  if (file.is_open()) {
    ss << file.rdbuf();
  }
  std::string log = ss.str();
  ImGui::Text("%s", log.c_str());

  ImGui::End();

  // print fps
  ImGui::Begin("Information");
  ImGui::Text("FPS: %d", GetFPS());
  // control editor camera
  ImGui::Text("Camera Control");
  if (ImGui::DragFloat2("Position", glm::value_ptr(editor_camera_info_->GetPosition()), 0.1f)) {
    editor_camera_info_->RecalculateViewMatrix();
  }
  if (ImGui::DragFloat("Rotation", &editor_camera_info_->GetRotation(), 0.1f)) {
    editor_camera_info_->RecalculateViewMatrix();
  }
  if (ImGui::DragFloat("Zoom Level", &editor_camera_info_->GetZoomLevel(), 0.1f, 0.1f, 100.0f)) {
    editor_camera_info_->SetProjection(-editor_camera_info_->GetAspectRatio() * editor_camera_info_->GetZoomLevel(),
                                       editor_camera_info_->GetAspectRatio() * editor_camera_info_->GetZoomLevel(),
                                       -editor_camera_info_->GetZoomLevel(), editor_camera_info_->GetZoomLevel());
  }
  if (ImGui::DragFloat("Aspect Ratio", &editor_camera_info_->GetAspectRatio(), 0.1f)) {
    editor_camera_info_->SetProjection(-editor_camera_info_->GetAspectRatio() * editor_camera_info_->GetZoomLevel(),
                                       editor_camera_info_->GetAspectRatio() * editor_camera_info_->GetZoomLevel(),
                                       -editor_camera_info_->GetZoomLevel(), editor_camera_info_->GetZoomLevel());
  }

  if (ImGui::Button("Reset Camera")) {
    editor_camera_info_->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    editor_camera_info_->SetRotation(0.0f);
    editor_camera_info_->SetZoomLevel(1.0f);
    editor_camera_info_->SetAspectRatio((float)viewport_width_ / viewport_height_);
  }

  ImGui::End();

  ImGui::End();

  EndImGui();
}

void Editor::BeginImGui() {
  // ImGui test
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  // Note: Switch this to true to enable dockspace
  static bool               dockspaceOpen             = true;
  static bool               opt_fullscreen_persistant = true;
  bool                      opt_fullscreen            = opt_fullscreen_persistant;
  static ImGuiDockNodeFlags dockspace_flags           = ImGuiDockNodeFlags_None;

  // We are using the ImGuiWindowFlags_NoDocking flag to make the parent
  // window not dockable into, because it would be confusing to have two
  // docking targets within each others.
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  if (opt_fullscreen) {
    ImGuiViewport *viewport = ImGui::GetMainViewport();
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
  ImGuiIO    &io          = ImGui::GetIO();
  ImGuiStyle &style       = ImGui::GetStyle();
  float       minWinSizeX = style.WindowMinSize.x;
  style.WindowMinSize.x   = 370.0f;
  if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
  }
}

void Editor::EndImGui() {
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

template <typename T, typename UIFunction>
static void DrawComponent(const std::string &name, Entity entity, UIFunction uiFunction) {
  const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
                                           ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap |
                                           ImGuiTreeNodeFlags_FramePadding;
  if (entity.HasComponent<T>()) {
    auto  &component              = entity.GetComponent<T>();
    ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4, 4});
    float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
    ImGui::Separator();
    bool open = ImGui::TreeNodeEx((void *)typeid(T).hash_code(), treeNodeFlags, "%s", name.c_str());
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
  ImGuiIO &io       = ImGui::GetIO();
  auto     boldFont = io.Fonts->Fonts[0];

  ImGui::PushID(label.c_str());

  ImGui::Columns(2);
  ImGui::SetColumnWidth(0, columnWidth);
  ImGui::Text("%s", label.c_str());
  ImGui::NextColumn();

  ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});

  float  lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
  ImVec2 buttonSize = {lineHeight + 3.0f, lineHeight};

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
  if (!selected_entity_.HasComponent<T>()) {
    if (ImGui::MenuItem(entryName.c_str())) {
      selected_entity_.AddComponent<T>();
      ImGui::CloseCurrentPopup();
    }
  }
}

void Editor::ShowImGuiScene() {
  ImGui::Begin("Scene");

  if (ImGui::Button("Create")) {
    active_scene_->CreateEntity();
  }

  ImGui::SameLine();

  // delete selected entity
  if (ImGui::Button("Delete")) {
    if (selected_entity_.GetHandle() != entt::null) {
      active_scene_->DestroyEntity(selected_entity_);
      selected_entity_ = Entity();
    }
  }

  std::vector<Entity> &entities = active_scene_->GetAllEntities();

  for (Entity &entity : entities) {
    auto              &tag = entity.GetComponent<Tag>().tag;
    ImGuiTreeNodeFlags flags =
        ((entity == selected_entity_) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
    bool opened = ImGui::TreeNodeEx((void *)(intptr_t)entity.GetHandle(), flags, "%s", tag.c_str());

    if (ImGui::IsItemClicked()) {
      selected_entity_ = entity;
    }
  }

  ImGui::End();
}

void Editor::ShowImGuiViewport() {
  ImGui::Begin("Viewport");
  ImVec2 size        = ImGui::GetContentRegionAvail();
  ImVec2 button_size = ImVec2(50, 25);
  size.y -= button_size.y + 5;
  if (viewport_width_ != size.x || viewport_height_ != size.y) {
    viewport_width_   = size.x;
    viewport_height_  = size.y;
    viewport_resized_ = true;
    frame_buffer_->Resize(viewport_width_, viewport_height_);
  } else {
    viewport_resized_ = false;
  }

  ImVec2 button_pos((size.x - button_size.x) / 2, button_size.y);

  ImGui::SetCursorPos(button_pos);

  if (game_mode_ == GameMode::Edit) {
    if (ImGui::Button("Play", button_size)) {
      game_mode_ = GameMode::Play;
    }
  } else {
    if (ImGui::Button("Stop", button_size)) {
      game_mode_ = GameMode::Edit;
    }
  }

  ImGui::Image((void *)(intptr_t)frame_buffer_->GetTextureId(), size, ImVec2(0, 1), ImVec2(1, 0));

  ImGui::End();
}

void Editor::ShowImGuiProperties() {
  ImGui::Begin("Properties");

  if (selected_entity_.GetHandle() != entt::null) {
    auto &tag = selected_entity_.GetComponent<Tag>().tag;
    char  buffer[256];
    memset(buffer, 0, sizeof(buffer));
    strcpy_s(buffer, sizeof(buffer), tag.c_str());
    if (ImGui::InputText("##Tag", buffer, sizeof(buffer))) {
      tag = std::string(buffer);
    }

    ImGui::SameLine();
    ImGui::PushItemWidth(-1);

    if (ImGui::Button("Add Component")) {
      ImGui::OpenPopup("AddComponent");
    }

    if (ImGui::BeginPopup("AddComponent")) {
      DisplayAddComponentEntry<Transform>("Transform");
      DisplayAddComponentEntry<Sprite2D>("Sprite2D");
      DisplayAddComponentEntry<Camera2D>("Camera2D");

      ImGui::EndPopup();
    }

    ImGui::PopItemWidth();

    DrawComponent<Transform>("Transform", selected_entity_, [](auto &component) {
      DrawVec3Control("Translation", component.translation);
      glm::vec3 rotation = glm::degrees(component.rotation);
      DrawVec3Control("Rotation", rotation);
      component.rotation = glm::radians(rotation);
      DrawVec3Control("Scale", component.scale, 1.0f);
    });

    DrawComponent<Camera2D>("Camera", selected_entity_, [](auto &component) {
      ImGui::Checkbox("Primary", &component.primary);

      DrawVec3Control("Position", component.position);

      ImGui::DragFloat("Rotation", &component.rotation, 0.1f);

      ImGui::DragFloat("Zoom Level", &component.zoom_level, 0.1f, 0.0f, 100.0f);

      ImGui::DragFloat("Aspect Ratio", &component.aspect_ratio, 0.1f);
    });

    DrawComponent<Sprite2D>("Sprite2D", selected_entity_, [](auto &component) {
      ImGui::ColorEdit4("Color", glm::value_ptr(component.color));

      ImGui::Button("Texture", ImVec2(100.0f, 0.0f));
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
          const wchar_t           *path = (const wchar_t *)payload->Data;
          std::filesystem::path    texturePath(path);
          std::shared_ptr<Texture> texture = Texture::Create(texturePath.string());
          component.texture                = texture;
        }
        ImGui::EndDragDropTarget();
      }

      ImGui::DragFloat("Tiling Factor", &component.tiling_factor, 0.1f, 0.0f, 100.0f);
    });
  }

  ImGui::End();
}

Application *CreateApplication() { return new Editor(); }