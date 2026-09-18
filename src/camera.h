#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/cglm.h>

typedef struct {
  vec3 pos, dir;
  vec3 up, right;
  float yaw, pitch;
  float radius;
  float fov;
} Camera;

void initialize_camera(Camera *camera);
void update_camera_position(Camera *camera);
void get_camera_view_matrix(const Camera *camera, mat4 view);

#endif
