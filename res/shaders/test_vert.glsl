#version 460 core
layout (location = 0) in vec3 aPos; // 位置变量的属性位置值为0
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view_proj;

void main()
{
    gl_Position = model * view_proj * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}