#include <cglm/cglm.h>
#include <glad/gl.h>
#include <stdint.h>

#include "defines.h"
#include "shader.h"
#include "utility.h"

uint32_t create_shader(uint32_t shader_program, GLenum type, const char *path) {
  char buf[8192], info[512];
  int success;
  const char *source = buf;
  bool status = read_file(path, buf, sizeof(buf));
  if (!status) {
    printf("[ERROR] Failed to read the shader's source\n");
    return INVALID;
  }
  uint32_t shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(shader, sizeof(info), NULL, info);
    printf("[ERROR] Failed to compile the shader: %s\n", info);
    return INVALID;
  }
  return shader;
}

uint32_t create_shader_program(const char *vertex_shader_path, const char *fragment_shader_path) {
  uint32_t shader_program = glCreateProgram();
  uint32_t vertex_shader = create_shader(shader_program, GL_VERTEX_SHADER, vertex_shader_path);
  uint32_t fragment_shader = create_shader(shader_program, GL_FRAGMENT_SHADER, fragment_shader_path);
  if (vertex_shader == INVALID || fragment_shader == INVALID) return INVALID;
  char info[512];
  int success;
  glAttachShader(shader_program, vertex_shader);
  glAttachShader(shader_program, fragment_shader);
  glLinkProgram(shader_program);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(shader_program, sizeof(info), NULL, info);
    printf("[ERROR] Failed to link the shader program: %s\n", info);
    return INVALID;
  }
  return shader_program;
}

void uniform_set_mat4(uint32_t shader_program, const char *name, mat4 mat) {
  glUniformMatrix4fv(glGetUniformLocation(shader_program, name), 1, GL_FALSE, (float *)mat);
}

void uniform_set_vec3(uint32_t shader_program, const char *name, vec3 vec) { glUniform3fv(glGetUniformLocation(shader_program, name), 1, vec); }
