#ifndef GRID_H
#define GRID_H

#include <cglm/cglm.h>
#include <stddef.h>
#include <stdint.h>

#include "settings.h"

// clang-format off
typedef enum {
  LEFT    = 1 << 3,
  DOWN    = 1 << 4,
  BACK    = 1 << 5,
  RIGHT   = 1 << 6,
  UP      = 1 << 7,
  FORWARD = 1 << 8,
} Direction;
// clang-format on

typedef enum { MATERIAL_SOLID, MATERIAL_EMPTY, MATERIAL_FLUID } MaterialType;

typedef enum { AXIS_X, AXIS_Y, AXIS_Z } Axis;

typedef struct {
  size_t nx, ny, nz;
  float *data;
} fArray;

typedef struct {
  size_t nx, ny, nz;
  uint16_t *data;
} usArray;

typedef struct {
  size_t nx, ny, nz;
  usArray n;
  fArray p, l, r, d, q;
  fArray u, v, w;
  fArray fu, fv, fw;
  vec3 lc, uc;
  float flip_ratio;
  float dens;
  float dx;
} Grid;

void initialize_grid(const Settings *settings, Grid *grid);
void free_grid(Grid *grid);
void particles_to_grid(const Grid *grid, const Particle *particles, size_t n);
void grid_to_particles(const Grid *grid, const Particle *particle, float flip_ratio, vec3 dest);
void advect(const Grid *grid, const vec3 pos, float dt, vec3 dest);
void apply_gravity(const Grid *grid, float dt);
void project_pressure(Grid *grid);

#endif
