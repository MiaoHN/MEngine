#version 460 core

in vec3 tex_dir;
out vec4 FragColor;

uniform sampler2D equirectangular_map;

const vec2 inv_atan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v) {
  vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
  uv *= inv_atan;
  uv += 0.5;
  return uv;
}

void main() {
  vec2  uv    = SampleSphericalMap(normalize(tex_dir));
  vec3  color = texture(equirectangular_map, uv).rgb;
  FragColor   = vec4(color, 1.0);
}
