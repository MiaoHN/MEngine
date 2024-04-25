/**
 * @file shader.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-19
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "logger.hpp"

namespace MEngine {

class Shader {
 public:
  Shader(const std::string& vert_path, const std::string& frag_path);
  ~Shader();

  void Bind();
  void Unbind();

  template <typename T>
  void SetUniform(const std::string& name, T value) {
    logger_->error("SetUniform not implemented for this type");
  }

  template <>
  void SetUniform<int>(const std::string& name, int value) {
    Bind();
    int location = glGetUniformLocation(id_, name.c_str());
    glUniform1i(location, value);
  }

  template <>
  void SetUniform<float>(const std::string& name, float value) {
    Bind();
    int location = glGetUniformLocation(id_, name.c_str());
    glUniform1f(location, value);
  }

  template <>
  void SetUniform<glm::vec2>(const std::string& name, glm::vec2 value) {
    Bind();
    int location = glGetUniformLocation(id_, name.c_str());
    glUniform2f(location, value.x, value.y);
  }

  template <>
  void SetUniform<glm::vec3>(const std::string& name, glm::vec3 value) {
    Bind();
    int location = glGetUniformLocation(id_, name.c_str());
    glUniform3f(location, value.x, value.y, value.z);
  }

  template <>
  void SetUniform<glm::vec4>(const std::string& name, glm::vec4 value) {
    Bind();
    int location = glGetUniformLocation(id_, name.c_str());
    glUniform4f(location, value.x, value.y, value.z, value.w);
  }

  template <>
  void SetUniform<glm::mat4>(const std::string& name, glm::mat4 value) {
    Bind();
    int location = glGetUniformLocation(id_, name.c_str());
    glUniformMatrix4fv(location, 1, GL_FALSE, &value[0][0]);
  }

 private:
  unsigned int id_;

  static std::vector<char> read_file(const std::string& path);

  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace MEngine
