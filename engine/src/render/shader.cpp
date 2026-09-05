#include "render/shader.hpp"

#include <fstream>
#include <utility>
#include "render/rhi/resource_backend.hpp"
#include "core/logger.hpp"

namespace MEngine {

namespace {
IShaderBackend *g_last_bound_shader_backend = nullptr;
}

Shader::Shader(const std::string &vert_path, const std::string &frag_path)
    : vert_path_(vert_path), frag_path_(frag_path) {
  backend_ = CreateShaderBackend(vert_path, frag_path);

  auto pos = vert_path.find_last_of("/\\");
  if (pos == std::string::npos) {
    name_ = vert_path;
  } else {
    name_ = vert_path.substr(pos + 1);
  }
  if (name_.size() > 5) {
    if (name_.substr(name_.size() - 5) == "_vert") {
      name_ = name_.substr(0, name_.size() - 5);
    }
  }
}

Shader::Shader(std::string name, const std::string &vert_path, const std::string &frag_path)
    : name_(std::move(name)), vert_path_(vert_path), frag_path_(frag_path) {
  backend_ = CreateShaderBackend(vert_path, frag_path);
}

Shader::~Shader() = default;

void Shader::Bind() const {
  if (backend_) {
    backend_->Bind();
    g_last_bound_shader_backend = backend_.get();
  }
}

void Shader::Unbind() {
  if (g_last_bound_shader_backend) {
    g_last_bound_shader_backend->Unbind();
  }
}

void Shader::SetUniform(const std::string &name, int value) {
  if (backend_) {
    backend_->SetUniformInt(name, value);
  }
}

void Shader::SetUniform(const std::string &name, float value) {
  if (backend_) {
    backend_->SetUniformFloat(name, value);
  }
}

void Shader::SetUniform(const std::string &name, const glm::vec2 &value) {
  if (backend_) {
    backend_->SetUniformVec2(name, value);
  }
}

void Shader::SetUniform(const std::string &name, const glm::vec3 &value) {
  if (backend_) {
    backend_->SetUniformVec3(name, value);
  }
}

void Shader::SetUniform(const std::string &name, const glm::vec4 &value) {
  if (backend_) {
    backend_->SetUniformVec4(name, value);
  }
}

void Shader::SetUniform(const std::string &name, const glm::mat4 &value) {
  if (backend_) {
    backend_->SetUniformMat4(name, value);
  }
}

std::vector<char> Shader::read_file(const std::string &path) {
  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    LOG_FATAL("Shader") << "Can't open file " << path;
    file.close();
    return {};
  }

  size_t size = file.tellg();

  std::vector<char> buffer(size + 1);

  file.seekg(0);
  file.read(buffer.data(), static_cast<std::streamsize>(size));
  buffer[size] = '\0';

  return buffer;
}

ShaderLibrary::ShaderLibrary() = default;

ShaderLibrary::~ShaderLibrary() = default;

void ShaderLibrary::Add(const std::string &name, const Ref<Shader> &shader) {
  if (Exists(name)) {
    LOG_WARN("ShaderLibrary") << "Shader already exists!";
  }
  shaders_[name] = shader;
}

void ShaderLibrary::Add(const Ref<Shader> &shader) {
  auto &name = shader->GetName();
  Add(name, shader);
}

Ref<Shader> ShaderLibrary::Load(const std::string &name, const std::string &vert_path, const std::string &frag_path) {
  auto shader = CreateRef<Shader>(name, vert_path, frag_path);
  Add(shader);
  LOG_DEBUG("ShaderLibrary") << "Loaded shader '" << name << "'";
  return shader;
}

Ref<Shader> ShaderLibrary::Get(const std::string &name) { return shaders_[name]; }

bool ShaderLibrary::Exists(const std::string &name) const { return shaders_.find(name) != shaders_.end(); }

}  // namespace MEngine
