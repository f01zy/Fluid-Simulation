#include <glad/gl.h>

#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <stdint.h>
#include <stdio.h>

#include "camera.h"
#include "defines.h"
#include "framebuffer.h"
#include "grid.h"
#include "renderer.h"
#include "shader.h"
#include "sphere.h"

void render_to_framebuffer(const Resources resources, float radius, size_t indices_count, ivec2 screen_size, uint32_t base_shader_program,
                           uint32_t text_shader_program, float dt) {
  const Grid *grid = resources.grid;
  const Particles *particles = resources.particles;
  const Mesh *sphere_mesh = resources.sphere_mesh;
  const Mesh *text_mesh = resources.text_mesh;
  const Font *font = resources.font;
  const Camera *camera = resources.camera;
  const Framebuffer *framebuffer = resources.framebuffer;

  mat4 base_projection;
  glm_perspective(camera->fov, (float)screen_size[1] / (float)screen_size[1], 0.1f, 100.0f, base_projection);

  mat4 text_projection;
  glm_ortho(0.0f, screen_size[0], 0.0f, screen_size[1], 0.0f, 100.0f, text_projection);

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->FBO);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glUseProgram(base_shader_program);
  mat4 view;
  get_camera_view_matrix(camera, view);
  uniform_set_mat4(base_shader_program, "view", view);
  uniform_set_mat4(base_shader_program, "projection", base_projection);

  for (int i = 0; i < particles->len; i++) {
    Particle *p = &particles->ptr[i];
    render_sphere(sphere_mesh, p->pos, (vec3){0.0f, 0.58f, 1.0f}, radius, indices_count, base_shader_program);
  }

  glUseProgram(text_shader_program);
  uniform_set_mat4(text_shader_program, "projection", text_projection);

  char fps_text[512];
  fps_text[snprintf(fps_text, sizeof(fps_text) - 1, "FPS: %.1f", 1.0f / dt)] = '\0';
  render_text(font, text_mesh, fps_text, (vec2){10.0f, HEIGHT - 20.0f}, 1.0f, (vec3){1.0f, 1.0f, 1.0f}, text_shader_program);

  char particles_text[512];
  particles_text[snprintf(particles_text, sizeof(particles_text) - 1, "particles: %zu", particles->len)] = '\0';
  render_text(font, text_mesh, particles_text, (vec2){10.0f, HEIGHT - 40.0f}, 1.0f, (vec3){1.0f, 1.0f, 1.0f}, text_shader_program);
}

void render_to_screen(const Mesh *screen_mesh, uint32_t screen_texture, uint32_t screen_shader_program) {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(screen_shader_program);
  glBindTexture(GL_TEXTURE_2D, screen_texture);
  glBindVertexArray(screen_mesh->VAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
}

void render_to_video(FILE *ffmpeg, uint8_t *pixels, ivec2 screen_size) {
  fprintf(ffmpeg, "P6\n%d %d\n255\n", screen_size[0], screen_size[1]);
  fwrite(pixels, 3, screen_size[0] * screen_size[1], ffmpeg);
}
