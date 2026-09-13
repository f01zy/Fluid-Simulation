#include <glad/gl.h>
#include <stddef.h>

#include "mesh.h"
#include "utility.h"

bool initialize_mesh(Mesh *mesh, Vertice *vertices, size_t vertices_len, ivec3 *indices, size_t indices_len, Attribute *attributes, size_t attributes_len,
                     GLenum render_mode) {
  for (int i = 0; i < attributes_len; i++) {
    size_t type_size = get_opengl_type_size(attributes[i].type);
    if (type_size == 0) {
      printf("[ERROR] Invalid attribute type\n");
      return false;
    }
  }

  glGenVertexArrays(1, &mesh->VAO);
  glBindVertexArray(mesh->VAO);

  glGenBuffers(1, &mesh->VBO);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
  glBufferData(GL_ARRAY_BUFFER, vertices_len, vertices, render_mode);
  size_t stride = 0;
  size_t offset = 0;
  for (int i = 0; i < attributes_len; i++) {
    stride += get_opengl_type_size(attributes[i].type) * attributes[i].size;
  }
  for (int i = 0; i < attributes_len; i++) {
    const Attribute *attribute = &attributes[i];
    glVertexAttribPointer(i, attribute->size, attribute->type, false, stride, (void *)offset);
    glEnableVertexAttribArray(i);
    offset += get_opengl_type_size(attribute->type) * attribute->size;
  }

  if (indices) {
    glGenBuffers(1, &mesh->IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_len, indices, GL_STATIC_DRAW);
  }

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  return true;
}

void free_mesh(const Mesh *mesh) {
  glDeleteVertexArrays(1, &mesh->VAO);
  glDeleteBuffers(1, &mesh->VBO);
  glDeleteBuffers(1, &mesh->IBO);
}
