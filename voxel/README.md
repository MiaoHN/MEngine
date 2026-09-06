# voxel — Minecraft-like tech demo

A standalone example target that builds a small voxel world on top of the
**engine's public API only** (no `engine/src` code is modified for it — the
engine stays generic).

It covers three things you asked for:

- **A. Terrain** — deterministic value-noise heightmap world (grass/dirt/stone/
  sand + scattered oak trees), chunked into 16×16-column meshes, each chunk one
  engine `Mesh` rendered as a `Scene` entity with a shared pbr material whose
  albedo is a **procedurally generated texture atlas** (built in CPU memory,
  uploaded via `Texture::SetData`). Sun light + directional shadows + skybox
  come from the existing `Scene::RenderMeshes` pipeline.
- **B. Player movement + collision** — first-person camera (yaw/pitch + mouse
  capture), WASD, jump, gravity, and **custom AABB-vs-voxel collision** (axis-
  separated integration with revert) — voxel terrain is *not* a physics-engine
  rigid body.
- **C. Place / break** — center-screen voxel raycast (reach 6), crosshair-style
  targeting, LMB breaks, RMB places the hotbar block, with a yellow **ghost
  preview** cube on the target cell and chunk remeshing of the affected chunk +
  neighbours.

## Build & run

```sh
cmake --build --preset windows-clang-debug --target voxel
./build/windows-clang-debug/voxel/voxel.exe          # from the build dir
```

The build copies `assets/` next to the exe so the shared skybox/shaders resolve.

## Controls

| Key | Action |
| --- | --- |
| Mouse | look |
| WASD | walk |
| Space | jump (or fly up) |
| Shift / Ctrl | fly down |
| F | toggle fly mode |
| 1..6 | select block type (grass/dirt/stone/sand/wood/leaves) |
| Esc | release / re-capture the mouse cursor |
| LMB | break block |
| RMB | place block (ghost preview shows the cell) |

## Layout

- `voxel_atlas.{hpp,cpp}` — block registry + CPU texture atlas (tiles, UVs).
- `voxel_world.{hpp,cpp}` — dense world storage, terrain + tree generation,
  and the chunk mesher (`BuildChunkMesh`).
- `voxel_app.{hpp,cpp}` — the `Application` subclass: chunks → `Scene`
  entities, player/controller, picking, remeshing.
