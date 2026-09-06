#version 460 core

in vec2 uv;
out vec4 FragColor;

uniform sampler2D scene;
uniform sampler2D bloom;
uniform sampler2D god_rays;
uniform float     exposure          = 1.2;
uniform float     bloom_strength    = 0.04;
uniform float     god_rays_strength = 0.05;
uniform int       u_linear_output   = 0;  // 1 = raw output (LearnOpenGL style, no ACES/gamma)

vec3 ACESToneMap(vec3 x) {
  const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
  vec3 hdr   = texture(scene, uv).rgb * exposure;
  hdr       += texture(bloom, uv).rgb * bloom_strength;
  hdr       += texture(god_rays, uv).rgb * god_rays_strength;

  vec3 mapped;
  if (u_linear_output == 1) {
    // Raw output: exactly what the shaders computed (like LearnOpenGL's
    // tutorials, which don't ACES tone map or gamma correct).
    mapped = clamp(hdr, 0.0, 1.0);
  } else {
    mapped = ACESToneMap(hdr);
    mapped = pow(mapped, vec3(1.0 / 2.2));
  }

  FragColor = vec4(mapped, 1.0);
}
