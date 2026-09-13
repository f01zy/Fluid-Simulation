#version 330 core

out vec4 color;
uniform vec3 sphere_color;

void main() { color = vec4(sphere_color, 1.0f); }
