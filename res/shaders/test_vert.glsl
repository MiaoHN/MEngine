#version 460 core
layout (location = 0) in vec3 aPos; // 位置变量的属性位置值为0

out vec4 vertexColor;

uniform mat4 model;
uniform mat4 view_proj;

void main()
{
    gl_Position = model * view_proj * vec4(aPos, 1.0);
    vertexColor = vec4(0.5, 0.0, 0.0, 1.0); // 把输出变量设置为暗红色
}