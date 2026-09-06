#include "render/model_loader.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <unordered_map>

#include <glm/glm.hpp>

#include "core/logger.hpp"
#include "render/material.hpp"
#include "render/mesh.hpp"
#include "render/texture.hpp"

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

Ref<Material> ModelLoader::LoadObjMaterial(const std::string &obj_path) {
  // Read the `mtllib` line(s) of the OBJ and open the first file that exists.
  const std::filesystem::path obj_dir = std::filesystem::path(obj_path).parent_path();
  std::ifstream               obj(obj_path);
  if (!obj.is_open()) {
    return nullptr;
  }
  std::string mtl_rel;
  std::string line;
  while (std::getline(obj, line)) {
    std::istringstream iss(line);
    std::string        type;
    iss >> type;
    if (type == "mtllib") {
      std::string name;
      iss >> name;
      if (std::filesystem::exists(obj_dir / name)) {
        mtl_rel = name;
        break;
      }
    }
  }
  if (mtl_rel.empty()) {
    return nullptr;
  }

  std::ifstream mtl(obj_dir / mtl_rel);
  if (!mtl.is_open()) {
    return nullptr;
  }

  // Reads a texture file name, skipping option tokens like `-bm 1.0`.
  const auto read_texture_name = [](std::istringstream &s) -> std::string {
    std::string tok;
    if (!(s >> tok)) return "";
    while (tok.size() > 1 && tok[0] == '-') {
      std::string dummy;
      if (!(s >> dummy) || !(s >> tok)) return "";
    }
    return tok;
  };

  Ref<Material> material = CreateRef<Material>();
  material->SetBaseColorFactor(glm::vec4(1.0f));
  material->SetMetallicFactor(0.0f);
  material->SetRoughnessFactor(1.0f);

  glm::vec3   kd(1.0f);
  std::string albedo_rel, normal_rel, spec_rel, refl_rel;
  float       ns = 0.0f;
  bool        started = false;
  std::string line2;
  while (std::getline(mtl, line2)) {
    std::istringstream iss(line2);
    std::string        type;
    iss >> type;
    if (type == "newmtl") {
      // Parse the FIRST material block (multi-material splitting happens in
      // LoadObjModel); stop once a second `newmtl` starts.
      if (started) break;
      started = true;
      continue;
    }
    if (type == "Ns") {
      iss >> ns;
    }
    if (type == "Kd") {
      float r = 1.0f, g = 1.0f, b = 1.0f;
      if (iss >> r >> g >> b) {
        kd = glm::vec3(r, g, b);
      }
    } else if (type == "map_Kd") {
      albedo_rel = read_texture_name(iss);
    } else if (type == "map_Bump" || type == "map_Kn" || type == "norm" || type == "bump") {
      if (normal_rel.empty()) normal_rel = read_texture_name(iss);
    } else if (type == "map_Ks") {
      spec_rel = read_texture_name(iss);
    } else if (type == "map_Ka") {
      refl_rel = read_texture_name(iss);
    }
  }

  // Kd is the base colour when no map_Kd overrides it (Blender exports both,
  // and multiplying albedo by Kd too would darken the texture).
  if (albedo_rel.empty()) {
    material->SetBaseColorFactor(glm::vec4(kd, 1.0f));
  }
  const auto resolve = [&](const std::string &rel) -> Ref<Texture> {
    if (rel.empty()) return nullptr;
    const std::filesystem::path p = obj_dir / rel;
    if (!std::filesystem::exists(p)) return nullptr;
    return Texture::Create(p.string());
  };
  if (Ref<Texture> albedo = resolve(albedo_rel)) {
    material->SetAlbedoMap(albedo);
    material->SetAlbedoSRGB(true);  // diffuse maps are sRGB-encoded
  }
  if (Ref<Texture> normal = resolve(normal_rel)) {
    material->SetNormalMap(normal);
  }
  if (Ref<Texture> spec = resolve(spec_rel)) {
    material->SetSpecularMap(spec);
    // Specular-workflow OBJ -> a low-roughness dielectric under PBR.
    material->SetMetallicFactor(0.0f);
    material->SetRoughnessFactor(0.4f);
  }
  if (Ref<Texture> refl = resolve(refl_rel)) {
    material->SetReflectionMap(refl);
  }
  if (ns > 0.0f) {
    material->SetShininess(ns);
  }

  LOG_INFO("ModelLoader") << "Loaded OBJ material from '" << mtl_rel
                           << "' (albedo=" << (albedo_rel.empty() ? std::string("Kd") : albedo_rel)
                           << " normal=" << normal_rel << " specular=" << spec_rel << ")";
  return material;
}

