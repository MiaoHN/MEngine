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

#include "core/common.hpp"

#include "core/logger.hpp"

namespace MEngine {

class Texture {
 public:
  Texture(const std::string &path);

  Texture(const std::string &name, const std::string &path);

  void SetData(unsigned char *data, int width, int height);

  Texture();

  ~Texture();

  void Bind(unsigned int slot = 0) const;
  void Unbind() const;

  int GetWidth() const { return width_; }

  int GetHeight() const { return height_; }

  void SetVFrames(int v_frames) { v_frames_ = v_frames; }
  void SetHFrames(int h_frames) { h_frames_ = h_frames; }

  void SetSubTexture(int frame = 0);

  const std::string &GetName() const { return name_; }

  std::string GetPath() const { return path_; }

  const unsigned int GetID() const { return id_; }

  static Ref<Texture> Create(const std::string &path);

 private:
  unsigned int id_;
  int          width_;
  int          height_;
  int          channels_;

  int v_frames_ = 1;
  int h_frames_ = 1;

  std::string path_;

  unsigned char *data_;

  std::string name_;
};

class TextureLibrary {
 public:
  TextureLibrary();
  ~TextureLibrary();

  void Add(const std::string &name, const Ref<Texture> &texture);

  void Add(const Ref<Texture> &texture);

  Ref<Texture> Load(const std::string &name, const std::string &path);

  Ref<Texture> Get(const std::string &name);

  bool Exists(const std::string &name) const;

 private:
  std::unordered_map<std::string, Ref<Texture>> textures_;
};

}  // namespace MEngine