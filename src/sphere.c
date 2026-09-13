#include <cglm/cglm.h>
#include <math.h>
#include <string.h>

#include "defines.h"
#include "mesh.h"
#include "sphere.h"

void initialize_sphere_data(SphereData *data, int sectors, int stacks) {
  memset(data, 0, sizeof(*data));
  float sector_step = PI * 2.0f / (float)sectors;
  float stack_step = PI / (float)stacks;

  for (int i = 0; i <= stacks; i++) {
    float stack_angle = PI / 2.0f - stack_step * (float)i;
    float xz = cosf(stack_angle);
    float y = sinf(stack_angle);

    for (int j = 0; j <= sectors; j++) {
      if (data->vertices.len >= data->vertices.size) {
        data->vertices.size = (data->vertices.size + 1) * 2;
        data->vertices.buf = realloc(data->vertices.buf, sizeof(*data->vertices.buf) * data->vertices.size);
      }
      float sector_angle = sector_step * (float)j;
      float x = xz * sinf(sector_angle);
      float z = xz * cosf(sector_angle);
      size_t len = data->vertices.len;
      data->vertices.buf[len].position[0] = x;
      data->vertices.buf[len].position[1] = y;
      data->vertices.buf[len].position[2] = z;

      data->vertices.buf[len].normal[0] = x;
      data->vertices.buf[len].normal[1] = y;
      data->vertices.buf[len].normal[2] = z;

      float s = (float)j / (float)sectors;
      float t = (float)i / (float)stacks;
      data->vertices.buf[len].texture_coordinates[0] = s;
      data->vertices.buf[len].texture_coordinates[1] = t;
      data->vertices.len++;
    }
  }

  for (int i = 0; i < stacks; i++) {
    int k1 = i * (sectors + 1);
    int k2 = k1 + sectors + 1;
    for (int j = 0; j < sectors; j++, k1++, k2++) {
      if (data->indices.len >= data->indices.size) {
        data->indices.size = (data->indices.size + 1) * 2;
        data->indices.buf = realloc(data->indices.buf, sizeof(*data->indices.buf) * data->indices.size);
      }
      if (i) {
        size_t len = data->indices.len;
        data->indices.buf[len][0] = k1;
        data->indices.buf[len][1] = k2;
        data->indices.buf[len][2] = k1 + 1;
        data->indices.len++;
      }
      if (i != (stacks - 1)) {
        size_t len = data->indices.len;
        data->indices.buf[len][0] = k1 + 1;
        data->indices.buf[len][1] = k2;
        data->indices.buf[len][2] = k2 + 1;
        data->indices.len++;
      }
    }
  }
}

void free_sphere_data(const SphereData *data) {
  free(data->vertices.buf);
  free(data->indices.buf);
}

size_t get_sphere_vertices_size(const SphereData *data) { return sizeof(*data->vertices.buf) * data->vertices.len; }

size_t get_sphere_indices_size(const SphereData *data) { return sizeof(*data->indices.buf) * data->indices.len; }
