#version 460 core

in vec2 uv;
out vec4 FragColor;

uniform sampler2D current;   // jittered current frame (HDR)
uniform sampler2D history;   // previous resolved frame (HDR)
uniform vec2  texel_size;
uniform float blend;         // 1.0 on the first frame, else ~0.1

// Clamp the history color to the current frame's local neighborhood (AABB) to
// reject stale / ghosted samples where geometry moved.
vec3 ClampAABB(vec3 h, vec3 c) {
  vec3 n1 = texture(current, uv + vec2(1.0, 0.0) * texel_size).rgb;
  vec3 n2 = texture(current, uv - vec2(1.0, 0.0) * texel_size).rgb;
  vec3 n3 = texture(current, uv + vec2(0.0, 1.0) * texel_size).rgb;
  vec3 n4 = texture(current, uv - vec2(0.0, 1.0) * texel_size).rgb;
  vec3 minv = min(c, min(min(n1, n2), min(n3, n4)));
  vec3 maxv = max(c, max(max(n1, n2), max(n3, n4)));
  return clamp(h, minv, maxv);
}

void main() {
  vec3 c = texture(current, uv).rgb;
  vec3 h = ClampAABB(texture(history, uv).rgb, c);
  FragColor = vec4(mix(h, c, blend), 1.0);
}
