#include "editor.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <backends/imgui_impl_glfw.cpp>
#include <backends/imgui_impl_opengl3.cpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/input.hpp"
#include "core/logger.hpp"
#include "core/task_dispatcher.hpp"
#include "render/renderer.hpp"

Editor::Editor() {}

Editor::~Editor() {}

void Editor::Initialize() {
  active_scene_ = std::make_shared<Scene>();

  Entity entity = active_scene_->CreateEntity("Checkerboard");

  auto shader = std::make_shared<Shader>("res/shaders/test_vert.glsl",
                                         "res/shaders/test_frag.glsl");

  auto texture = std::make_shared<Texture>("res/textures/checkerboard.png");

  // checkboard six vertices with uv for test
  float vertices[] = {
      // positions        // texture coords
      0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  // top right
      0.5f,  -0.5f, 0.0f, 1.0f, 0.0f,  // bottom right
      -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,  // bottom left
      -0.5f, 0.5f,  0.0f, 0.0f, 1.0f   // top left
  };
  unsigned int indices[] = {
      0, 1, 3,  // first triangle
      1, 2, 3   // second triangle
  };

  auto vertex_buffer = std::make_shared<GL::VertexBuffer>();
  vertex_buffer->SetData(vertices, sizeof(vertices));
  vertex_buffer->AddLayout({
      {GL::ShaderDataType::Float3, "aPos"},
      {GL::ShaderDataType::Float2, "aTexCoord"},
  });

  auto index_buffer = std::make_shared<GL::IndexBuffer>();
  index_buffer->SetData(indices, 6);

  auto vertex_array = std::make_shared<GL::VertexArray>();
  vertex_array->SetVertexBuffer(vertex_buffer);
  vertex_array->SetIndexBuffer(index_buffer);

  auto& render_info = entity.AddComponent<RenderInfo>();

  render_info.texture      = texture;
  render_info.shader       = shader;
  render_info.vertex_array = vertex_array;

  entity.AddComponent<Transform>(glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(1.0f, 1.0f, 1.0f));

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

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  frame_buffer_ = std::make_shared<FrameBuffer>();
}

