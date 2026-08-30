#include "render/mesh.hpp"

#include <cmath>

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

Ref<Mesh> Mesh::CreatePlane(float size) {
  const float h = size * 0.5f;

  const std::vector<Vertex> vertices = {
      {{-h, 0.0f, -h}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
      {{h, 0.0f, -h}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
      {{h, 0.0f, h}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
      {{-h, 0.0f, h}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
  };
  const std::vector<unsigned int> indices = {0, 1, 2, 2, 3, 0};

  return Create(vertices, indices);
}

Ref<Mesh> Mesh::CreateSphere(float radius, int segments) {
  constexpr float kPi = 3.14159265358979323846f;

  std::vector<Vertex>         vertices;
  std::vector<unsigned int>   indices;

  const int rings   = segments;
  const int sectors = segments * 2;

  vertices.reserve(static_cast<size_t>(rings + 1) * (sectors + 1));
  indices.reserve(static_cast<size_t>(rings) * sectors * 6);

  for (int r = 0; r <= rings; ++r) {
    const float phi        = kPi * static_cast<float>(r) / static_cast<float>(rings);  // 0 .. pi
    const float y          = std::cos(phi);
    const float ring_radius = std::sin(phi);

    for (int s = 0; s <= sectors; ++s) {
      const float theta = 2.0f * kPi * static_cast<float>(s) / static_cast<float>(sectors);
      const glm::vec3 n(std::cos(theta) * ring_radius, y, std::sin(theta) * ring_radius);

      vertices.push_back({n * radius, n,
                          {static_cast<float>(s) / static_cast<float>(sectors),
                           static_cast<float>(r) / static_cast<float>(rings)}});
    }
  }

  for (int r = 0; r < rings; ++r) {
    for (int s = 0; s < sectors; ++s) {
      const unsigned int a = static_cast<unsigned int>(r * (sectors + 1) + s);
      const unsigned int b = a + static_cast<unsigned int>(sectors) + 1;

      indices.push_back(a);
      indices.push_back(b);
      indices.push_back(a + 1);

      indices.push_back(a + 1);
      indices.push_back(b);
      indices.push_back(b + 1);
    }
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
