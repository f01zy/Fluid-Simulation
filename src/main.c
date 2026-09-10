// --------------------- HEADERS ---------------------

// clang-format off
#include "cglm/vec3.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// clang-format on

// --------------------- DEFINES ---------------------

#define G                   (9.80665f)
#define FPS                 (60.0f)
#define CUSHION             (1.0e-4f)
#define EPSILON             (1.0e-6f)
#define PRESSURE_ITERS      (1000)
#define WIDTH               (600)
#define HEIGHT              (500)
#define INVALID             ((uint32_t)-1)
#define TITLE               ("Fluid Simulation")
#define IX(i, j, k, ny, nz) ((i) * (ny) * (nz) + (j) * (nz) + (k))

// --------------------- TYPES ---------------------

typedef enum { MATERIAL_SOLID, MATERIAL_EMPTY, MATERIAL_FLUID } MaterialType;

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
  float dens;
  float dx;
  vec3 lc, uc;
} Grid;

typedef struct {
  vec3 pos, vel;
} Particle;

// --------------------- UTILITY ---------------------

bool utility_read_file(const char *path, char *buf, size_t size) {
  FILE *file = fopen(path, "r");
  if (!file) return false;
  buf[fread(buf, sizeof(char), size - 1, file)] = '\0';
  fclose(file);
  return true;
}

void initialize_array(size_t nx, size_t ny, size_t nz, fArray *dest) {
  dest->nx = nx;
  dest->ny = ny;
  dest->nz = nz;
  dest->data = calloc(nx * ny * nz, sizeof(*dest->data));
}

void initialize_grid(size_t nx, size_t ny, size_t nz, float dx, const vec3 lc, Grid *grid) {
  grid->nx = nx;
  grid->ny = ny;
  grid->nz = nz;
  grid->dx = dx;

  glm_vec3_copy((float *)lc, grid->lc);
  glm_vec3_add((float *)lc, (vec3){nx * dx, ny * dx, nz * dx}, grid->uc);

  struct {
    size_t nx, ny, nz;
    fArray *arr;
  } arrs[] = {
    {nx, ny, nz, &grid->p},      {nx, ny, nz, &grid->l},      {nx, ny, nz, &grid->r},      {nx, ny, nz, &grid->d},
    {nx, ny, nz, &grid->q},      {nx + 1, ny, nz, &grid->u},  {nx, ny + 1, nz, &grid->v},  {nx, ny, nz + 1, &grid->w},
    {nx + 1, ny, nz, &grid->fu}, {nx, ny + 1, nz, &grid->fv}, {nx, ny, nz + 1, &grid->fw},
  };
  for (int i = 0; i < sizeof(arrs) / sizeof(*arrs); i++) {
    initialize_array(arrs[i].nx, arrs[i].ny, arrs[i].nx, arrs[i].arr);
  }
}

void free_grid(Grid *grid) {
  fArray *arrs[] = {&grid->p, &grid->l, &grid->u, &grid->v, &grid->w, &grid->fu, &grid->fv, &grid->fw};
  for (int i = 0; i < sizeof(arrs) / sizeof(*arrs); i++) {
    free(arrs[i]->data);
  }
}

void zero_out_farray(const fArray *x) {
  size_t size = x->nx * x->ny * x->nz;
  memset(x->data, 0, size);
}

void zero_out_usarray(const usArray *x) {
  size_t size = x->nx * x->ny * x->nz;
  memset(x->data, 0, size);
}

void zero_out_velocities(const Grid *grid) {
  const fArray *arrs[] = {&grid->u, &grid->v, &grid->w, &grid->fu, &grid->fv, &grid->fw};
  for (int i = 0; i < sizeof(arrs) / sizeof(*arrs); i++) {
    zero_out_farray(arrs[i]);
  }
}

void farray_copy(const fArray *source, fArray *dest) {
  size_t len = source->nx * source->ny * source->nz;
  memcpy(dest->data, source->data, len);
}

void farray_plus_equal(fArray *source, const fArray *arr, float scalar) {
  size_t len = source->nx * source->ny * source->nz;
  for (int i = 0; i < len; i++) {
    source->data[i] += arr->data[i] * scalar;
  }
}

void farray_plus_times(fArray *source, const fArray *arr, float scalar) {
  size_t len = source->nx * source->ny * source->nz;
  for (int i = 0; i < len; i++) {
    source->data[i] = arr->data[i] + source->data[i] * scalar;
  }
}

