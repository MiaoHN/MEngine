#include "render/mesh.hpp"

#include "core/logger.hpp"
#include "render/rhi/resource_backend.hpp"

namespace MEngine {

Mesh::Mesh(const std::vector<Vertex> &vertices, const std::vector<unsigned int> &indices)
    : vertices_(vertices), indices_(indices), index_count_(static_cast<int>(indices.size())) {
  vao_ = CreateVertexArrayBackend();
  vao_->SetVertexBuffer(vertices_.data(), vertices_.size() * sizeof(Vertex), Vertex::GetLayout());
  vao_->SetIndexBuffer(indices_.data(), index_count_);
}

Mesh::~Mesh() = default;

void Mesh::Bind() const { vao_->Bind(); }

void Mesh::Unbind() const { vao_->Unbind(); }

Ref<Mesh> Mesh::Create(const std::vector<Vertex> &vertices, const std::vector<unsigned int> &indices) {
  return CreateRef<Mesh>(vertices, indices);
}

Ref<Mesh> Mesh::CreateCube(float size) {
  const float h = size * 0.5f;

  std::vector<Vertex> vertices;
  vertices.reserve(24);

  // Each face: 4 vertices with an outward-facing normal and a full 0..1 UV.
  auto push_face = [&vertices](const glm::vec3 &n, const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c,
                               const glm::vec3 &d) {
    vertices.push_back({a, n, {0.0f, 0.0f}});
    vertices.push_back({b, n, {1.0f, 0.0f}});
    vertices.push_back({c, n, {1.0f, 1.0f}});
    vertices.push_back({d, n, {0.0f, 1.0f}});
  };

  // +X
  push_face({1.0f, 0.0f, 0.0f}, {h, -h, -h}, {h, h, -h}, {h, h, h}, {h, -h, h});
  // -X
  push_face({-1.0f, 0.0f, 0.0f}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}, {-h, -h, -h});
  // +Y (up)
  push_face({0.0f, 1.0f, 0.0f}, {-h, h, -h}, {h, h, -h}, {h, h, h}, {-h, h, h});
  // -Y (down)
  push_face({0.0f, -1.0f, 0.0f}, {-h, -h, h}, {h, -h, h}, {h, -h, -h}, {-h, -h, -h});
  // +Z
  push_face({0.0f, 0.0f, 1.0f}, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h});
  // -Z
  push_face({0.0f, 0.0f, -1.0f}, {h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h});

  std::vector<unsigned int> indices;
  indices.reserve(36);
  for (unsigned int face = 0; face < 6; ++face) {
    const unsigned int base = face * 4;
    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
    indices.push_back(base + 0);
  }

  return Create(vertices, indices);
}

void MeshLibrary::Add(const std::string &name, const Ref<Mesh> &mesh) {
  if (Exists(name)) {
    LOG_WARN("MeshLibrary") << "Mesh already exists!";
  }
  meshes_[name] = mesh;
}

Ref<Mesh> MeshLibrary::Get(const std::string &name) { return meshes_[name]; }

bool MeshLibrary::Exists(const std::string &name) const { return meshes_.find(name) != meshes_.end(); }

}  // namespace MEngine
