#version 460 core

out vec2 uv;

void main() {
  // Fullscreen triangle from gl_VertexID (no vertex buffer needed).
  vec2 pos   = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
  gl_Position = vec4(pos, 0.0, 1.0);
  uv          = pos * 0.5 + 0.5;
}