void get_indices(const vec3 pos, float dx, ivec3 dest) {
  for (int i = 0; i < 3; i++) {
    dest[i] = (uint32_t)floorf(pos[i] / dx);
  }
}

void get_weights(const vec3 pos, float dx, ivec3 indices, vec3 dest) {
  vec3 tmp;
  glm_vec3_divs((float *)pos, dx, tmp);
  for (int i = 0; i < 3; i++) {
    dest[i] = tmp[i] - indices[i];
  }
}

void half_shift(const vec3 pos, float dx, Axis cnst, vec3 dest) {
  float half = dx / 2.0f;
  glm_vec3_copy((float *)pos, dest);
  for (int i = AXIS_X; i < AXIS_Z; i++) {
    if (cnst != i) dest[i] -= half;
  }
}

float dot(const fArray *a, const fArray *b) {
  float ans = 0.0f;
  for (int i = 0; i < a->nx; i++) {
    for (int j = 0; j < a->ny; j++) {
      for (int k = 0; k < a->nz; k++) {
        size_t t = IX(i, j, k, a->ny, a->nz);
        ans += a->data[i] * b->data[i];
      }
    }
  }
  return ans;
}

// --------------------- LABELS ---------------------

void set_outer_labels_to_solid(const Grid *grid) {
  float *l = grid->l.data;
  size_t nx = grid->nx, ny = grid->ny, nz = grid->nz;
  for (int j = 0; j < ny; j++) {
    for (int k = 0; k < nz; k++) {
      l[IX(0, j, k, ny, nz)] = l[IX(nx - 1, j, k, ny, nz)] = MATERIAL_SOLID;
    }
  }
  for (int i = 0; i < nx; i++) {
    for (int k = 0; k < nz; k++) {
      l[IX(i, 0, k, ny, nz)] = l[IX(i, ny - 1, k, ny, nz)] = MATERIAL_SOLID;
    }
  }
  for (int i = 0; i < ny; i++) {
    for (int j = 0; j < nz; j++) {
      l[IX(i, j, 0, ny, nz)] = l[IX(i, j, nz - 1, ny, nz)] = MATERIAL_SOLID;
    }
  }
}

void set_inner_labels_to_empty(const Grid *grid) {
  for (int i = 1; i < grid->nx - 1; i++) {
    for (int j = 1; j < grid->ny - 1; j++) {
      for (int k = 1; k < grid->nz - 1; k++) {
        grid->l.data[IX(i, j, k, grid->ny, grid->nz)] = MATERIAL_EMPTY;
      }
    }
  }
}

void clear_cell_labels(const Grid *grid) {
  set_outer_labels_to_solid(grid);
  set_inner_labels_to_empty(grid);
}

void set_particle_cell_to_fluid(const fArray *l, const vec3 shifted_pos, float dx) {
  ivec3 i;
  get_indices((float *)shifted_pos, dx, i);
  l->data[IX(i[0], i[1], i[2], l->ny, l->nz)] = MATERIAL_FLUID;
}

// --------------------- NEIGHBOURS ---------------------

MaterialType get_neighbour_material(const fArray *labels, size_t i, size_t j, size_t k, Direction dir) {
  size_t ny = labels->ny, nz = labels->nz;
  switch (dir) {
  case LEFT:
    return labels->data[IX(i - 1, j, k, ny, nz)];
  case RIGHT:
    return labels->data[IX(i + 1, j, k, ny, nz)];
  case DOWN:
    return labels->data[IX(i, j - 1, k, ny, nz)];
  case UP:
    return labels->data[IX(i, j + 1, k, ny, nz)];
  case BACK:
    return labels->data[IX(i, j, k - 1, ny, nz)];
  case FORWARD:
    return labels->data[IX(i, j, k + 1, ny, nz)];
  }
}

uint16_t update_from_neighbour(uint16_t info, MaterialType material, Direction dir) {
  uint16_t tmp;
  if (material != MATERIAL_SOLID) tmp++;
  if (material != MATERIAL_FLUID) return tmp;
  return tmp | dir;
}

void make_neighbour_material_info(const fArray *labels, const usArray *neighbours) {
  size_t nx = labels->nx, ny = labels->ny, nz = labels->nz;
  zero_out_usarray(neighbours);
  for (int i = 1; i < nx - 1; i++) {
    for (int j = 1; j < ny - 1; j++) {
      for (int k = 1; k < nz - 1; k++) {
        if (labels->data[IX(i, j, k, ny, nz)] != MATERIAL_FLUID) continue;
        uint16_t info = 0;
        for (Direction dir = LEFT; dir < FORWARD; dir++) {
          MaterialType nbr = get_neighbour_material(labels, i, j, k, dir);
          info = update_from_neighbour(info, nbr, dir);
        }
        neighbours->data[IX(i, j, k, ny, nz)] = info;
      }
    }
  }
}

