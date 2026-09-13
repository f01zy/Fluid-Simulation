#ifndef RENDERER_H
#define RENDERER_H

#include "mesh.h"
#include <cglm/cglm.h>
#include <stddef.h>

void draw_sphere(const Mesh *mesh, vec3 pos, vec3 color, float radius, size_t indices_count, uint32_t shader_program);

#endif
