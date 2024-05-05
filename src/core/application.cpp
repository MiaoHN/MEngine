#include "core/application.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include <imgui.h>

#include <backends/imgui_impl_glfw.cpp>
#include <backends/imgui_impl_opengl3.cpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/command.hpp"
#include "core/input.hpp"
#include "core/script_engine.hpp"
#include "core/task_dispatcher.hpp"
#include "core/task_handler.hpp"
#include "render/frame_buffer.hpp"
#include "render/gl.hpp"
#include "render/renderer.hpp"
#include "render/shader.hpp"
#include "scene/camera.hpp"
#include "scene/component.hpp"
#include "scene/scene.hpp"

namespace MEngine {

static Application* s_app;

Application* Application::GetInstance() { return s_app; }

Application::Application() {
  if (s_app) {
    logger_->error("Application already exists");
    exit(-1);
  }
  s_app   = this;
  logger_ = Logger::Get("Application");
  logger_->info("Application started");
  prev_time_   = glfwGetTime();
  frame_time_  = glfwGetTime();
  frame_count_ = 0;
  fps_         = 0;

  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  window_ = glfwCreateWindow(1600, 900, "MEngine", nullptr, nullptr);

  if (!window_) {
    logger_->error("Failed to create GLFW window");
    glfwTerminate();
    exit(-1);
  }

  glfwMakeContextCurrent(window_);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    logger_->critical("Failed to initialize GLAD!");
    exit(-1);
  }

  task_dispatcher_ = std::make_unique<TaskDispatcher>();
  scene_           = std::make_shared<Scene>();
  renderer_        = std::make_shared<Renderer>();

  task_dispatcher_->PushHandler(scene_->GetCamera());

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

  logger_->info("Application initialized");
}

Application::~Application() {
  if (window_) {
    glfwDestroyWindow(window_);
  }
  glfwTerminate();
  logger_->info("Application terminated");
}

void Application::Run() {
  while (!glfwWindowShouldClose(window_)) {
    float dt = GetDeltaTime();

    if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) {
      glfwSetWindowShouldClose(window_, true);
    }

    // handle logic
    glm::vec3 direction(0.0f);
    if (Input::IsKeyPressed(GLFW_KEY_A)) {
      direction += glm::vec3(-1.0f, 0.0f, 0.0f);
    } else if (Input::IsKeyPressed(GLFW_KEY_D)) {
      direction += glm::vec3(1.0f, 0.0f, 0.0f);
    }
    if (Input::IsKeyPressed(GLFW_KEY_W)) {
      direction += glm::vec3(0.0f, 1.0f, 0.0f);
    } else if (Input::IsKeyPressed(GLFW_KEY_S)) {
      direction += glm::vec3(0.0f, -1.0f, 0.0f);
    }
    if (direction != glm::vec3(0.0f)) direction = glm::normalize(direction);
    float       velocity = 1.0f;
    MoveCommand cmd(direction, velocity * dt);

    task_dispatcher_->Run(&cmd);

    // handle render
    auto entities = scene_->GetAllEntitiesWith<RenderInfo>();
    auto camera   = scene_->GetCamera();

    camera->OnWindowResize(viewport_width_, viewport_height_);

    // TODO: do not control camera zoom level by keyboard.
    if (Input::IsKeyPressed(GLFW_KEY_UP)) {
      camera->OnMouseScroll(1.0f * dt);
    } else if (Input::IsKeyPressed(GLFW_KEY_DOWN)) {
      camera->OnMouseScroll(-1.0f * dt);
    }

    frame_buffer_->Bind();
    frame_buffer_->Clear();
    frame_buffer_->Resize(viewport_width_, viewport_height_);
    frame_buffer_->CheckStatus();
    frame_buffer_->AttachTexture();
    frame_buffer_->AttachRenderBuffer();

    for (auto& entity : entities) {
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
		static bool dockspaceOpen = true;
		static bool opt_fullscreen_persistant = true;
		bool opt_fullscreen = opt_fullscreen_persistant;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		if (opt_fullscreen)
		{
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}

		// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive, 
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise 
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// DockSpace
		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 370.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
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
    ImVec2 size      = ImGui::GetContentRegionAvail();
    viewport_width_  = size.x;
    viewport_height_ = size.y;
    ImGui::Image((void*)(intptr_t)frame_buffer_->GetTextureId(), size,
                 ImVec2(0, 1), ImVec2(1, 0));
    ImGui::End();

    ImGui::Begin("Scene");
    ImGui::Text("Hello, world!");
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

    glfwSwapBuffers(window_);

    glfwPollEvents();
  }
}

float Application::GetDeltaTime() {
  float current_time = static_cast<float>(glfwGetTime());
  float delta_time   = current_time - prev_time_;
  prev_time_         = current_time;

  frame_count_++;

  if (current_time - frame_time_ >= 1.0f) {
    fps_         = frame_count_;
    frame_count_ = 0;
    frame_time_  = current_time;
  }

  return delta_time;
}

}  // namespace MEngine