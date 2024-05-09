/**
 * @file texture.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-05-05
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <memory>
#include <string>

#include "core/logger.hpp"

namespace MEngine {

class Texture {
 public:
  Texture(const std::string& path);
  ~Texture();

  void Bind(unsigned int slot = 0) const;
  void Unbind() const;

  int GetWidth() const { return width_; }

  int GetHeight() const { return height_; }

  std::string GetPath() const { return path_; }

  const unsigned int GetID() const { return id_; }

 private:
  unsigned int id_;
  int          width_;
  int          height_;
  int          channels_;

  std::shared_ptr<spdlog::logger> logger_;

  std::string path_;
};

}  // namespace MEngine