// --------------------- SPLATTING ---------------------

void contribute(float weight, float particle_vel, const fArray *grid_vels, const fArray *grid_wgts, size_t i, size_t j, size_t k) {
  grid_vels->data[IX(i, j, k, grid_vels->ny, grid_vels->nz)] += weight * particle_vel;
  grid_wgts->data[IX(i, j, k, grid_vels->ny, grid_vels->nz)] += weight;
}

void splat(const vec3 shifted_pos, float dx, float particle_vel, const fArray *grid_vels, const fArray *grid_wgts) {
  vec3 shifted_pos_over_dx;
  glm_vec3_scale((float *)shifted_pos, dx, shifted_pos_over_dx);

  ivec3 indices;
  vec3 weights;
  get_indices(shifted_pos_over_dx, dx, indices);
  get_weights(shifted_pos_over_dx, dx, indices, weights);

  float w0 = weights[0];
  float iw0 = 1 - w0;
  float w1 = weights[1];
  float iw1 = 1 - w1;
  float w2 = weights[2];
  float iw2 = 1 - w2;
  size_t i = indices[0];
  size_t j = indices[1];
  size_t k = indices[2];

  contribute(iw0 * iw1 * iw2, particle_vel, grid_vels, grid_wgts, i, j, k);
  contribute(w0 * iw1 * iw2, particle_vel, grid_vels, grid_wgts, i + 1, j, k);
  contribute(iw0 * w1 * iw2, particle_vel, grid_vels, grid_wgts, i, j + 1, k);
  contribute(w0 * w1 * iw2, particle_vel, grid_vels, grid_wgts, i + 1, j + 1, k);
  contribute(iw0 * iw1 * w2, particle_vel, grid_vels, grid_wgts, i, j, k + 1);
  contribute(w0 * iw1 * w2, particle_vel, grid_vels, grid_wgts, i + 1, j, k + 1);
  contribute(iw0 * w1 * w2, particle_vel, grid_vels, grid_wgts, i, j + 1, k + 1);
  contribute(w0 * w1 * w2, particle_vel, grid_vels, grid_wgts, i + 1, j + 1, k + 1);
}

void normalize(const fArray *x, const fArray *fx, const vec2 ix, const vec2 iy, const vec2 iz) {
  for (int i = ix[0]; i < ix[1]; i++) {
    for (int j = iy[0]; j < iy[1]; j++) {
      for (int k = iz[0]; k < iz[0]; k++) {
        size_t t = IX(i, j, k, x->ny, x->nz);
        if (fabsf(fx->data[t]) < EPSILON) {
          x->data[t] = 0.0f;
          continue;
        }
        x->data[t] /= fx->data[t];
      }
    }
  }
}

void handle_boundaries(const Grid *grid) {
  float *u = grid->u.data, *v = grid->v.data, *w = grid->w.data;
  size_t nx = grid->nx, ny = grid->ny, nz = grid->nz;
  for (int j = 0; j < ny; j++) {
    for (int k = 0; k < nz; k++) {
      u[IX(0, j, k, ny, nz)] = u[IX(1, j, k, ny, nz)] = u[IX(nx - 1, j, k, ny, nz)] = u[IX(nx, j, k, ny, nz)];
      v[IX(0, j, k, ny, nz)] = v[IX(1, j, k, ny, nz)];
      w[IX(0, j, k, ny, nz)] = w[IX(1, j, k, ny, nz)];
      v[IX(nx - 1, j, k, ny, nz)] = v[IX(nx - 2, j, k, ny, nz)];
      w[IX(nx - 1, j, k, ny, nz)] = w[IX(nx - 2, j, k, ny, nz)];
    }
  }
  for (int i = 0; i < ny; i++) {
    for (int k = 0; k < nz; k++) {
      v[IX(i, 0, k, ny, nz)] = u[IX(i, 1, k, ny, nz)] = u[IX(i, ny - 1, k, ny, nz)] = u[IX(i, ny, k, ny, nz)];
      u[IX(i, 0, k, ny, nz)] = u[IX(i, 1, k, ny, nz)];
      w[IX(i, 0, k, ny, nz)] = w[IX(i, 1, k, ny, nz)];
      u[IX(i, ny - 1, k, ny, nz)] = v[IX(i, ny - 2, k, ny, nz)];
      w[IX(i, ny - 1, k, ny, nz)] = w[IX(i, ny - 2, k, ny, nz)];
    }
  }
  for (int i = 0; i < ny; i++) {
    for (int j = 0; j < nz; j++) {
      w[IX(i, j, 0, ny, nz)] = w[IX(i, j, 1, ny, nz)] = w[IX(i, j, nz - 1, ny, nz)] = w[IX(i, j, nz, ny, nz)];
      u[IX(i, j, 0, ny, nz)] = u[IX(i, j, 1, ny, nz)];
      v[IX(i, j, 0, ny, nz)] = v[IX(i, j, 1, ny, nz)];
      u[IX(i, j, nz - 1, ny, nz)] = u[IX(i, j, nz - 2, ny, nz)];
      v[IX(i, j, nz - 1, ny, nz)] = v[IX(i, j, nz - 2, ny, nz)];
    }
  }
}

