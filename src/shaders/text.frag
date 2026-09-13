#version 330 core

in vec2 vertex_texture_coordinates;
out vec4 color;

uniform sampler2D text_texture;
uniform vec3 text_color;

void main() {
  vec2 inverted_coordinates = vec2(vertex_texture_coordinates.x, 1.0f - vertex_texture_coordinates.y);
  vec4 sampled = vec4(1.0f, 1.0f, 1.0f, texture(text_texture, inverted_coordinates).r);
  color = vec4(text_color, 1.0f) * sampled;
}
