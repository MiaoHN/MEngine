#include "render/texture.hpp"

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace MEngine {

Texture::Texture(const std::string &path) : path_(path) {
  glGenTextures(1, &id_);
  glBindTexture(GL_TEXTURE_2D, id_);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  unsigned char *data = stbi_load(path.c_str(), &width_, &height_, &channels_, 0);
  if (data) {
    GLenum format;
    if (channels_ == 1) {
      format = GL_RED;
    } else if (channels_ == 3) {
      format = GL_RGB;
    } else if (channels_ == 4) {
      format = GL_RGBA;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, format, width_, height_, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    LOG_ERROR("Texture") << "Failed to load texture: " << path;
  }

  stbi_image_free(data);

  size_t last_slash = path.find_last_of("/\\");
  size_t last_dot   = path.find_last_of(".");
  name_             = path.substr(last_slash + 1, last_dot - last_slash - 1);
}

Texture::Texture(const std::string &name, const std::string &path) {
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

    glTexImage2D(GL_TEXTURE_2D, 0, format, width_, height_, 0, format, GL_UNSIGNED_BYTE, data_);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    LOG_ERROR("Texture") << "Failed to load texture: " << path;
  }

  name_ = name;
}

Texture::Texture() {
  glGenTextures(1, &id_);
  glBindTexture(GL_TEXTURE_2D, id_);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

Texture::~Texture() {
  glDeleteTextures(1, &id_);
  stbi_image_free(data_);
}

void Texture::SetData(unsigned char *data, int width, int height) {
  data_     = data;
  width_    = width;
  height_   = height;
  channels_ = 4;

  glBindTexture(GL_TEXTURE_2D, id_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, data_);
  glGenerateMipmap(GL_TEXTURE_2D);
}

void Texture::Bind(unsigned int slot) const {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, id_);
}

void Texture::Unbind() const { glBindTexture(GL_TEXTURE_2D, 0); }

void Texture::SetSubTexture(int frame) {
  int w = width_ / h_frames_;
  int h = height_ / v_frames_;
  int x = frame % h_frames_;
  int y = (frame / h_frames_) % v_frames_;

  std::vector<glm::vec2> uv = {
      {x * w, y * h + h},
      {x * w + w, y * h + h},
      {x * w + w, y * h},
      {x * w, y * h},
  };

  glBindTexture(GL_TEXTURE_2D, id_);
  void *gpu_buffer = nullptr;
  gpu_buffer       = glMapBufferRange(GL_ARRAY_BUFFER, 0, sizeof(uv), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
  memcpy(gpu_buffer, uv.data(), sizeof(uv));
  glUnmapBuffer(GL_ARRAY_BUFFER);
  // glTexSubImage2D(GL_TEXTURE_2D, 0, x * w, y * h, w, h, GL_RGBA,
  //                 GL_UNSIGNED_BYTE, data_);
  // glTextureSubImage2D(id_, 0, x * w, y * h, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
  //                     data_);
}

Ref<Texture> Texture::Create(const std::string &path) { return CreateRef<Texture>(path); }

TextureLibrary::TextureLibrary() {}

TextureLibrary::~TextureLibrary() {}

void TextureLibrary::Add(const std::string &name, const Ref<Texture> &texture) {
  if (Exists(name)) {
    LOG_WARN("TextureLibrary") << "Texture already exists!";
  }
  textures_[name] = texture;
}

void TextureLibrary::Add(const Ref<Texture> &texture) {
  auto &name = texture->GetName();
  Add(name, texture);
}

Ref<Texture> TextureLibrary::Load(const std::string &name, const std::string &path) {
  auto texture = CreateRef<Texture>(name, path);
  Add(texture);
  return texture;
}

Ref<Texture> TextureLibrary::Get(const std::string &name) { return textures_[name]; }

bool TextureLibrary::Exists(const std::string &name) const { return textures_.find(name) != textures_.end(); }

}  // namespace MEngine