void particles_to_grid(const Grid *grid, const Particle *particles, size_t n) {
  float dx = grid->dx;
  zero_out_velocities(grid);
  clear_cell_labels(grid);

  for (int i = 0; i < n; i++) {
    const Particle *p = &particles[i];
    vec3 shifted_pos;
    glm_vec3_sub((float *)p->pos, (float *)grid->lc, shifted_pos);
    set_particle_cell_to_fluid(&grid->l, shifted_pos, grid->dx);

    vec3 yz, xz, xy;
    half_shift(shifted_pos, dx, AXIS_X, yz);
    half_shift(shifted_pos, dx, AXIS_Y, xz);
    half_shift(shifted_pos, dx, AXIS_Z, xy);
    splat(yz, dx, p->vel[0], &grid->u, &grid->fu);
    splat(xz, dx, p->vel[1], &grid->v, &grid->fv);
    splat(xy, dx, p->vel[2], &grid->w, &grid->fw);
  }

  size_t nx = grid->nx, ny = grid->ny, nz = grid->nz;
  normalize(&grid->u, &grid->fu, (vec2){2, nx - 1}, (vec2){0, ny}, (vec2){0, nz});
  normalize(&grid->v, &grid->fv, (vec2){0, nx}, (vec2){2, ny - 1}, (vec2){0, nz});
  normalize(&grid->w, &grid->fw, (vec2){0, nx}, (vec2){0, ny}, (vec2){2, nz - 1});
  handle_boundaries(grid);
}

// --------------------- ADVECTION ---------------------

float interpolate_velocities(const vec3 shifted_pos, float dx, const fArray *v) {
  vec3 shifted_pos_over_dx;
  glm_vec3_scale((float *)shifted_pos, dx, shifted_pos_over_dx);

  ivec3 indices;
  vec3 weights;
  get_indices(shifted_pos_over_dx, dx, indices);
  get_weights(shifted_pos_over_dx, dx, indices, weights);

  float w0 = weights[0];
  float iw0 = 1 - w0;
  float w1 = weights[1];
  float iw1 = 1 - w1;
  float w2 = weights[2];
  float iw2 = 1 - w2;
  size_t i = indices[0];
  size_t j = indices[1];
  size_t k = indices[2];
  size_t ny = v->ny, nz = v->nz;
  float *s = v->data;

  // clang-format off
  return iw0 * iw1 * iw2 * s[IX(i, j, k, ny, nz)]         +
	 iw0 * iw1 *  w2 * s[IX(i, j, k + 1, ny, nz)]     +
	 iw0 *  w1 * iw2 * s[IX(i, j + 1, k, ny, nz)]     +
	 iw0 *  w1 *  w2 * s[IX(i, j + 1, k + 1, ny, nz)] +
	 w0  * iw1 * iw2 * s[IX(i + 1, j, k, ny, nz)]     +
	 w0  * iw1 * w2  * s[IX(i + 1, j, k + 1, ny, nz)] +
	 w0  * w1  * iw2 * s[IX(i + 1, j + 1, k, ny, nz)] +
	 w0  * w1  * w2  * s[IX(i + 1, j + 1, k + 1, ny, nz)];
  // clang-format on
}

