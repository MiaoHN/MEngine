#include "shader.hpp"

#include <glad/glad.h>

#include <fstream>

namespace MEngine {

Shader::Shader(const std::string& vert_path, const std::string& frag_path) {
  logger_                    = Logger::Get("Shader");
  std::vector<char> vert_src = read_file(vert_path);
  std::vector<char> frag_src = read_file(frag_path);

  if (vert_src.empty() || frag_src.empty()) return;

  const char* vertCode = vert_src.data();
  const char* fragCode = frag_src.data();

  // 创建顶点着色器
  unsigned int vert_shader = glCreateShader(GL_VERTEX_SHADER);
  // 将着色器源码附加到着色器对象
  glShaderSource(vert_shader, 1, &vertCode, nullptr);
  glCompileShader(vert_shader);
  int  success;
  char infoLog[512];
  glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vert_shader, 512, nullptr, infoLog);
    logger_->critical("Failed compilation for vertex shader! detail:\n{}",
                      infoLog);
    glDeleteShader(vert_shader);
    return;
  }

  // 创建片段着色器
  unsigned int frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(frag_shader, 1, &fragCode, nullptr);
  glCompileShader(frag_shader);
  glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(frag_shader, 512, nullptr, infoLog);
    logger_->critical("Failed compilation for fragment shader! detail:\n{}",
                      infoLog);
    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);
    return;
  }

  // 创建程序对象
  unsigned int shader_program = glCreateProgram();
  // 将之前编译的着色器附加到程序对象上
  glAttachShader(shader_program, vert_shader);
  glAttachShader(shader_program, frag_shader);
  glLinkProgram(shader_program);
  glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(shader_program, 512, nullptr, infoLog);
    logger_->critical("Failed link for program! detail:\n{}", infoLog);
    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);
    glDeleteProgram(shader_program);
    return;
  }
  glDeleteShader(vert_shader);
  glDeleteShader(frag_shader);
  id_ = shader_program;
}

Shader::~Shader() { glDeleteProgram(id_); }

void Shader::Bind() { glUseProgram(id_); }

void Shader::Unbind() { glUseProgram(0); }

std::vector<char> Shader::read_file(const std::string& path) {
  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    Logger::Get("Shader")->critical("read_file Can't open file '{}'", path.c_str());
    file.close();
    return {};
  }

  size_t size = file.tellg();

  std::vector<char> buffer(size + 1);

  file.seekg(0);
  file.read(buffer.data(), size);
  buffer[size] = '\0';

  return buffer;
}

}  // namespace MEngine
