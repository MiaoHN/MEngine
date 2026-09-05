#include "render/texture.hpp"

#include "render/rhi/resource_backend.hpp"

#include "core/logger.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace MEngine {

Texture::Texture(const std::string &path) : path_(path) {
  backend_ = CreateTextureBackend();

  stbi_set_flip_vertically_on_load(true);

  unsigned char *loaded_data = stbi_load(path.c_str(), &width_, &height_, &channels_, 0);
  if (loaded_data) {
    backend_->SetData(loaded_data, width_, height_, channels_);
  } else {
    LOG_WARN("Texture") << "Failed to load texture: " << path << ", using fallback checkerboard.";
    static unsigned char fallback_data[] = {
        255, 0,   255, 255, 0,   0,   0,   255,
        0,   0,   0,   255, 255, 0,   255, 255,
    };
    width_    = 2;
    height_   = 2;
    channels_ = 4;
    backend_->SetData(fallback_data, width_, height_, channels_);
  }

  stbi_image_free(loaded_data);

  size_t last_slash = path.find_last_of("/\\");
  size_t last_dot   = path.find_last_of(".");
  name_             = path.substr(last_slash + 1, last_dot - last_slash - 1);
}

Texture::Texture(const std::string &name, const std::string &path) : path_(path) {
  backend_ = CreateTextureBackend();

  stbi_set_flip_vertically_on_load(true);

  data_ = stbi_load(path.c_str(), &width_, &height_, &channels_, 0);
  if (data_) {
    owns_data_ = true;
    backend_->SetData(data_, width_, height_, channels_);
  } else {
    LOG_WARN("Texture") << "Failed to load texture: " << path << ", using fallback checkerboard.";
    static unsigned char fallback_data[] = {
        255, 0,   255, 255, 0,   0,   0,   255,
        0,   0,   0,   255, 255, 0,   255, 255,
    };
    width_    = 2;
    height_   = 2;
    channels_ = 4;
    backend_->SetData(fallback_data, width_, height_, channels_);
  }

  name_ = name;
}

Texture::Texture() { backend_ = CreateTextureBackend(); }

Texture::~Texture() {
  if (owns_data_ && data_) {
    stbi_image_free(data_);
  }
}

void Texture::SetData(unsigned char *data, int width, int height) {
  if (owns_data_ && data_) {
    stbi_image_free(data_);
  }

  data_     = data;
  owns_data_ = false;
  width_    = width;
  height_   = height;
  channels_ = 4;

  backend_->SetData(data_, width_, height_, channels_);
}

void Texture::Bind(unsigned int slot) const { backend_->Bind(slot); }

void Texture::Unbind() const { backend_->Unbind(); }

void Texture::SetSubTexture(int frame) { backend_->SetSubTexture(frame, h_frames_, v_frames_, width_, height_); }

unsigned int Texture::GetID() const { return backend_ ? backend_->GetID() : 0; }

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