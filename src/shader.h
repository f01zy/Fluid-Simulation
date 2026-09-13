#ifndef SHADER_H
#define SHADER_H

#include <cglm/cglm.h>
#include <glad/gl.h>
#include <stdint.h>

uint32_t create_shader_program(const char *vertex_shader_path, const char *fragment_shader_path);
void uniform_set_mat4(uint32_t shader_program, const char *name, mat4 mat);
void uniform_set_vec3(uint32_t shader_program, const char *name, vec3 vec);

#endif
