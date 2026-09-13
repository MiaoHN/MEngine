#version 460 core

in vec3 v_world_pos;

layout(location = 0) out vec4 frag_color;

uniform vec3 view_pos;

// Antialiased line coverage for world-space coordinate `coord` at the given
// line spacing `scale`. Returns 1.0 on a line, fading to 0.0 between lines.
float grid_line(float coord, float scale) {
  float f = abs(fract(coord / scale - 0.5) - 0.5) / fwidth(coord);
  return 1.0 - min(f, 1.0);
}

void main() {
  // Fade the grid out with distance so the finite plane reads as infinite.
  float dist = length(v_world_pos.xz - view_pos.xz);
  float fade = clamp(1.0 - dist / 120.0, 0.0, 1.0);

  vec3 base = vec3(0.07, 0.075, 0.08);

  float minor = grid_line(v_world_pos.x, 1.0) + grid_line(v_world_pos.z, 1.0);
  float major = grid_line(v_world_pos.x, 10.0) + grid_line(v_world_pos.z, 10.0);

  vec3 color = base;
  color = mix(color, vec3(0.14, 0.15, 0.16), clamp(minor, 0.0, 1.0) * 0.5 * fade);
  color = mix(color, vec3(0.34, 0.36, 0.39), clamp(major, 0.0, 1.0) * 0.8 * fade);

  // X axis (red) and Z axis (blue) center lines.
  float x_axis = (1.0 - smoothstep(0.0, 0.12, abs(v_world_pos.z))) * fade;
  float z_axis = (1.0 - smoothstep(0.0, 0.12, abs(v_world_pos.x))) * fade;
  color = mix(color, vec3(0.85, 0.20, 0.20), x_axis * 0.7);
  color = mix(color, vec3(0.20, 0.35, 0.90), z_axis * 0.7);

  frag_color = vec4(color, 1.0);
}
