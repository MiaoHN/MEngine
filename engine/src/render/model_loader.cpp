#include "render/model_loader.hpp"

#include <array>
#include <fstream>
#include <functional>
#include <sstream>
#include <unordered_map>

#include <glm/glm.hpp>

#include "core/logger.hpp"
#include "render/mesh.hpp"

namespace MEngine {

namespace {

// A face corner references positions/texcoords/normals by (1-based) OBJ index.
// A value of 0 means "not provided".
struct Corner {
  int p = 0;
  int t = 0;
  int n = 0;

  bool operator==(const Corner &other) const { return p == other.p && t == other.t && n == other.n; }
};

struct CornerHash {
  size_t operator()(const Corner &c) const {
    return std::hash<int>()(c.p) ^ (std::hash<int>()(c.t) << 1) ^ (std::hash<int>()(c.n) << 2);
  }
};

int ToInt(const std::string &field) { return field.empty() ? 0 : std::stoi(field); }

// Parses a face corner token: "v", "v/vt", "v//vn" or "v/vt/vn".
Corner ParseCorner(const std::string &token) {
  std::vector<std::string> fields;
  size_t                   start = 0;
  while (true) {
    const size_t pos   = token.find('/', start);
    const std::string field = (pos == std::string::npos) ? token.substr(start) : token.substr(start, pos - start);
    fields.push_back(field);
    if (pos == std::string::npos) {
      break;
    }
    start = pos + 1;
  }

  Corner corner;
  corner.p = ToInt(fields[0]);
  if (fields.size() == 2) {
    corner.t = ToInt(fields[1]);  // v/vt
  } else if (fields.size() >= 3) {
    corner.t = ToInt(fields[1]);  // v/vt/vn or v//vn (empty middle -> 0)
    corner.n = ToInt(fields[2]);
  }
  return corner;
}

// Resolves a 1-based OBJ index (negative indices are relative to the end).
int ResolveIndex(int raw, size_t count) {
  if (raw > 0) {
    return raw - 1;
  }
  if (raw < 0) {
    return static_cast<int>(count) + raw;
  }
  return 0;
}

}  // namespace

Ref<Mesh> ModelLoader::LoadObj(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("ModelLoader") << "Failed to open model file: " << path;
    return nullptr;
  }

  std::vector<glm::vec3> positions;
  std::vector<glm::vec2> texcoords;
  std::vector<glm::vec3> normals;

  std::vector<std::array<Corner, 3>> triangles;

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    std::istringstream iss(line);
    std::string       type;
    iss >> type;

    if (type == "v") {
      float x = 0.0f, y = 0.0f, z = 0.0f;
      iss >> x >> y >> z;
      positions.emplace_back(x, y, z);
    } else if (type == "vt") {
      float u = 0.0f, v = 0.0f;
      iss >> u >> v;
      texcoords.emplace_back(u, v);
    } else if (type == "vn") {
      float x = 0.0f, y = 0.0f, z = 0.0f;
      iss >> x >> y >> z;
      normals.emplace_back(x, y, z);
    } else if (type == "f") {
      std::vector<Corner> corners;
      std::string         token;
      while (iss >> token) {
        corners.push_back(ParseCorner(token));
      }

      // Fan-triangulate n-gons.
      for (size_t i = 1; i + 1 < corners.size(); ++i) {
        triangles.push_back({corners[0], corners[i], corners[i + 1]});
      }
    }
  }

  if (triangles.empty()) {
    LOG_WARN("ModelLoader") << "No faces found in OBJ file: " << path;
    return nullptr;
  }

  std::vector<Vertex>         vertices;
  std::vector<unsigned int>   indices;
  vertices.reserve(triangles.size() * 3);
  indices.reserve(triangles.size() * 3);

  if (!normals.empty()) {
    // Dedup corners that share position/texcoord/normal indices.
    std::unordered_map<Corner, unsigned int, CornerHash> cache;
    for (const auto &tri : triangles) {
      for (const Corner &corner : tri) {
        const auto it = cache.find(corner);
        if (it != cache.end()) {
          indices.push_back(it->second);
          continue;
        }

        Vertex vertex;
        vertex.position = positions[static_cast<size_t>(ResolveIndex(corner.p, positions.size()))];
        vertex.texcoord = corner.t != 0
                              ? texcoords[static_cast<size_t>(ResolveIndex(corner.t, texcoords.size()))]
                              : glm::vec2(0.0f);
        vertex.normal = normals[static_cast<size_t>(ResolveIndex(corner.n, normals.size()))];

        const unsigned int index = static_cast<unsigned int>(vertices.size());
        vertices.push_back(vertex);
        cache[corner] = index;
        indices.push_back(index);
      }
    }
  } else {
    // No normals in file: emit flat-shaded triangles (3 fresh vertices each).
    for (const auto &tri : triangles) {
      const glm::vec3 &p0 = positions[static_cast<size_t>(ResolveIndex(tri[0].p, positions.size()))];
      const glm::vec3 &p1 = positions[static_cast<size_t>(ResolveIndex(tri[1].p, positions.size()))];
      const glm::vec3 &p2 = positions[static_cast<size_t>(ResolveIndex(tri[2].p, positions.size()))];
      const glm::vec3 face_normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));

      for (const Corner &corner : tri) {
        Vertex vertex;
        vertex.position = positions[static_cast<size_t>(ResolveIndex(corner.p, positions.size()))];
        vertex.texcoord = corner.t != 0
                              ? texcoords[static_cast<size_t>(ResolveIndex(corner.t, texcoords.size()))]
                              : glm::vec2(0.0f);
        vertex.normal   = face_normal;
        indices.push_back(static_cast<unsigned int>(vertices.size()));
        vertices.push_back(vertex);
      }
    }
  }

  LOG_INFO("ModelLoader") << "Loaded OBJ '" << path << "': " << vertices.size() << " vertices, " << indices.size()
                          << " indices.";
  return Mesh::Create(vertices, indices);
}

}  // namespace MEngine
