#ifndef RENDERER_H
#define RENDERER_H

#include "camera.h"
#include "framebuffer.h"
#include "grid.h"
#include "mesh.h"
#include "text.h"

#include <GLFW/glfw3.h>
#include <stdio.h>

typedef struct {
  const Grid *grid;
  const Particles *particles;
  const Mesh *sphere_mesh;
  const Mesh *text_mesh;
  const Font *font;
  const Camera *camera;
  Framebuffer *framebuffer;
} Resources;

void render_to_video(FILE *ffmpeg, uint8_t *pixels, ivec2 screen_size);
void render_to_screen(const Mesh *screen_mesh, uint32_t screen_texture, uint32_t screen_shader_program);
void render_to_framebuffer(const Resources resources, size_t indices_count, ivec2 screen_size, uint32_t base_shader_program, uint32_t text_shader_program,
                           float dt);

#endif
