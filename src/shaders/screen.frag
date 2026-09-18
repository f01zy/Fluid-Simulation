#version 330 core

in vec2 vertex_texture_coordinates;
out vec4 color;
uniform sampler2D screen_texture;

void main() { color = texture(screen_texture, vertex_texture_coordinates); }
