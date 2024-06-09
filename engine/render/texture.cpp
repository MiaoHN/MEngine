#include "render/texture.hpp"

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace MEngine {

Texture::Texture(const std::string& path) : path_(path) {
  logger_ = Logger::Get("Texture");

  glGenTextures(1, &id_);
  glBindTexture(GL_TEXTURE_2D, id_);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  unsigned char* data =
      stbi_load(path.c_str(), &width_, &height_, &channels_, 0);
  if (data) {
    GLenum format;
    if (channels_ == 1) {
      format = GL_RED;
    } else if (channels_ == 3) {
      format = GL_RGB;
    } else if (channels_ == 4) {
      format = GL_RGBA;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, format, width_, height_, 0, format,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    logger_->error("Failed to load texture: {0}", path);
  }

  stbi_image_free(data);

  size_t last_slash = path.find_last_of("/\\");
  size_t last_dot   = path.find_last_of(".");
  name_             = path.substr(last_slash + 1, last_dot - last_slash - 1);
}

Texture::Texture(const std::string& name, const std::string& path) {
  logger_ = Logger::Get("Texture");

  glGenTextures(1, &id_);
  glBindTexture(GL_TEXTURE_2D, id_);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  stbi_set_flip_vertically_on_load(true);

  data_ = stbi_load(path.c_str(), &width_, &height_, &channels_, 0);
  if (data_) {
    GLenum format;
    if (channels_ == 1) {
      format = GL_RED;
    } else if (channels_ == 3) {
      format = GL_RGB;
    } else if (channels_ == 4) {
      format = GL_RGBA;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, format, width_, height_, 0, format,
                 GL_UNSIGNED_BYTE, data_);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    logger_->error("Failed to load texture: {0}", path);
  }

  name_ = name;
}

Texture::~Texture() {
  glDeleteTextures(1, &id_);
  stbi_image_free(data_);
}

void Texture::Bind(unsigned int slot) const {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, id_);
}

void Texture::Unbind() const { glBindTexture(GL_TEXTURE_2D, 0); }

void Texture::SetSubTexture(int frame) {
  static int w = width_ / h_frames_;
  static int h = height_ / v_frames_;
  float      x = (frame % h_frames_) / (float)h_frames_;
  float      y = (frame / h_frames_) / (float)v_frames_;

  glBindTexture(GL_TEXTURE_2D, id_);
  glTexSubImage2D(GL_TEXTURE_2D, 0, x * w, y * h, w, h, GL_RGBA,
                  GL_UNSIGNED_BYTE, data_);
  glBindTexture(GL_TEXTURE_2D, 0);
}

TextureLibrary::TextureLibrary() { logger_ = Logger::Get("TextureLibrary"); }

TextureLibrary::~TextureLibrary() {}

void TextureLibrary::Add(const std::string&              name,
                         const std::shared_ptr<Texture>& texture) {
  if (Exists(name)) {
    logger_->warn("Texture already exists!");
  }
  textures_[name] = texture;
}

void TextureLibrary::Add(const std::shared_ptr<Texture>& texture) {
  auto& name = texture->GetName();
  Add(name, texture);
}

std::shared_ptr<Texture> TextureLibrary::Load(const std::string& name,
                                              const std::string& path) {
  auto texture = std::make_shared<Texture>(name, path);
  Add(texture);
  return texture;
}

std::shared_ptr<Texture> TextureLibrary::Get(const std::string& name) {
  return textures_[name];
}

bool TextureLibrary::Exists(const std::string& name) const {
  return textures_.find(name) != textures_.end();
}

}  // namespace MEngine