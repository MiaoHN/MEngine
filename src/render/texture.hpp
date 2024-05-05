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

 private:
  unsigned int id_;
  int          width_;
  int          height_;
  int          channels_;

  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace MEngine