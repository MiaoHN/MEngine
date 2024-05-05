#include "core/application.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include <backends/imgui_impl_opengl3.cpp>
#include <backends/imgui_impl_glfw.cpp>

#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/command.hpp"
#include "core/input.hpp"
#include "core/script_engine.hpp"
#include "core/task_dispatcher.hpp"
#include "core/task_handler.hpp"
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
  prev_time_ = glfwGetTime();
  frame_time_ = glfwGetTime();
  frame_count_ = 0;
  fps_ = 0;

  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  window_ = glfwCreateWindow(800, 600, "MEngine", nullptr, nullptr);

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
  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init("#version 330");

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
    int  width, height;
    glfwGetFramebufferSize(window_, &width, &height);

    glViewport(0, 0, width, height);
    camera->OnWindowResize(width, height);

    // TODO: do not control camera zoom level by keyboard.
    if (Input::IsKeyPressed(GLFW_KEY_UP)) {
      camera->OnMouseScroll(1.0f * dt);
    } else if (Input::IsKeyPressed(GLFW_KEY_DOWN)) {
      camera->OnMouseScroll(-1.0f * dt);
    }

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

    // ImGui test
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // print fps
    ImGui::Begin("Information");
    ImGui::Text("FPS: %d", GetFPS());
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
    fps_ = frame_count_;
    frame_count_ = 0;
    frame_time_  = current_time;
  }

  return delta_time;
}

}  // namespace MEngine