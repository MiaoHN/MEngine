#version 460 core

in vec3 tex_dir;
out vec4 FragColor;

uniform samplerCube skybox;

void main() {
  FragColor = vec4(texture(skybox, tex_dir).rgb, 1.0);
}
