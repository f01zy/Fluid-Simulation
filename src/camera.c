#include <cglm/cglm.h>
#include <math.h>

#include "camera.h"

vec3 world_up = {0.0f, 1.0f, 0.0f};
vec3 target = {0.0f, 0.0f, 0.0f};

void update_camera_position(Camera *camera) {
  vec3 temp, direction;
  temp[0] = camera->radius * cos(glm_rad(camera->yaw)) * cos(glm_rad(camera->pitch));
  temp[1] = camera->radius * sin(glm_rad(camera->pitch));
  temp[2] = camera->radius * sin(glm_rad(camera->yaw)) * cos(glm_rad(camera->pitch));
  glm_vec3_copy(temp, camera->pos);
  glm_vec3_sub(target, camera->pos, camera->dir);
  glm_normalize(camera->dir);
  glm_cross(camera->dir, world_up, camera->right);
  glm_normalize(camera->right);
  glm_cross(camera->right, camera->dir, camera->up);
  glm_normalize(camera->up);
}

void initialize_camera(Camera *camera) {
  camera->fov = 45.0f;
  camera->yaw = 90.0f;
  camera->pitch = 45.0f;
  camera->radius = 10.0f;
  update_camera_position(camera);
}

void get_camera_view_matrix(Camera *camera, mat4 view) { return glm_lookat(camera->pos, target, camera->up, view); }