void clamp_to_non_solid_cells(const vec3 pos, const vec3 lc, const vec3 uc, float dx, vec3 dest) {
  vec3 clamped_pos;
  glm_vec3_copy((float *)pos, clamped_pos);
  float border = dx + CUSHION;
  for (int i = 0; i < 3; i++) {
    float min = lc[i] + border;
    if (clamped_pos[i] < min) {
      clamped_pos[i] = min;
      continue;
    }
    float max = uc[i] - border;
    if (clamped_pos[i] > max) clamped_pos[i] = max;
  }
  glm_vec3_copy(clamped_pos, dest);
}

void advect(const Grid *grid, const vec3 pos, float dt, vec3 dest) {
  vec3 shifted_pos;
  glm_vec3_sub((float *)pos, (float *)grid->lc, shifted_pos);
  float up = interpolate_velocities(shifted_pos, grid->dx, &grid->u);
  float vp = interpolate_velocities(shifted_pos, grid->dx, &grid->v);
  float wp = interpolate_velocities(shifted_pos, grid->dx, &grid->w);
  vec3 new_pos;
  glm_vec3_scale((vec3){up, vp, wp}, dt, new_pos);
  glm_vec3_add(new_pos, (float *)pos, new_pos);
  clamp_to_non_solid_cells(new_pos, grid->lc, grid->uc, grid->dx, dest);
}

// --------------------- PHYSICS ---------------------

void apply_gravity(const Grid *grid, float dt) {
  const fArray *v = &grid->v;
  for (int i = 0; i < v->nx; i++) {
    for (int j = 0; j < v->ny + 1; j++) {
      for (int k = 0; k < v->nz; k++) {
        v->data[IX(i, j, k, v->ny, v->nz)] += G * dt;
      }
    }
  }
  handle_boundaries(grid);
}

// --------------------- PRESSURE ---------------------

void make_residual_from_divergence(const Grid *grid) {
  size_t nx = grid->nx, ny = grid->ny, nz = grid->nz;
  for (int i = 1; i < nx - 1; i++) {
    for (int j = 1; j < ny - 1; j++) {
      for (int k = 1; k < nz - 1; k++) {
        size_t t = IX(i, j, k, ny, nz);
        if (grid->l.data[t] != MATERIAL_FLUID) {
          grid->r.data[t] = 0.0f;
          continue;
        }
        float du_dx = grid->u.data[IX(i + 1, j, k, ny, nz)] - grid->u.data[IX(i, j, k, ny, nz)];
        float dv_dy = grid->v.data[IX(i, j + 1, k, ny, nz)] - grid->v.data[IX(i, j, k, ny, nz)];
        float dw_dz = grid->w.data[IX(i, j, k + 1, ny, nz)] - grid->w.data[IX(i, j, k, ny, nz)];
        float divergence = du_dx + dv_dy + dw_dz;
        grid->r.data[t] = -divergence;
      }
    }
  }
}

void a_times_d(const fArray *d, const usArray *neighbours, fArray *q) {
  size_t nx = d->nx, ny = d->ny, nz = d->nz;
  const uint16_t center = 7;
  for (int i = 1; i < nx - 1; i++) {
    for (int j = 1; j < ny - 1; j++) {
      for (int k = 1; k < nz - 1; k++) {
        size_t t = IX(i, j, k, ny, nz);
        uint16_t nbrs = neighbours->data[t];
        if (!nbrs) {
          q->data[t] = 0.0f;
          continue;
        }
        // clang-format off
        q->data[t] = ((nbrs & center)  * d->data[IX(i, j, k, ny, nz)])         -
		     ((nbrs & LEFT)    ? d->data[IX(i - 1, j, k, ny, nz)] : 0) -
		     ((nbrs & DOWN)    ? d->data[IX(i, j - 1, k, ny, nz)] : 0) -
		     ((nbrs & BACK)    ? d->data[IX(i, j, k - 1, ny, nz)] : 0) -
		     ((nbrs & RIGHT)   ? d->data[IX(i + 1, j, k, ny, nz)] : 0) -
		     ((nbrs & UP)      ? d->data[IX(i, j + 1, k, ny, nz)] : 0) -
		     ((nbrs & FORWARD) ? d->data[IX(i, j, k + 1, ny, nz)] : 0);
        // clang-format on
      }
    }
  }
}

