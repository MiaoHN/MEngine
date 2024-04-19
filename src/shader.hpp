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

 private:
  unsigned int id_;

  static std::vector<char> read_file(const std::string& path);

  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace MEngine
