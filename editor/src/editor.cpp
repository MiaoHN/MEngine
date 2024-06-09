#include "editor.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <backends/imgui_impl_glfw.cpp>
#include <backends/imgui_impl_opengl3.cpp>
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

  auto texture1 =
      texture_library_.Load("checkerboard", "res/textures/checkerboard.png");

  auto texture2 =
      texture_library_.Load("qoguldsd", "res/textures/qoguldsd.png");

  Entity entity1 = active_scene_->CreateEntity("Checkerboard");
  auto&  sprite1 = entity1.AddComponent<Sprite2D>();

  sprite1.position = glm::vec3(0.5f, 0.0f, -1.0f);
  sprite1.scale = glm::vec3(texture1->GetWidth() / 32, texture1->GetHeight() / 32, 1.0f);
  sprite1.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
  sprite1.color    = glm::vec4(1.0f);
  sprite1.texture  = texture1;

  Entity entity2 = active_scene_->CreateEntity("qoguldsd");
  auto&  sprite2 = entity2.AddComponent<AnimatedSprite2D>();

  sprite2.position = glm::vec3(0.5f, 0.0f, 0.0f);
  sprite2.scale = glm::vec3(texture2->GetWidth() / 32, texture2->GetHeight() / 32, 1.0f);
  sprite2.rotation      = glm::vec3(0.0f, 0.0f, 0.0f);
  sprite2.color         = glm::vec4(1.0f);
  sprite2.texture       = texture2;
  sprite2.frame_time    = 0.1f;
  sprite2.current_frame = 0;
  sprite2.current_time  = 0.0f;

  texture2->SetHFrames(6);
  texture2->SetVFrames(1);

  auto& camera_info =
      entity2.AddComponent<Camera2D>(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, true);
  camera_info.SetZoomLevel(100.0f);

  editor_camera_info_ =
      std::make_shared<Camera2D>(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, true);

  editor_camera_info_->SetZoomLevel(100.0f);

  renderer_ = std::make_shared<Renderer>();

  script_engine_ = std::make_shared<ScriptEngine>();

  script_engine_->LoadScript("res/scripts/test.lua");

  // ImGUI setup
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  frame_buffer_ = std::make_shared<FrameBuffer>();
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

  if (game_mode_ == GameMode::Edit) {
    for (auto& entity : active_scene_->GetAllEntitiesWith<Sprite2D>()) {
      auto& sprite = entity.GetComponent<Sprite2D>();
      renderer_->RenderSprite(sprite, editor_camera_info_->GetProjectionView());
    }
    for (auto& entity : active_scene_->GetAllEntitiesWith<AnimatedSprite2D>()) {
      auto& sprite = entity.GetComponent<AnimatedSprite2D>();
      renderer_->RenderSprite(sprite, editor_camera_info_->GetProjectionView());
    }
  } else {
    bool has_primary_camera = false;
    for (auto& entity : active_scene_->GetAllEntitiesWith<Camera2D>()) {
      auto& camera_info = entity.GetComponent<Camera2D>();
      glm::vec3 position;
      float     rotation;
      if (entity.HasComponent<Sprite2D>()) {
        auto& sprite = entity.GetComponent<Sprite2D>();
        position = sprite.position;
        rotation = sprite.rotation.z;
      } else if (entity.HasComponent<AnimatedSprite2D>()) {
        auto& sprite = entity.GetComponent<AnimatedSprite2D>();
        position = sprite.position;
        rotation = sprite.rotation.z;
      } else {
        position = glm::vec3(0.0f);
        rotation = 0.0f;
      }
      if (camera_info.primary) {
        has_primary_camera = true;
        camera_info.SetPosition(position);
        camera_info.SetRotation(rotation);
        camera_info.SetProjection(-1.0f, 1.0f, -1.0f, 1.0f);
        camera_info.SetAspectRatio((float)viewport_width_ / viewport_height_);
        for (auto& entity : active_scene_->GetAllEntitiesWith<Sprite2D>()) {
          auto& sprite = entity.GetComponent<Sprite2D>();
          renderer_->RenderSprite(sprite, camera_info.GetProjectionView());
        }
        for (auto& entity : active_scene_->GetAllEntitiesWith<AnimatedSprite2D>()) {
          auto& sprite = entity.GetComponent<AnimatedSprite2D>();
          renderer_->RenderSprite(sprite, camera_info.GetProjectionView());
        }
      }
    }
    if (!has_primary_camera) {
      for (auto& entity : active_scene_->GetAllEntitiesWith<Sprite2D>()) {
        auto& sprite = entity.GetComponent<Sprite2D>();
        renderer_->RenderSprite(
            sprite, scene_->GetDefaultCameraInfo()->GetProjectionView());
      }
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
  if (ImGui::DragFloat2("Position",
                        glm::value_ptr(editor_camera_info_->GetPosition()),
                        0.1f)) {
    editor_camera_info_->RecalculateViewMatrix();
  }
  if (ImGui::DragFloat("Rotation", &editor_camera_info_->GetRotation(), 0.1f)) {
    editor_camera_info_->RecalculateViewMatrix();
  }
  if (ImGui::DragFloat("Zoom Level", &editor_camera_info_->GetZoomLevel(), 0.1f,
                       0.1f, 100.0f)) {
    editor_camera_info_->SetProjection(-editor_camera_info_->GetAspectRatio() *
                                           editor_camera_info_->GetZoomLevel(),
                                       editor_camera_info_->GetAspectRatio() *
                                           editor_camera_info_->GetZoomLevel(),
                                       -editor_camera_info_->GetZoomLevel(),
                                       editor_camera_info_->GetZoomLevel());
  }
  if (ImGui::DragFloat("Aspect Ratio", &editor_camera_info_->GetAspectRatio(),
                       0.1f)) {
    editor_camera_info_->SetProjection(-editor_camera_info_->GetAspectRatio() *
                                           editor_camera_info_->GetZoomLevel(),
                                       editor_camera_info_->GetAspectRatio() *
                                           editor_camera_info_->GetZoomLevel(),
                                       -editor_camera_info_->GetZoomLevel(),
                                       editor_camera_info_->GetZoomLevel());
  }

  if (ImGui::Button("Reset Camera")) {
    editor_camera_info_->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    editor_camera_info_->SetRotation(0.0f);
    editor_camera_info_->SetZoomLevel(1.0f);
    editor_camera_info_->SetAspectRatio((float)viewport_width_ /
                                        viewport_height_);
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
  bool                      opt_fullscreen  = opt_fullscreen_persistant;
  static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

  // We are using the ImGuiWindowFlags_NoDocking flag to make the parent
  // window not dockable into, because it would be confusing to have two
  // docking targets within each others.
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  if (opt_fullscreen) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |=
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
  }

  // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will
  // render our background and handle the pass-thru hole, so we ask Begin() to
  // not render a background.
  if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
    window_flags |= ImGuiWindowFlags_NoBackground;

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
  ImGuiIO&    io          = ImGui::GetIO();
  ImGuiStyle& style       = ImGui::GetStyle();
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

  std::vector<Entity>& entities = active_scene_->GetAllEntities();

  for (Entity& entity : entities) {
    auto&              tag = entity.GetComponent<Tag>().tag;
    ImGuiTreeNodeFlags flags =
        ((entity == selected_entity_) ? ImGuiTreeNodeFlags_Selected : 0) |
        ImGuiTreeNodeFlags_OpenOnArrow;
    bool opened = ImGui::TreeNodeEx((void*)(intptr_t)entity.GetHandle(), flags,
                                    "%s", tag.c_str());

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

  ImGui::Image((void*)(intptr_t)frame_buffer_->GetTextureId(), size,
               ImVec2(0, 1), ImVec2(1, 0));

  ImGui::End();
}

void Editor::ShowImGuiProperties() {
  ImGui::Begin("Properties");

  if (selected_entity_.GetHandle() != entt::null) {
    auto& tag = selected_entity_.GetComponent<Tag>().tag;
    char  buffer[256];
    memset(buffer, 0, sizeof(buffer));
    strcpy_s(buffer, sizeof(buffer), tag.c_str());
    if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
      tag = std::string(buffer);
    }

    ImGui::Separator();

    if (ImGui::Button("Add Component")) {
      ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
      if (ImGui::MenuItem("Sprite2D")) {
        auto& sprite = selected_entity_.AddComponent<Sprite2D>();

        sprite.position = glm::vec3(0.0f, 0.0f, 0.0f);
        sprite.scale    = glm::vec3(1.0f, 1.0f, 1.0f);
        sprite.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        sprite.color    = glm::vec4(1.0f);

        sprite.texture = texture_library_.Get("checkerboard");
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    if (selected_entity_.HasComponent<Sprite2D>()) {
      if (ImGui::Button("Remove Sprite2D")) {
        selected_entity_.RemoveComponent<Sprite2D>();
      }
    }

    if (selected_entity_.HasComponent<Sprite2D>()) {
      if (ImGui::TreeNodeEx("Sprite2D", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& sprite = selected_entity_.GetComponent<Sprite2D>();
        ImGui::DragFloat2("Position", glm::value_ptr(sprite.position), 0.1f);
        ImGui::DragFloat("Rotation", &sprite.rotation.z, 0.1f);
        ImGui::DragFloat2("Scale", glm::value_ptr(sprite.scale), 0.1f);
        ImGui::Text("Texture: %s", sprite.texture->GetPath().c_str());
        ImGui::Image((void*)(intptr_t)sprite.texture->GetID(),
                     ImVec2(100, 100));

        // TODO: Add button to change shader and texture

        ImGui::TreePop();
      }
    }
  }

  ImGui::End();
}

Application* CreateApplication() { return new Editor(); }