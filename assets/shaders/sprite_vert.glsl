#version 460 core
// 2D sprite vertex shader. Shares the engine's mesh layout (position / normal /
// texture coordinates) and the per-instance model matrix convention (locations
// 3..6), so sprites use the same Mesh + instancing path as everything else.
// It is deliberately minimal: the 2D pass has no lighting, shadows or IBL.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;    // unused: sprites are unlit
layout(location = 2) in vec2 aTexCoord;
// Per-instance model matrix (engine convention, locations 3..6).
layout(location = 3) in mat4 aInstanceModel;

uniform mat4 proj_view;

out vec2 TexCoord;

void main() {
  TexCoord    = aTexCoord;
  gl_Position = proj_view * aInstanceModel * vec4(aPos, 1.0);
}
