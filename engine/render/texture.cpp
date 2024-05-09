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
}

Texture::~Texture() { glDeleteTextures(1, &id_); }

void Texture::Bind(unsigned int slot) const {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, id_);
}

void Texture::Unbind() const { glBindTexture(GL_TEXTURE_2D, 0); }

}  // namespace MEngine