namespace {

/// @brief Parsed .mtl material definition (diffuse colour + texture paths).
struct ObjMtlDef {
  std::string name;
  glm::vec3   kd{1.0f};
  std::string albedo;     // map_Kd
  std::string normal;     // map_Bump / map_Kn / norm / bump
  std::string specular;   // map_Ks
  std::string reflection; // map_Ka (equirect env)
  float       shininess = 0.0f;  // Ns
};

/// @brief Last whitespace-separated token of a stream - the texture file name
/// once option groups such as `-bm 1.0` / `-s u v w` are skipped.
std::string ReadMapPath(std::istringstream &s) {
  std::string last, tok;
  while (s >> tok) last = tok;
  return last;
}

/// @brief Parses every `newmtl` block of an .mtl file.
std::vector<ObjMtlDef> ParseObjMtl(const std::string &mtl_path) {
  std::vector<ObjMtlDef> defs;
  std::ifstream          f(mtl_path);
  if (!f.is_open()) return defs;

  ObjMtlDef cur;
  bool      have = false;
  std::string line;
  while (std::getline(f, line)) {
    std::istringstream iss(line);
    std::string        type;
    iss >> type;
    if (type == "newmtl") {
      if (have) defs.push_back(cur);
      cur  = ObjMtlDef{};
      have = true;
      iss >> cur.name;
    } else if (type == "Kd") {
      float r = 1.0f, g = 1.0f, b = 1.0f;
      if (iss >> r >> g >> b) cur.kd = glm::vec3(r, g, b);
    } else if (type == "Ns") {
      iss >> cur.shininess;
    } else if (type == "map_Kd") {
      cur.albedo = ReadMapPath(iss);
    } else if (type == "map_Bump" || type == "map_Kn" || type == "norm" || type == "bump") {
      if (cur.normal.empty()) cur.normal = ReadMapPath(iss);
    } else if (type == "map_Ks") {
      cur.specular = ReadMapPath(iss);
    } else if (type == "map_Ka") {
      cur.reflection = ReadMapPath(iss);
    }
  }
  if (have) defs.push_back(cur);
  return defs;
}

/// @brief Turns an .mtl def into a Material; textures resolve relative to the
/// OBJ's folder. Kd only applies when no map_Kd overrides it.
Ref<Material> MaterializeObjMtl(const ObjMtlDef &d, const std::filesystem::path &obj_dir) {
  Ref<Material> m = CreateRef<Material>();
  m->SetBaseColorFactor(glm::vec4(1.0f));
  m->SetMetallicFactor(0.0f);
  m->SetRoughnessFactor(1.0f);
  const auto resolve = [&](const std::string &rel) -> Ref<Texture> {
    if (rel.empty()) return nullptr;
    const std::filesystem::path p = obj_dir / rel;
    return std::filesystem::exists(p) ? Texture::Create(p.string()) : nullptr;
  };
  if (d.albedo.empty()) {
    m->SetBaseColorFactor(glm::vec4(d.kd, 1.0f));
  }
  if (Ref<Texture> t = resolve(d.albedo)) {
    m->SetAlbedoMap(t);
    m->SetAlbedoSRGB(true);  // diffuse maps are sRGB-encoded
  }
  if (Ref<Texture> t = resolve(d.normal)) m->SetNormalMap(t);
  if (Ref<Texture> t = resolve(d.specular)) {
    m->SetSpecularMap(t);
    // Specular-workflow OBJ -> a low-roughness dielectric under PBR.
    m->SetMetallicFactor(0.0f);
    m->SetRoughnessFactor(0.4f);
  }
  if (Ref<Texture> t = resolve(d.reflection)) m->SetReflectionMap(t);
  if (d.shininess > 0.0f) m->SetShininess(d.shininess);
  return m;
}

/// @brief Locates the first existing `mtllib` file next to an OBJ.
std::string ResolveObjMtl(const std::string &obj_path, const std::filesystem::path &obj_dir) {
  std::ifstream obj(obj_path);
  if (!obj.is_open()) return "";
  std::string line;
  while (std::getline(obj, line)) {
    std::istringstream iss(line);
    std::string        type;
    iss >> type;
    if (type == "mtllib") {
      std::string name;
      iss >> name;
      if (std::filesystem::exists(obj_dir / name)) return name;
    }
  }
  return "";
}

/// @brief Builds a Mesh from the triangles of one `usemtl` group.
Ref<Mesh> BuildObjPartMesh(const std::vector<glm::vec3> &positions, const std::vector<glm::vec2> &texcoords,
                           const std::vector<glm::vec3> &normals,
                           const std::vector<std::array<Corner, 3>> &tris) {
  std::vector<Vertex>       vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(tris.size() * 3);
  indices.reserve(tris.size() * 3);

  if (!normals.empty()) {
    std::unordered_map<Corner, unsigned int, CornerHash> cache;
    for (const auto &tri : tris) {
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
    // No normals in file: flat-shade the group (3 fresh vertices per triangle).
    for (const auto &tri : tris) {
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
        vertex.normal = face_normal;
        indices.push_back(static_cast<unsigned int>(vertices.size()));
        vertices.push_back(vertex);
      }
    }
  }
  return Mesh::Create(vertices, indices);
}

}  // namespace

