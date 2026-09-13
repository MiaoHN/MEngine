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
uniform int       u_lo_hdr_tone     = 0;  // 1 = LearnOpenGL HDR/bloom tone: 1 - exp(-x), + gamma
uniform int       u_reinhard_tone   = 0;  // 1 = LearnOpenGL PBR tone: color/(color+1), + gamma

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
  } else if (u_lo_hdr_tone == 1) {
    // LearnOpenGL 6.hdr / 7.bloom tone: 1 - exp(-hdr), then gamma. (hdr already
    // includes the exposure multiply, so this is 1 - exp(-color*exposure).)
    mapped = vec3(1.0) - exp(-hdr);
    mapped = pow(mapped, vec3(1.0 / 2.2));
  } else if (u_reinhard_tone == 1) {
    // LearnOpenGL PBR (6.pbr 1.1/1.2): color / (color + 1), then gamma.
    mapped = hdr / (hdr + vec3(1.0));
    mapped = pow(mapped, vec3(1.0 / 2.2));
  } else {
    mapped = ACESToneMap(hdr);
    mapped = pow(mapped, vec3(1.0 / 2.2));
  }

  FragColor = vec4(mapped, 1.0);
}
