#ifndef SPHERE_H
#define SPHERE_H

#include "mesh.h"
#include <cglm/cglm.h>

typedef struct {
  struct {
    Vertice *buf;
    size_t len;
    size_t size;
  } vertices;

  struct {
    ivec3 *buf;
    size_t len;
    size_t size;
  } indices;
} SphereData;

void initialize_sphere_data(SphereData *data, int sectors, int stacks);
void free_sphere_data(const SphereData *data);
size_t get_sphere_vertices_size(const SphereData *data);
size_t get_sphere_indices_size(const SphereData *data);

#endif
