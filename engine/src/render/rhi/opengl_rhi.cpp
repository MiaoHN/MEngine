#include "render/rhi/opengl_rhi.hpp"

#include <glad/glad.h>

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "core/logger.hpp"

namespace MEngine {

void OpenGLRHI::SetupWindowHints() const {
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

bool OpenGLRHI::Initialize(GLFWwindow *window) {
  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
    LOG_ERROR("RHI") << "Failed to load OpenGL function pointers via GLAD";
    return false;
  }

  // Seamless cubemap sampling avoids visible seams between cube-map faces
  // (skybox and point-light shadow maps).
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  LOG_DEBUG("RHI") << "Enabled seamless cubemap sampling";

  LOG_INFO("RHI") << "OpenGL Version: " << glGetString(GL_VERSION);
  LOG_INFO("RHI") << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION);
  LOG_INFO("RHI") << "Vendor: " << glGetString(GL_VENDOR);
  LOG_INFO("RHI") << "Renderer: " << glGetString(GL_RENDERER);

  return true;
}

void OpenGLRHI::BeginFrame(const glm::vec4 &clear_color) const {
  glEnable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRHI::EndFrame(GLFWwindow *window) const { glfwSwapBuffers(window); }

bool OpenGLRHI::InitializeImGuiBackend(GLFWwindow *window) {
  const bool ok = ImGui_ImplGlfw_InitForOpenGL(window, true) && ImGui_ImplOpenGL3_Init("#version 330");
  if (ok) {
    LOG_DEBUG("RHI") << "ImGui OpenGL3 backend initialized";
  }
  return ok;
}

void OpenGLRHI::ShutdownImGuiBackend() const {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
}

void OpenGLRHI::BeginImGuiFrame() const {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
}

void OpenGLRHI::RenderImGuiDrawData(ImDrawData *draw_data) const { ImGui_ImplOpenGL3_RenderDrawData(draw_data); }

void OpenGLRHI::DrawIndexedTriangles(int index_count) const {
  glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, nullptr);
}

void OpenGLRHI::DrawIndexedInstanced(int index_count, int instance_count) const {
  if (instance_count <= 0) {
    return;
  }
  glDrawElementsInstanced(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, nullptr, instance_count);
}

void OpenGLRHI::SetWireframe(bool wireframe) const {
  glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
}

unsigned int OpenGLRHI::CreateFramebuffer() const {
  unsigned int framebuffer = 0;
  glGenFramebuffers(1, &framebuffer);
  return framebuffer;
}

void OpenGLRHI::DestroyFramebuffer(unsigned int framebuffer) const { glDeleteFramebuffers(1, &framebuffer); }

void OpenGLRHI::BindFramebuffer(unsigned int framebuffer) const { glBindFramebuffer(GL_FRAMEBUFFER, framebuffer); }

void OpenGLRHI::ClearBoundFramebufferColor(const glm::vec4 &clear_color) const {
  glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
  glClear(GL_COLOR_BUFFER_BIT);
}

}  // namespace MEngine