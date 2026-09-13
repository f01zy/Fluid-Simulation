#include <cglm/cglm.h>
#include <stddef.h>

#include "mesh.h"
#include "renderer.h"
#include "shader.h"

void draw_sphere(const Mesh *mesh, vec3 pos, vec3 color, float radius, size_t indices_count, uint32_t shader_program) {
  mat4 model = GLM_MAT4_IDENTITY_INIT;
  glm_translate(model, pos);
  glm_scale_uni(model, radius);
  uniform_set_mat4(shader_program, "model", model);
  uniform_set_vec3(shader_program, "sphere_color", color);
  glBindVertexArray(mesh->VAO);
  glDrawElements(GL_TRIANGLES, indices_count, GL_UNSIGNED_INT, NULL);
  glBindVertexArray(0);
}
