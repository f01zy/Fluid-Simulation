#include <glad/gl.h>

#include <GLFW/glfw3.h>
#include <assert.h>
#include <cJSON.h>
#include <cglm/cglm.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera.h"
#include "defines.h"
#include "grid.h"
#include "input.h"
#include "mesh.h"
#include "renderer.h"
#include "settings.h"
#include "shader.h"
#include "sphere.h"

GLFWwindow *create_window(float width, float height, const char *title, Camera *camera) {
  if (!glfwInit()) {
    printf("[ERROR] Failed to initialize GLFW\n");
    return NULL;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
  GLFWwindow *window = glfwCreateWindow(width, height, title, NULL, NULL);
  if (!window) {
    printf("[ERROR] Failed to create a window\n");
    glfwTerminate();
    return NULL;
  }
  glfwMakeContextCurrent(window);
  glfwSetWindowUserPointer(window, camera);
  glfwSetScrollCallback(window, mouse_scroll_callback);
  glfwSetCursorPosCallback(window, mouse_position_callback);
  int version = gladLoadGL(glfwGetProcAddress);
  if (version == 0) {
    printf("[ERROR] Failed to initialize OpenGL context\n");
    glfwTerminate();
    return NULL;
  }
  printf("[LOG] Loaded OpenGL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
  glEnable(GL_DEPTH_TEST);
  return window;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("[ERROR] The settings path is not specified\n");
    return -1;
  }

  const char *settings_path = argv[1];
  Camera camera;
  initialize_camera(&camera);

  GLFWwindow *window = create_window(WIDTH, HEIGHT, TITLE, &camera);
  if (!window) return -1;

  uint32_t shader_program
    = create_shader_program("/home/f01zy/Programming/Fluid Simulation/src/shaders/base.vert", "/home/f01zy/Programming/Fluid Simulation/src/shaders/base.frag");
  if (shader_program == INVALID) return -1;

  Settings settings;
  if (!read_settings(settings_path, &settings)) goto cleanup;

  Grid grid;
  initialize_grid(&settings, &grid);

  Particles particles;
  if (!read_particles(settings.particles, &particles)) goto cleanup;

  SphereData data;
  initialize_sphere_data(&data, 72, 24);

  Attribute sphere_attributes[3] = {
    {.size = 3, .type = GL_FLOAT},
    {.size = 3, .type = GL_FLOAT},
    {.size = 2, .type = GL_FLOAT},
  };
  Mesh mesh;
  size_t vertices_size = get_sphere_vertices_size(&data);
  size_t indices_size = get_sphere_indices_size(&data);
  if (!initialize_mesh(&mesh, data.vertices.buf, vertices_size, data.indices.buf, indices_size, sphere_attributes, 3, GL_STATIC_DRAW)) goto cleanup;

  size_t indices_count = sizeof(*data.indices.buf) / sizeof(*data.indices.buf[0]) * data.indices.len;
  float last_frame = 0.0f;
  float dt_need = 1.0f / FPS;
  mat4 projection;
  glm_perspective(camera.fov, (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f, projection);

  while (!glfwWindowShouldClose(window)) {
    float now = glfwGetTime();
    float dt = now - last_frame;
    if (dt < dt_need) continue;
    last_frame = now;
    printf("FPS: %f\n", 1.0f / dt);

    for (int i = 0; i < particles.len; i++) {
      Particle *p = &particles.ptr[i];
      advect(&grid, p->pos, dt, p->pos);
    }

    particles_to_grid(&grid, particles.ptr, particles.len);
    apply_gravity(&grid, dt);
    project_pressure(&grid);

    for (int i = 0; i < particles.len; i++) {
      Particle *p = &particles.ptr[i];
      grid_to_particles(&grid, p, grid.flip_ratio, p->vel);
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shader_program);
    mat4 view;
    get_camera_view_matrix(&camera, view);
    uniform_set_mat4(shader_program, "view", view);
    uniform_set_mat4(shader_program, "projection", projection);

    for (int i = 0; i < particles.len; i++) {
      Particle *p = &particles.ptr[i];
      draw_sphere(&mesh, p->pos, (vec3){1.0f, 1.0f, 1.0f}, 0.4f, indices_count, shader_program);
    }

    glfwPollEvents();
    glfwSwapBuffers(window);
  }

cleanup:
  glDeleteProgram(shader_program);
  glfwTerminate();
  free_mesh(&mesh);
  free_sphere_data(&data);
  free_grid(&grid);
  free(particles.ptr);
}