void _project_pressure(Grid *grid) {
  zero_out_farray(&grid->p);
  make_residual_from_divergence(grid);
  farray_copy(&grid->r, &grid->d);

  float sigma = dot(&grid->r, &grid->r);
  float tolerance = sigma * EPSILON;

  for (int i = 0; i < PRESSURE_ITERS; i++) {
    a_times_d(&grid->d, &grid->n, &grid->q);
    float alpha = sigma / dot(&grid->d, &grid->q);
    farray_plus_equal(&grid->p, &grid->d, alpha);
    farray_plus_equal(&grid->r, &grid->q, -alpha);
    float sigma_old = sigma;
    sigma = dot(&grid->r, &grid->r);
    float beta = sigma / sigma_old;
    farray_plus_times(&grid->d, &grid->r, beta);
  }
}

void substract_pressure_gradient(const Grid *grid) {
  size_t nx = grid->nx, ny = grid->ny, nz = grid->nz;
  for (int i = 1; i < nx - 1; i++) {
    for (int j = 1; j < ny - 1; j++) {
      for (int k = 1; k < nz - 1; k++) {
        size_t t = IX(i, j, k, ny, nz);
        if (grid->l.data[i] == MATERIAL_SOLID) continue;
        float curr_pressure = grid->p.data[t];
        if (grid->l.data[IX(i - 1, j, k, ny, nz)] != MATERIAL_SOLID) grid->u.data[t] -= curr_pressure - grid->p.data[IX(i - 1, j, k, ny, nz)];
        if (grid->l.data[IX(i, j - 1, k, ny, nz)] != MATERIAL_SOLID) grid->v.data[t] -= curr_pressure - grid->p.data[IX(i, j - 1, k, ny, nz)];
        if (grid->l.data[IX(i, j, k - 1, ny, nz)] != MATERIAL_SOLID) grid->w.data[t] -= curr_pressure - grid->p.data[IX(i, j, k - 1, ny, nz)];
      }
    }
  }
}

void project_pressure(Grid *grid) {
  make_neighbour_material_info(&grid->l, &grid->n);
  _project_pressure(grid);
  substract_pressure_gradient(grid);
}

// --------------------- SHADERS ---------------------

uint32_t create_shader(uint32_t shader_program, GLenum type, const char *path) {
  char buf[8192], info[512];
  int success;
  const char *source = buf;
  bool status = utility_read_file(path, buf, sizeof(buf));
  if (!status) {
    printf("[ERROR] Failed to read the shader's source\n");
    return INVALID;
  }
  uint32_t shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(shader, sizeof(info), NULL, info);
    printf("[ERROR] Failed to compile the shader: %s\n", info);
    return INVALID;
  }
  return shader;
}

uint32_t create_shader_program(const char *vertex_shader_path, const char *fragment_shader_path) {
  uint32_t shader_program = glCreateProgram();
  uint32_t vertex_shader = create_shader(shader_program, GL_VERTEX_SHADER, vertex_shader_path);
  uint32_t fragment_shader = create_shader(shader_program, GL_FRAGMENT_SHADER, fragment_shader_path);
  if (vertex_shader == INVALID || fragment_shader == INVALID) return INVALID;
  char info[512];
  int success;
  glAttachShader(shader_program, vertex_shader);
  glAttachShader(shader_program, fragment_shader);
  glLinkProgram(shader_program);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(shader_program, sizeof(info), NULL, info);
    printf("[ERROR] Failed to link the shader program: %s\n", info);
    return INVALID;
  }
  return shader_program;
}

// --------------------- MAIN LOOP ---------------------

int main() {
  if (!glfwInit()) {
    printf("[ERROR] Failed to initialize GLFW\n");
    return -1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

  GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, TITLE, NULL, NULL);
  if (!window) {
    printf("[ERROR] Failed to create a window\n");
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);
  int version = gladLoadGL(glfwGetProcAddress);
  if (version == 0) {
    printf("[ERROR] Failed to initialize OpenGL context\n");
    glfwTerminate();
    return -1;
  }
  printf("[LOG] Loaded OpenGL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
  glEnable(GL_DEPTH_TEST);

  uint32_t shader_program
    = create_shader_program("/home/f01zy/Programming/Fluid Simulation/src/base.vert", "/home/f01zy/Programming/Fluid Simulation/src/base.frag");
  if (shader_program == INVALID) return -1;

  Grid grid;
  initialize_grid(10, 10, 10, 1.0f, GLM_VEC3_ZERO, &grid);

  while (!glfwWindowShouldClose(window)) {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwPollEvents();
    glfwSwapBuffers(window);
  }

  glDeleteProgram(shader_program);
  glfwTerminate();
  free_grid(&grid);
}
