# voxel — Minecraft-like tech demo

A standalone example target that builds a small voxel world on top of the
**engine's public API only** (no `engine/src` code is modified for it — the
engine stays generic).

It covers three things you asked for:

- **A. Terrain** — **endless, procedurally generated** world (deterministic
  value-noise heightmap: grass/dirt/stone/sand + oak trees) that **streams in
  chunks around the player** (each 16×16 chunk one engine `Mesh`/`Scene`
  entity). Blocks use one pbr material whose albedo is a **procedurally
  generated texture atlas** (CPU pixels uploaded via `Texture::SetData`). Side
  faces are UV-unwrapped so tiles are never rotated (grass-green always on top
  of a grass side). Sun light + directional shadows + skybox come from
  `Scene::RenderMeshes`.
- **B. Player movement + collision** — first-person camera (mouse capture),
  WASD, jump, gravity, and **custom AABB-vs-voxel collision** (axis-separated
  integration with revert) — voxel terrain is *not* a physics-engine rigid
  body.
- **C. Place / break** — center-screen voxel raycast (reach 6), LMB breaks,
  RMB places the hotbar block, with a yellow **ghost preview** cube on the
  target cell and remeshing of the affected chunk + neighbours.
- **Fly** — press **F**, or double-press **Space**, to toggle fly (Space = up,
  Shift/Ctrl = down).

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
| Space-Space / F | toggle fly |
| Shift / Ctrl | fly down |
| 1..6 | select block type (grass/dirt/stone/sand/wood/leaves) |
| Esc | release / re-capture the mouse cursor |
| LMB | break block |
| RMB | place block (ghost preview shows the cell) |

Optional env: `MENGINE_VOXEL_SEED=<n>` picks a different world; every seed is
fully deterministic and endless.

## Layout

- `voxel_atlas.{hpp,cpp}` — block registry + CPU texture atlas (tiles, UVs).
- `voxel_world.{hpp,cpp}` — **chunked endless world** (lazy, deterministic
  generation), terrain + trees, chunk prep, and the chunk mesher
  (`BuildChunkMesh`).
- `voxel_app.{hpp,cpp}` — the `Application` subclass: streams chunks → `Scene`
  entities around the player, player/controller, picking, remeshing.