Ref<ObjModel> ModelLoader::LoadObjModel(const std::string &path) {
  const std::filesystem::path obj_dir = std::filesystem::path(path).parent_path();
  const std::string           mtl_rel = ResolveObjMtl(path, obj_dir);
  const std::vector<ObjMtlDef> defs =
      mtl_rel.empty() ? std::vector<ObjMtlDef>{} : ParseObjMtl((obj_dir / mtl_rel).string());
  std::unordered_map<std::string, size_t> def_by_name;
  for (size_t i = 0; i < defs.size(); ++i) def_by_name[defs[i].name] = i;

  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("ModelLoader") << "Failed to open model file: " << path;
    return nullptr;
  }

  std::vector<glm::vec3> positions;
  std::vector<glm::vec2> texcoords;
  std::vector<glm::vec3> normals;

  // Per `usemtl` group: the material name and its triangles.
  std::vector<std::string> mat_names;
  std::unordered_map<std::string, size_t> part_by_name;
  std::vector<std::vector<std::array<Corner, 3>>> part_tris;
  const auto part_of = [&](const std::string &name) -> size_t {
    const auto it = part_by_name.find(name);
    if (it != part_by_name.end()) return it->second;
    part_by_name[name] = mat_names.size();
    mat_names.push_back(name);
    part_tris.emplace_back();
    return mat_names.size() - 1;
  };

  std::string cur_mat;
  std::string line;
  while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string        type;
    iss >> type;
    if (type == "v") {
      float x = 0, y = 0, z = 0;
      iss >> x >> y >> z;
      positions.emplace_back(x, y, z);
    } else if (type == "vt") {
      float u = 0, v = 0;
      iss >> u >> v;
      texcoords.emplace_back(u, v);
    } else if (type == "vn") {
      float x = 0, y = 0, z = 0;
      iss >> x >> y >> z;
      normals.emplace_back(x, y, z);
    } else if (type == "usemtl") {
      iss >> cur_mat;
    } else if (type == "f") {
      std::vector<Corner> corners;
      std::string         token;
      while (iss >> token) corners.push_back(ParseCorner(token));
      std::vector<std::array<Corner, 3>> &tris = part_tris[part_of(cur_mat)];
      for (size_t i = 1; i + 1 < corners.size(); ++i) {
        tris.push_back({corners[0], corners[i], corners[i + 1]});
      }
    }
  }

  if (positions.empty() || part_tris.empty()) {
    LOG_WARN("ModelLoader") << "No faces found in OBJ file: " << path;
    return nullptr;
  }

  Ref<ObjModel> model = CreateRef<ObjModel>();
  for (size_t p = 0; p < part_tris.size(); ++p) {
    if (part_tris[p].empty()) continue;
    ObjModelPart part;
    part.mesh = BuildObjPartMesh(positions, texcoords, normals, part_tris[p]);
    const auto def_it = def_by_name.find(mat_names[p]);
    part.material = def_it != def_by_name.end() ? MaterializeObjMtl(defs[def_it->second], obj_dir)
                                                : MaterializeObjMtl(ObjMtlDef{}, obj_dir);
    part.name = mat_names[p];
    model->parts.push_back(part);
  }

  LOG_INFO("ModelLoader") << "Loaded multi-material OBJ '" << path << "': " << model->parts.size() << " parts";
  return model->parts.empty() ? nullptr : model;
}

}  // namespace MEngine
