#pragma once

#include <string>
#include <vector>

#include "core/common.hpp"

namespace MEngine {

class Material;
class Mesh;
class Texture;

/// @brief One rendered part of a multi-material model: a submesh plus the
/// Material that should shade it. `name` is the source material name (the OBJ
/// `usemtl` / `.mtl` `newmtl` name; empty when the part had no material).
struct ModelPart {
  Ref<Mesh>     mesh;
  Ref<Material> material;
  std::string   name;
};

/// @brief A multi-material 3D model (single entity): geometry split per
/// material group, each part with its own submesh + material. Rendered by a
/// ModelComponent - every part shares the owning entity's transform.
struct Model {
  std::vector<ModelPart> parts;
};

/**
 * @brief Loads 3D model files into engine resources.
 *
 * Currently supports Wavefront OBJ and glTF 2.0 (`.gltf` / `.glb`).
 */
class ModelLoader {
 public:
  /// @brief Loads an OBJ file into a single Mesh. Returns nullptr on failure.
  ///
  /// Supported features:
  ///  - `v` / `vt` / `vn` / `f` (including `v/vt`, `v//vn`, `v/vt/vn`)
  ///  - polygon triangulation (fan)
  ///  - flat face normals are generated when the file has no `vn`
  static Ref<Mesh> LoadObj(const std::string &path);

  /// @brief Builds a Material from an OBJ's sidecar `.mtl` file (the `mtllib`
  /// the OBJ references, resolved next to it). Returns the FIRST material's
  /// maps/factors: `Kd` base colour, `map_Kd` albedo (marked sRGB), `map_Bump` /
  /// `map_Kn` normal and `map_Ks` specular. Returns nullptr when there is no
  /// readable `.mtl`, so callers can fall back to name-based heuristics.
  ///
  /// The shader is not assigned here; callers should set it. For OBJ files
  /// using SEVERAL materials (`usemtl`), use LoadObjModel instead - this only
  /// returns the FIRST material block.
  static Ref<Material> LoadObjMaterial(const std::string &obj_path);

  /// @brief Loads a multi-material OBJ into a Model: geometry is split per
  /// `usemtl` group and each part gets its own submesh + Material (from the
  /// `.mtl` `newmtl` block with that name, or a default white material when the
  /// name is unknown). Shaders are NOT assigned; callers set them per part.
  /// Identical part meshes (same file + material group) are shared through a
  /// MeshLibrary, so several entities of one model reuse the same GPU mesh and
  /// batch into instanced draws. Returns nullptr on failure / no faces.
  static Ref<Model> LoadObjModel(const std::string &path);

  /// @brief Loads a glTF 2.0 file (`.gltf` or `.glb`) into a single Mesh.
  ///
  /// Takes the first mesh's first primitive; attributes used: POSITION,
  /// NORMAL (generated flat if missing) and TEXCOORD_0.
  static Ref<Mesh> LoadGltf(const std::string &path);

  /// @brief Loads the base-color texture of the first glTF material.
  ///
  /// Decoded to RGBA and uploaded as a Texture; returns nullptr if the model
  /// has no base-color texture.
  static Ref<Texture> LoadGltfBaseColorTexture(const std::string &path);

  /// @brief Builds a PBR Material from the first glTF material.
  ///
  /// Extracts base color / normal / metallic-roughness / occlusion textures
  /// and the material factors. The shader is not assigned here; callers should
  /// set it via Material::SetShader.
  static Ref<Material> LoadGltfMaterial(const std::string &path);
};

}  // namespace MEngine
