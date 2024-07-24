#include "render/shader.hpp"

#include <glad/glad.h>

#include <fstream>

namespace MEngine {

Shader::Shader(const std::string &vert_path, const std::string &frag_path)
    : vert_path_(vert_path), frag_path_(frag_path) {
  logger_                    = Logger::Get("Shader");
  std::vector<char> vert_src = read_file(vert_path);
  std::vector<char> frag_src = read_file(frag_path);

  if (vert_src.empty() || frag_src.empty()) return;

  const char *vertCode = vert_src.data();
  const char *fragCode = frag_src.data();

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
    logger_->critical("Failed compilation for vertex shader! detail:\n{}", infoLog);
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
    logger_->critical("Failed compilation for fragment shader! detail:\n{}", infoLog);
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

  auto pos = vert_path.find_last_of("/\\");
  if (pos == std::string::npos) {
    name_ = vert_path;
  } else {
    name_ = vert_path.substr(pos + 1);
  }
  if (name_.size() > 5) {
    if (name_.substr(name_.size() - 5) == "_vert") {
      name_ = name_.substr(0, name_.size() - 5);
    }
  }
}

Shader::Shader(const std::string &name, const std::string &vert_path, const std::string &frag_path)
    : name_(name), vert_path_(vert_path), frag_path_(frag_path) {
  logger_                    = Logger::Get("Shader");
  std::vector<char> vert_src = read_file(vert_path);
  std::vector<char> frag_src = read_file(frag_path);

  if (vert_src.empty() || frag_src.empty()) return;

  const char *vertCode = vert_src.data();
  const char *fragCode = frag_src.data();

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
    logger_->critical("Failed compilation for vertex shader! detail:\n{}", infoLog);
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
    logger_->critical("Failed compilation for fragment shader! detail:\n{}", infoLog);
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

std::vector<char> Shader::read_file(const std::string &path) {
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

ShaderLibrary::ShaderLibrary() { logger_ = Logger::Get("ShaderLibrary"); }

ShaderLibrary::~ShaderLibrary() {}

void ShaderLibrary::Add(const std::string &name, const std::shared_ptr<Shader> &shader) {
  if (Exists(name)) {
    logger_->warn("Shader already exists!");
  }
  shaders_[name] = shader;
}

void ShaderLibrary::Add(const std::shared_ptr<Shader> &shader) {
  auto &name = shader->GetName();
  Add(name, shader);
}

std::shared_ptr<Shader> ShaderLibrary::Load(const std::string &name, const std::string &vert_path,
                                            const std::string &frag_path) {
  auto shader = std::make_shared<Shader>(name, vert_path, frag_path);
  Add(shader);
  return shader;
}

std::shared_ptr<Shader> ShaderLibrary::Get(const std::string &name) { return shaders_[name]; }

bool ShaderLibrary::Exists(const std::string &name) const { return shaders_.find(name) != shaders_.end(); }

}  // namespace MEngine
