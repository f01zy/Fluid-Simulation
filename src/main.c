#include <glad/gl.h>

#include <GLFW/glfw3.h>
#include <cJSON.h>
#include <cglm/cglm.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "camera.h"
#include "defines.h"
#include "framebuffer.h"
#include "grid.h"
#include "input.h"
#include "mesh.h"
#include "renderer.h"
#include "settings.h"
#include "shader.h"
#include "sphere.h"
#include "text.h"

static GLFWwindow *init_opengl_context(float width, float height, const char *title, bool visible, Camera *camera) {
  if (!glfwInit()) {
    printf("[ERROR] Failed to initialize GLFW\n");
    return NULL;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
  glfwWindowHint(GLFW_VISIBLE, visible ? GL_TRUE : GL_FALSE);

  GLFWwindow *window = glfwCreateWindow((int)width, (int)height, title, NULL, NULL);
  if (!window) {
    printf("[ERROR] Failed to create window\n");
    glfwTerminate();
    return NULL;
  }

  glfwMakeContextCurrent(window);

  if (visible && camera) {
    glfwSetWindowUserPointer(window, camera);
    glfwSetScrollCallback(window, mouse_scroll_callback);
    glfwSetCursorPosCallback(window, mouse_position_callback);
  }

  int version = gladLoadGL(glfwGetProcAddress);
  if (version == 0) {
    printf("[ERROR] Failed to initialize OpenGL context (GLAD)\n");
    glfwDestroyWindow(window);
    glfwTerminate();
    return NULL;
  }

  printf("[LOG] Loaded OpenGL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  return window;
}

void step(Grid *grid, const Particles *particles, float dt) {
  for (int i = 0; i < particles->len; i++) {
    Particle *p = &particles->ptr[i];
    advect(grid, p->pos, dt, p->pos);
  }

  particles_to_grid(grid, particles->ptr, particles->len);
  apply_gravity(grid, dt);
  project_pressure(grid);

  for (int i = 0; i < particles->len; i++) {
    Particle *p = &particles->ptr[i];
    grid_to_particles(grid, p, grid->flip_ratio, p->vel);
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("[ERROR] The settings path is not specified\n");
    return -1;
  }

  GLFWwindow *window = NULL;
  FILE *ffmpeg = NULL;
  uint8_t *pixels = NULL;

  uint32_t base_shader_program = INVALID;
  uint32_t text_shader_program = INVALID;
  uint32_t screen_shader_program = INVALID;

  bool grid_inited = false;
  bool particles_inited = false;
  bool sphere_data_inited = false;
  bool font_inited = false;
  bool sphere_mesh_inited = false;
  bool text_mesh_inited = false;
  bool screen_mesh_inited = false;

  Mesh sphere_mesh = {0};
  Mesh text_mesh = {0};
  Mesh screen_mesh = {0};
  Framebuffer framebuffer = {0};
  SphereData data = {0};
  Grid grid = {0};
  Particles particles = {0};
  Font font = {0};

  Settings settings;
  if (!read_settings(argv[1], &settings)) goto cleanup;

  initialize_grid(&settings, &grid);
  grid_inited = true;

  if (!read_particles(settings.particles, &particles)) goto cleanup;
  particles_inited = true;

  Camera camera;
  initialize_camera(&camera);

  bool is_screen = (settings.render_type == RENDER_TO_SCREEN);
  window = init_opengl_context(WIDTH, HEIGHT, TITLE, is_screen, &camera);
  if (!window) goto cleanup;

  if (!is_screen) {
    const char *ffmpeg_cmd = "ffmpeg -y -f image2pipe -vcodec ppm -r 60 -i - "
                             "-c:v libx264 -pix_fmt yuv420p -vf vflip output.mp4";
    ffmpeg = popen(ffmpeg_cmd, "w");
    if (!ffmpeg) {
      printf("[ERROR] Failed to open ffmpeg pipe\n");
      goto cleanup;
    }
    pixels = malloc(WIDTH * HEIGHT * 3);
  }

  base_shader_program = create_shader_program("shaders/base.vert", "shaders/base.frag");
  if (base_shader_program == INVALID) goto cleanup;

  text_shader_program = create_shader_program("shaders/text.vert", "shaders/text.frag");
  if (text_shader_program == INVALID) goto cleanup;

  screen_shader_program = create_shader_program("shaders/screen.vert", "shaders/screen.frag");
  if (screen_shader_program == INVALID) goto cleanup;

  if (!create_framebuffer((vec2){WIDTH, HEIGHT}, &framebuffer)) goto cleanup;
  if (!initialize_font(&font, "resources/fonts/Tamzen8x16b.ttf", FONT_SIZE)) goto cleanup;
  font_inited = true;

  initialize_sphere_data(&data, 72, 24);
  sphere_data_inited = true;

  size_t indices_count = sizeof(*data.indices.buf) / sizeof(*data.indices.buf[0]) * data.indices.len;
  size_t vertices_size = get_sphere_vertices_size(&data);
  size_t indices_size = get_sphere_indices_size(&data);

  Attribute sphere_attributes[] = {{.size = 3, .type = GL_FLOAT}, {.size = 3, .type = GL_FLOAT}, {.size = 2, .type = GL_FLOAT}};
  if (!initialize_mesh(&sphere_mesh, (float *)data.vertices.buf, vertices_size, data.indices.buf, indices_size, sphere_attributes, 3, GL_STATIC_DRAW))
    goto cleanup;
  sphere_mesh_inited = true;

  Attribute text_attributes[] = {{.size = 4, .type = GL_FLOAT}};
  ivec3 text_indices[] = {{0, 1, 3}, {1, 2, 3}};
  if (!initialize_mesh(&text_mesh, NULL, sizeof(vec4) * 4, text_indices, sizeof(text_indices), text_attributes, 1, GL_DYNAMIC_DRAW)) goto cleanup;
  text_mesh_inited = true;

  Attribute screen_attributes[] = {{.size = 2, .type = GL_FLOAT}, {.size = 2, .type = GL_FLOAT}};
  const float screen_vertices[] = {
    -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
  };
  if (!initialize_mesh(&screen_mesh, screen_vertices, sizeof(screen_vertices), NULL, 0, screen_attributes, 2, GL_STATIC_DRAW)) goto cleanup;
  screen_mesh_inited = true;

  float radius = cbrt((3 * pow(settings.dx, 3)) / (32 * PI));
  float last_frame = 0.0f;
  float dt_need = 1.0f / FPS;
  int last_frames = 0;
  int need_frames = settings.duration * FPS;

  while (!glfwWindowShouldClose(window)) {
    float dt = dt_need;
    float now = (float)glfwGetTime();

    if (last_frames >= need_frames) break;
    if (is_screen) {
      dt = now - last_frame;
      if (dt < dt_need) continue;
      last_frame = now;
    }

    step(&grid, &particles, dt);
    render_to_framebuffer(
      (Resources){
        .grid = &grid,
        .particles = &particles,
        .sphere_mesh = &sphere_mesh,
        .text_mesh = &text_mesh,
        .font = &font,
        .camera = &camera,
        .framebuffer = &framebuffer,
      },
      radius, indices_count, (ivec2){WIDTH, HEIGHT}, base_shader_program, text_shader_program, dt);

    if (is_screen) {
      render_to_screen(&screen_mesh, framebuffer.screen_texture, screen_shader_program);
      glfwSwapBuffers(window);
      glfwPollEvents();
    } else {
      glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, pixels);
      render_to_video(ffmpeg, pixels, (ivec2){WIDTH, HEIGHT});
    }

    last_frames++;
  }

cleanup:
  if (base_shader_program != INVALID) glDeleteProgram(base_shader_program);
  if (text_shader_program != INVALID) glDeleteProgram(text_shader_program);
  if (screen_shader_program != INVALID) glDeleteProgram(screen_shader_program);
  if (sphere_mesh_inited) free_mesh(&sphere_mesh);
  if (text_mesh_inited) free_mesh(&text_mesh);
  if (screen_mesh_inited) free_mesh(&screen_mesh);
  if (sphere_data_inited) free_sphere_data(&data);
  if (font_inited) free_font(&font);
  if (grid_inited) free_grid(&grid);
  if (particles_inited) free(particles.ptr);
  if (pixels) free(pixels);
  if (ffmpeg) pclose(ffmpeg);
  if (window) glfwDestroyWindow(window);
  glfwTerminate();
}
