#ifndef SETTINGS_H
#define SETTINGS_H

#include <cglm/cglm.h>
#include <stddef.h>

typedef struct {
  vec3 pos, vel;
} Particle;

typedef struct {
  Particle *ptr;
  size_t len;
} Particles;

typedef struct {
  float flip_ratio;
  float density;
  float dx;
  vec3 res;
  vec3 lc;
  char particles[1024];
} Settings;

bool read_particles(const char *path, Particles *dest);
bool read_settings(const char *path, Settings *dest);

#endif
