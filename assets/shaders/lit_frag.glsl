#version 460 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D texture1;
uniform vec3      view_pos;
uniform vec3      tint_color   = vec3(1.0);
uniform int       has_texture  = 0;

// Simple directional light; kept as uniforms so it can be driven from C++ later.
uniform vec3 light_dir   = normalize(vec3(-0.3, -1.0, -0.4));
uniform vec3 light_color = vec3(1.0);

void main() {
  vec3 n = normalize(Normal);
  vec3 l = normalize(-light_dir);

  float diff = max(dot(n, l), 0.0);

  vec3 base = tint_color;
  if (has_texture == 1) {
    base *= texture(texture1, TexCoord).rgb;
  }

  vec3 view_dir = normalize(view_pos - FragPos);
  vec3 half_vec = normalize(l + view_dir);
  float spec = pow(max(dot(n, half_vec), 0.0), 32.0);

  vec3 ambient = 0.15 * base;
  vec3 color = ambient + diff * base * light_color + spec * light_color;

  FragColor = vec4(color, 1.0);
}
