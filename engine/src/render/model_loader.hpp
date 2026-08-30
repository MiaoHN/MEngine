#pragma once

#include "core/common.hpp"

namespace MEngine {

class Mesh;

/**
 * @brief Loads 3D model files into engine resources.
 *
 * Currently supports the Wavefront OBJ format. glTF/Assimp-based import can be
 * added later without changing the caller-facing API.
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
};

}  // namespace MEngine
