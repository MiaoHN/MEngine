#version 460 core
// 2D sprite fragment shader: the albedo (texture * tint) is the final colour and
// the texture alpha is the sprite's opacity.
//
// No tone mapping, bloom or gamma step is applied anywhere in the 2D path - a
// sprite is drawn straight into the target framebuffer - so a sprite looks
// exactly like its PNG (the engine uploads textures as raw bytes, i.e. already
// in display space).

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D albedo_map;
uniform int       has_albedo_map     = 0;
uniform vec4      base_color_factor  = vec4(1.0);

void main() {
  vec4 texel = has_albedo_map == 1 ? texture(albedo_map, TexCoord) : vec4(1.0);
  FragColor  = texel * base_color_factor;
}
