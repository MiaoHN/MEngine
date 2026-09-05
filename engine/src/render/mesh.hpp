#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/common.hpp"
#include "render/vertex.hpp"

namespace MEngine {

class IVertexArrayBackend;

/**
 * @brief A GPU mesh: interleaved vertices + index buffer.
 *
 * Keeps a CPU-side copy of the data for re-upload, export and ray/collision
 * queries. The actual GPU storage is abstracted behind `IVertexArrayBackend`,
 * so OpenGL and Vulkan backends can differ while this class stays API-neutral.
 */
class Mesh {
 public:
  Mesh(const std::vector<Vertex> &vertices, const std::vector<unsigned int> &indices);
  ~Mesh();

  void Bind() const;
  void Unbind() const;

  [[nodiscard]] int GetIndexCount() const { return index_count_; }
  [[nodiscard]] const std::vector<Vertex> &GetVertices() const { return vertices_; }
  [[nodiscard]] const std::vector<unsigned int> &GetIndices() const { return indices_; }

  /// @brief Object-space AABB of the vertices (computed once at construction).
  /// `out_min`/`out_max` are filled with the box corners; returns false when the
  /// mesh has no vertices (callers should then treat it as always visible).
  [[nodiscard]] bool GetLocalBounds(glm::vec3 &out_min, glm::vec3 &out_max) const {
    out_min = local_min_;
    out_max = local_max_;
    return has_local_bounds_;
  }

  static Ref<Mesh> Create(const std::vector<Vertex> &vertices, const std::vector<unsigned int> &indices);

  /// @brief Source description used by scene serialization: "cube", "plane"
  /// or "sphere" for built-in primitives, or a model file path.
  void SetSource(const std::string &source) { source_ = source; }
  [[nodiscard]] const std::string &GetSource() const { return source_; }

  /// @brief Unit cube with per-face normals and UVs, centered at origin.
  static Ref<Mesh> CreateCube(float size = 1.0f);

  /// @brief Axis-aligned quad in the XZ plane (normal +Y), centered at origin.
  static Ref<Mesh> CreatePlane(float size = 1.0f);

  /// @brief UV sphere centered at origin, with `segments` rings/sectors.
  static Ref<Mesh> CreateSphere(float radius = 0.5f, int segments = 32);

 private:
  std::unique_ptr<IVertexArrayBackend> vao_;

  std::vector<Vertex>         vertices_;
  std::vector<unsigned int>   indices_;
  int                         index_count_ = 0;

  std::string source_;

  glm::vec3 local_min_{0.0f};
  glm::vec3 local_max_{0.0f};
  bool      has_local_bounds_ = false;
};

/// @brief Name-based mesh cache (mirrors ShaderLibrary / TextureLibrary).
class MeshLibrary {
 public:
  void Add(const std::string &name, const Ref<Mesh> &mesh);

  Ref<Mesh> Get(const std::string &name);

  [[nodiscard]] bool Exists(const std::string &name) const;

 private:
  std::unordered_map<std::string, Ref<Mesh>> meshes_;
};

}  // namespace MEngine
