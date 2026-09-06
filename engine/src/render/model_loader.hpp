#pragma once

#include <string>
#include <vector>

#include "core/common.hpp"

namespace MEngine {

class Material;
class Mesh;
class Texture;

/// @brief One rendered part of a multi-material OBJ: a submesh plus the
/// Material that should shade it. `name` is the OBJ `usemtl` / `.mtl`
/// `newmtl` material name (empty when the OBJ had no material for the part).
struct ObjModelPart {
  Ref<Mesh>     mesh;
  Ref<Material> material;
  std::string   name;
};

/// @brief A multi-material OBJ model: geometry split per `usemtl` group, each
/// part with its own submesh + material (the .mtl `newmtl` block of that name).
struct ObjModel {
  std::vector<ObjModelPart> parts;
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

  /// @brief Loads a multi-material OBJ into an ObjModel: geometry is split per
  /// `usemtl` group and each part gets its own submesh + Material (from the
  /// `.mtl` `newmtl` block with that name, or a default white material when the
  /// name is unknown). Shaders are NOT assigned; callers set them per part.
  /// Returns nullptr on failure / when no faces were found.
  static Ref<ObjModel> LoadObjModel(const std::string &path);

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