void Editor::OnUpdate(float dt) {
  if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) {
    glfwSetWindowShouldClose(window_, true);
  }
  // handle logic
  // glm::vec3 direction(0.0f);
  // if (Input::IsKeyPressed(GLFW_KEY_A)) {
  //   direction += glm::vec3(-1.0f, 0.0f, 0.0f);
  // } else if (Input::IsKeyPressed(GLFW_KEY_D)) {
  //   direction += glm::vec3(1.0f, 0.0f, 0.0f);
  // }
  // if (Input::IsKeyPressed(GLFW_KEY_W)) {
  //   direction += glm::vec3(0.0f, 1.0f, 0.0f);
  // } else if (Input::IsKeyPressed(GLFW_KEY_S)) {
  //   direction += glm::vec3(0.0f, -1.0f, 0.0f);
  // }
  // if (direction != glm::vec3(0.0f)) direction = glm::normalize(direction);
  // float       velocity = 1.0f;
  // MoveCommand cmd(direction, velocity * dt);

  // task_dispatcher_->Run(&cmd);

  // handle render
  auto render_entities = active_scene_->GetAllEntitiesWith<RenderInfo>();
  auto camera          = active_scene_->GetCamera();

  if (viewport_resized_) {
    camera->OnWindowResize(viewport_width_, viewport_height_);
  }

  // TODO: do not control camera zoom level by keyboard.
  // if (Input::IsKeyPressed(GLFW_KEY_UP)) {
  //   camera->OnMouseScroll(1.0f * dt);
  // } else if (Input::IsKeyPressed(GLFW_KEY_DOWN)) {
  //   camera->OnMouseScroll(-1.0f * dt);
  // }

  frame_buffer_->Bind();
  frame_buffer_->Clear();
  if (viewport_resized_) {
    frame_buffer_->Resize(viewport_width_, viewport_height_);
    frame_buffer_->CheckStatus();
    frame_buffer_->AttachTexture();
    frame_buffer_->AttachRenderBuffer();
  }

  for (auto& entity : render_entities) {
    auto& transform = entity.GetComponent<Transform>();

    RenderInfo&   render_info = entity.GetComponent<RenderInfo>();
    RenderCommand render_command;
    render_command.SetViewProjectionMatrix(camera->GetViewProjectionMatrix());
    render_command.SetModelMatrix(
        entity.GetComponent<Transform>().GetModelMatrix());
    render_command.SetRenderInfo(render_info);
    renderer_->Run(&render_command);
  }

  frame_buffer_->Unbind();

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

  bool open = false;
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      ImGui::MenuItem("Open", NULL, &open);

      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }

  // draw frame buffer on scene using ImGui
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

  ImGui::Begin("Scene");
  std::vector<Entity>& entities = active_scene_->GetAllEntities();

  if (ImGui::Button("Create Entity")) {
    active_scene_->CreateEntity();
  }

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

    bool entity_deleted = false;
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Delete")) {
        entity_deleted = true;
      }
      ImGui::EndPopup();
    }

    if (opened) {
      ImGuiTreeNodeFlags flags =
          ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
      bool opened = ImGui::TreeNodeEx((void*)9817239, flags, tag.c_str());
      if (opened) {
        ImGui::TreePop();
      }
      ImGui::TreePop();
    }

    if (entity_deleted) {
      active_scene_->DestroyEntity(entity);
      selected_entity_ = Entity();
    }
  }
  ImGui::End();

  ImGui::Begin("Properties");

  if (selected_entity_.GetHandle() != entt::null) {
    auto& tag = selected_entity_.GetComponent<Tag>().tag;
    char  buffer[256];
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, tag.c_str());
    if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
      tag = std::string(buffer);
    }

    ImGui::Separator();

    if (ImGui::Button("Add Component")) {
      ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
      if (ImGui::MenuItem("Transform")) {
        selected_entity_.AddComponent<Transform>();
        ImGui::CloseCurrentPopup();
      }

      if (ImGui::MenuItem("Render Info")) {
        selected_entity_.AddComponent<RenderInfo>();
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    // add button to remove component
    if (selected_entity_.HasComponent<Transform>()) {
      if (ImGui::Button("Remove Transform")) {
        selected_entity_.RemoveComponent<Transform>();
      }
    }

    if (selected_entity_.HasComponent<RenderInfo>()) {
      if (ImGui::Button("Remove Render Info")) {
        selected_entity_.RemoveComponent<RenderInfo>();
      }
    }

    if (selected_entity_.HasComponent<Transform>()) {
      if (ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& transform = selected_entity_.GetComponent<Transform>();
        ImGui::DragFloat3("Position", glm::value_ptr(transform.position), 0.1f);
        ImGui::DragFloat3("Rotation", glm::value_ptr(transform.rotation), 0.1f);
        ImGui::DragFloat3("Scale", glm::value_ptr(transform.scale), 0.1f);
        ImGui::TreePop();
      }
    }

    if (selected_entity_.HasComponent<RenderInfo>()) {
      if (ImGui::TreeNodeEx("Render Info", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& render_info = selected_entity_.GetComponent<RenderInfo>();
        ImGui::Text("Vertex Shader: %s",
                    render_info.shader->GetVertPath().c_str());
        ImGui::Text("Fragment Shader: %s",
                    render_info.shader->GetFragPath().c_str());
        ImGui::Text("Texture: %s", render_info.texture->GetPath().c_str());

        ImGui::Image((void*)(intptr_t)render_info.texture->GetID(),
                     ImVec2(100, 100));
        ImGui::TreePop();
      }
    }
  }

  ImGui::End();

  ImGui::Begin("Log");
  ImGui::Text("Hello, world!");
  ImGui::End();

  // print fps
  ImGui::Begin("Information");
  ImGui::Text("FPS: %d", GetFPS());
  ImGui::End();

  ImGui::End();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

Application* CreateApplication() { return new Editor(); }