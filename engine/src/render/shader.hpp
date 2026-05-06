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

#include "core/common.hpp"

#include "core/logger.hpp"

namespace MEngine {

class IShaderBackend;

class Shader {
 public:
  Shader(const std::string &vert_path, const std::string &frag_path);
  Shader(std::string name, const std::string &vert_path, const std::string &frag_path);
  ~Shader();

  void Bind() const;
  static void Unbind();

  [[nodiscard]] std::string GetVertPath() const { return vert_path_; }

  [[nodiscard]] std::string GetFragPath() const { return frag_path_; }

  [[nodiscard]] const std::string &GetName() const { return name_; }

  void SetUniform(const std::string &name, int value);
  void SetUniform(const std::string &name, float value);
  void SetUniform(const std::string &name, const glm::vec2 &value);
  void SetUniform(const std::string &name, const glm::vec3 &value);
  void SetUniform(const std::string &name, const glm::vec4 &value);
  void SetUniform(const std::string &name, const glm::mat4 &value);


 private:
  std::unique_ptr<IShaderBackend> backend_;

  static std::vector<char> read_file(const std::string &path);

  std::string name_;

  std::string vert_path_;
  std::string frag_path_;
};

class ShaderLibrary {
 public:
  ShaderLibrary();
  ~ShaderLibrary();

  void Add(const std::string &name, const Ref<Shader> &shader);

  void Add(const Ref<Shader> &shader);

  Ref<Shader> Load(const std::string &name, const std::string &vert_path, const std::string &frag_path);

  Ref<Shader> Get(const std::string &name);

  bool Exists(const std::string &name) const;

 private:
  std::unordered_map<std::string, Ref<Shader>> shaders_;
};

}  // namespace MEngine
