#ifndef MESH_H
#define MESH_H

#include <cglm/cglm.h>
#include <glad/gl.h>

typedef struct {
  vec3 position;
  vec3 normal;
  vec2 texture_coordinates;
} __attribute__((packed)) Vertice;

typedef struct {
  size_t size;
  GLenum type;
} Attribute;

typedef struct {
  unsigned VAO;
  unsigned VBO;
  unsigned IBO;
} Mesh;

void free_mesh(const Mesh *mesh);
bool initialize_mesh(Mesh *mesh, const float *vertices, size_t vertices_len, const ivec3 *indices, size_t indices_len, const Attribute *attributes,
                     size_t attributes_len, GLenum render_mode);

#endif
