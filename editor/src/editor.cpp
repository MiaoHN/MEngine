#include "editor.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <backends/imgui_impl_glfw.cpp>
#include <backends/imgui_impl_opengl3.cpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// clang-format off
// #include <ImGuizmo.h>
// clang-format on

#include "core/input.hpp"
#include "core/task_dispatcher.hpp"
#include "render/renderer.hpp"

Editor::Editor() {}

Editor::~Editor() {}

void Editor::Initialize() {
  active_scene_ = std::make_shared<Scene>();

  auto texture1 =
      texture_library_.Load("checkerboard", "res/textures/checkerboard.png");

  auto texture2 = texture_library_.Load("wall", "res/textures/wall.jpg");

  Entity entity1 = active_scene_->CreateEntity("Checkerboard");
  auto&  sprite1 = entity1.AddComponent<Sprite>();

  sprite1.position = glm::vec3(0.5f, 0.0f, 0.0f);
  sprite1.scale    = glm::vec3(1.0f, 1.0f, 1.0f);
  sprite1.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
  sprite1.color    = glm::vec4(1.0f);
  sprite1.texture  = texture1;

  Entity entity2 = active_scene_->CreateEntity("Wall");
  auto&  sprite2 = entity2.AddComponent<Sprite>();

  sprite2.position = glm::vec3(-0.5f, 0.0f, 0.0f);
  sprite2.scale    = glm::vec3(1.0f, 1.0f, 1.0f);
  sprite2.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
  sprite2.color    = glm::vec4(1.0f);
  sprite2.texture  = texture2;

  camera_ = std::make_shared<OrthographicCamera>(-1.0f, 1.0f, -1.0f, 1.0f);

  active_scene_->SetCamera(camera_);

  renderer_ = std::make_shared<Renderer>();

  task_dispatcher_->PushHandler(active_scene_->GetCamera());

  script_engine_ = std::make_shared<ScriptEngine>();

  script_engine_->LoadScript("res/scripts/test.lua");

  // ImGUI setup
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsLight();

  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  frame_buffer_ = std::make_shared<FrameBuffer>();
}

void Editor::OnUpdate(float dt) {
  if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) {
    glfwSetWindowShouldClose(window_, true);
  }

  auto camera = active_scene_->GetCamera();

  if (viewport_resized_) {
    camera->OnWindowResize(viewport_width_, viewport_height_);
  }

  frame_buffer_->Bind();
  frame_buffer_->Clear();
  if (viewport_resized_) {
    frame_buffer_->Resize(viewport_width_, viewport_height_);
    frame_buffer_->CheckStatus();
    frame_buffer_->AttachTexture();
    frame_buffer_->AttachRenderBuffer();
  }

  for (auto& entity : active_scene_->GetAllEntitiesWith<Sprite>()) {
    auto& sprite = entity.GetComponent<Sprite>();
    renderer_->RenderSprite(sprite, camera->GetProjectionView());
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
  ImGui::Text("Hello, world!");
  ImGui::End();

  // print fps
  ImGui::Begin("Information");
  ImGui::Text("FPS: %d", GetFPS());
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
  ImVec2 size = ImGui::GetContentRegionAvail();
  if (viewport_width_ != size.x || viewport_height_ != size.y) {
    viewport_width_   = size.x;
    viewport_height_  = size.y;
    viewport_resized_ = true;
    frame_buffer_->Resize(viewport_width_, viewport_height_);
  } else {
    viewport_resized_ = false;
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
      if (ImGui::MenuItem("Sprite")) {
        auto& sprite = selected_entity_.AddComponent<Sprite>();

        sprite.position = glm::vec3(0.0f, 0.0f, 0.0f);
        sprite.scale    = glm::vec3(1.0f, 1.0f, 1.0f);
        sprite.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        sprite.color    = glm::vec4(1.0f);

        sprite.texture = texture_library_.Get("checkerboard");
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    if (selected_entity_.HasComponent<Sprite>()) {
      if (ImGui::Button("Remove Sprite")) {
        selected_entity_.RemoveComponent<Sprite>();
      }
    }

    if (selected_entity_.HasComponent<Sprite>()) {
      if (ImGui::TreeNodeEx("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& sprite = selected_entity_.GetComponent<Sprite>();
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