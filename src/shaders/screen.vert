#version 330 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texture_coordinates;

out vec2 vertex_texture_coordinates;

void main() {
  gl_Position = vec4(position, 0.0f, 1.0f);
  vertex_texture_coordinates = texture_coordinates;
}
