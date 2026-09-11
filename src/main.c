// --------------------- HEADERS ---------------------

// clang-format off
#include <assert.h>
#include <errno.h>
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
#include <cJSON.h>
// clang-format on

// --------------------- DEFINES ---------------------
#define PI                  (3.141592653589f)
#define G                   (9.80665f)
#define FPS                 (60.0f)
#define CUSHION             (1.0e-4f)
#define EPSILON             (1.0e-6f)
#define WIDTH               (600)
#define HEIGHT              (500)
#define INVALID             ((uint32_t)-1)
#define TITLE               ("Fluid Simulation")
#define PRESSURE_ITERS      (100)
#define MAX_CAMERA_RADIUS   (100.0f)
#define IX(i, j, k, ny, nz) ((i) * (ny) * (nz) + (j) * (nz) + (k))

// --------------------- GLOBAL TYPES ---------------------

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
  float flip_ratio;
  float density;
  float dx;
  vec3 res;
  vec3 lc;
  char particles[1024];
} Settings;

typedef struct {
  vec3 pos, vel;
} Particle;

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

// --------------------- UTILITY ---------------------

bool read_file(const char *path, char *buf, size_t size) {
  FILE *file = fopen(path, "r");
  if (!file) return false;
  buf[fread(buf, sizeof(char), size - 1, file)] = '\0';
  fclose(file);
  return true;
}

void initialize_farray(size_t nx, size_t ny, size_t nz, fArray *dest) {
  dest->nx = nx;
  dest->ny = ny;
  dest->nz = nz;
  dest->data = calloc(nx * ny * nz, sizeof(*dest->data));
}

void initialize_usarray(size_t nx, size_t ny, size_t nz, usArray *dest) {
  dest->nx = nx;
  dest->ny = ny;
  dest->nz = nz;
  dest->data = calloc(nx * ny * nz, sizeof(*dest->data));
}

void initialize_grid(const Settings *settings, Grid *grid) {
  size_t nx = settings->res[0];
  size_t ny = settings->res[1];
  size_t nz = settings->res[2];
  float flip_ratio = settings->flip_ratio;
  float density = settings->density;
  float dx = settings->dx;

  grid->nx = nx;
  grid->ny = ny;
  grid->nz = nz;
  grid->dx = dx;
  grid->dens = density;
  grid->flip_ratio = flip_ratio;

  glm_vec3_copy((float *)settings->lc, grid->lc);
  glm_vec3_add((float *)settings->lc, (vec3){nx * dx, ny * dx, nz * dx}, grid->uc);
  initialize_usarray(ny, ny, nz, &grid->n);

  struct {
    size_t nx, ny, nz;
    fArray *arr;
  } arrs[] = {
    {nx, ny, nz, &grid->p},      {nx, ny, nz, &grid->l},      {nx, ny, nz, &grid->r},      {nx, ny, nz, &grid->d},
    {nx, ny, nz, &grid->q},      {nx + 1, ny, nz, &grid->u},  {nx, ny + 1, nz, &grid->v},  {nx, ny, nz + 1, &grid->w},
    {nx + 1, ny, nz, &grid->fu}, {nx, ny + 1, nz, &grid->fv}, {nx, ny, nz + 1, &grid->fw},
  };
  for (int i = 0; i < sizeof(arrs) / sizeof(*arrs); i++) {
    initialize_farray(arrs[i].nx, arrs[i].ny, arrs[i].nz, arrs[i].arr);
  }
}

void free_grid(Grid *grid) {
  fArray *arrs[] = {&grid->p, &grid->l, &grid->r, &grid->d, &grid->q, &grid->u, &grid->v, &grid->w, &grid->fu, &grid->fv, &grid->fw};
  for (int i = 0; i < sizeof(arrs) / sizeof(*arrs); i++) {
    free(arrs[i]->data);
  }
}

void zero_out_farray(const fArray *x) {
  size_t size = x->nx * x->ny * x->nz * sizeof(float);
  memset(x->data, 0, size);
}

void zero_out_usarray(const usArray *x) {
  size_t size = x->nx * x->ny * x->nz * sizeof(uint16_t);
  memset(x->data, 0, size);
}

void zero_out_velocities(const Grid *grid) {
  const fArray *arrs[] = {&grid->u, &grid->v, &grid->w, &grid->fu, &grid->fv, &grid->fw};
  for (int i = 0; i < sizeof(arrs) / sizeof(*arrs); i++) {
    zero_out_farray(arrs[i]);
  }
}

void farray_copy(const fArray *source, const fArray *dest) {
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

void get_indices(const vec3 shifted_pos, float dx, ivec3 dest) {
  printf("%f, %f, %f\n", shifted_pos[0], shifted_pos[1], shifted_pos[2]);
  assert(shifted_pos[0] >= 0.0f);
  assert(shifted_pos[1] >= 0.0f);
  assert(shifted_pos[2] >= 0.0f);
  for (int i = 0; i < 3; i++) {
    dest[i] = (uint32_t)floorf(shifted_pos[i] / dx);
  }
}

void get_weights(const vec3 shifted_pos, float dx, ivec3 indices, vec3 dest) {
  vec3 tmp;
  glm_vec3_divs((float *)shifted_pos, dx, tmp);
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

size_t get_opengl_type_size(GLenum type) {
  switch (type) {
  case GL_FLOAT:
    return sizeof(GLfloat);
  case GL_INT:
    return sizeof(GLint);
  case GL_UNSIGNED_INT:
    return sizeof(GLuint);
  case GL_SHORT:
    return sizeof(GLshort);
  case GL_UNSIGNED_SHORT:
    return sizeof(GLushort);
  case GL_BYTE:
    return sizeof(GLbyte);
  case GL_UNSIGNED_BYTE:
    return sizeof(GLubyte);
  case GL_DOUBLE:
    return sizeof(GLdouble);
  default:
    return 0;
  }
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
  if (material != MATERIAL_SOLID) info++;
  if (material == MATERIAL_FLUID) info |= dir;
  return info;
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

// --------------------- INTERPOLATION ---------------------

float trilinear_interpolation(const vec3 shifted_pos, float dx, const fArray *v) {
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

void interpolate_velocities(const vec3 pos, const vec3 lc, float dx, const fArray *u, const fArray *v, const fArray *w, vec3 dest) {
  vec3 shifted_pos;
  glm_vec3_sub((float *)pos, (float *)lc, shifted_pos);
  dest[0] = trilinear_interpolation(shifted_pos, dx, u);
  dest[1] = trilinear_interpolation(shifted_pos, dx, v);
  dest[2] = trilinear_interpolation(shifted_pos, dx, w);
}

void interpolate_curr_velocities(const Grid *grid, const vec3 pos, vec3 dest) {
  interpolate_velocities(pos, grid->lc, grid->dx, &grid->u, &grid->v, &grid->w, dest);
}

void interpolate_old_velocities(const Grid *grid, const vec3 pos, vec3 dest) {
  interpolate_velocities(pos, grid->lc, grid->dx, &grid->fu, &grid->fv, &grid->fw, dest);
}

// --------------------- SPLATTING (from particle to grid) ---------------------

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
      for (int k = iz[0]; k < iz[1]; k++) {
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
  farray_copy(&grid->u, &grid->fu);
  farray_copy(&grid->v, &grid->fv);
  farray_copy(&grid->w, &grid->fw);
  handle_boundaries(grid);
}

// --------------------- GATHERING (from grid to particle) ---------------------

void grid_to_particles(const Grid *grid, const Particle *particle, float flip_ratio, vec3 dest) {
  vec3 old_vel, new_vel;
  interpolate_old_velocities(grid, particle->pos, old_vel);
  interpolate_curr_velocities(grid, particle->pos, new_vel);
  glm_vec3_sub((float *)particle->vel, old_vel, dest);
  glm_vec3_scale(dest, flip_ratio, dest);
  glm_vec3_add(dest, new_vel, dest);
}

// --------------------- ADVECTION ---------------------

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
  vec3 interpolation;
  interpolate_velocities(pos, grid->lc, grid->dx, &grid->u, &grid->v, &grid->w, interpolation);
  vec3 new_pos;
  glm_vec3_scale(interpolation, dt, new_pos);
  glm_vec3_add(new_pos, (float *)pos, new_pos);
  clamp_to_non_solid_cells(new_pos, grid->lc, grid->uc, grid->dx, dest);
}

// --------------------- PHYSICS ---------------------

void apply_gravity(const Grid *grid, float dt) {
  const fArray *v = &grid->v;
  for (int i = 0; i < v->nx; i++) {
    for (int j = 0; j < v->ny + 1; j++) {
      for (int k = 0; k < v->nz; k++) {
        v->data[IX(i, j, k, v->ny, v->nz)] -= G * dt;
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
  const fArray *l = &grid->l, *p = &grid->p, *u = &grid->u, *v = &grid->v, *w = &grid->w;
  for (int i = 1; i < nx - 1; i++) {
    for (int j = 1; j < ny - 1; j++) {
      for (int k = 1; k < nz - 1; k++) {
        size_t t = IX(i, j, k, ny, nz);
        // TODO: если заменить ошибочный i на t, позиция становится nan
        if (l->data[t] == MATERIAL_SOLID) continue;
        float curr_pressure = p->data[t];
        if (l->data[IX(i - 1, j, k, ny, nz)] != MATERIAL_SOLID) u->data[IX(i, j, k, u->ny, u->nz)] -= curr_pressure - p->data[IX(i - 1, j, k, ny, nz)];
        if (l->data[IX(i, j - 1, k, ny, nz)] != MATERIAL_SOLID) v->data[IX(i, j, k, v->ny, v->nz)] -= curr_pressure - p->data[IX(i, j - 1, k, ny, nz)];
        if (l->data[IX(i, j, k - 1, ny, nz)] != MATERIAL_SOLID) w->data[IX(i, j, k, w->ny, w->nz)] -= curr_pressure - p->data[IX(i, j, k - 1, ny, nz)];
      }
    }
  }
}

void project_pressure(Grid *grid) {
  make_neighbour_material_info(&grid->l, &grid->n);
  _project_pressure(grid);
  substract_pressure_gradient(grid);
}

// --------------------- READ SETTINGS ---------------------

bool str_to_ul(const char *str, size_t *dest) {
  errno = 0;
  char *end;
  float ans = strtoul(str, &end, 10);
  if (errno == ERANGE || end == str) return false;
  *dest = ans;
  return true;
}

size_t read_particles_count(FILE *file) {
  size_t count = INVALID;
  char buf[256];
  if (!fgets(buf, sizeof(buf), file)) return INVALID;
  if (!str_to_ul(buf, &count)) return INVALID;
  return count;
}

bool read_particle(char *str, Particle *dest) {
  Particle p;
  if (sscanf(str, "%f %f %f %f %f %f", &p.pos[0], &p.pos[1], &p.pos[2], &p.vel[0], &p.vel[1], &p.vel[2]) != 6) return false;
  *dest = p;
  return true;
}

typedef struct {
  Particle *ptr;
  size_t len;
} Particles;

bool read_particles(const char *path, Particles *dest) {
  FILE *file = fopen(path, "r");
  if (!file) {
    printf("[ERROR] Failed to open the particles file\n");
    return false;
  }
  size_t count = read_particles_count(file);
  if (count == INVALID) {
    printf("[ERROR] Failed to read the particles count\n");
    return false;
  }
  Particle *particles = malloc(sizeof(Particle) * count);
  size_t curr = 0;
  char buf[1024];
  while (fgets(buf, sizeof(buf), file)) {
    if (curr >= count) break;
    Particle *p = &particles[curr++];
    if (!read_particle(buf, p)) {
      fclose(file);
      free(particles);
      printf("[ERROR] Failed to read the particle at the %zu line\n", curr + 1);
      return false;
    }
  }
  fclose(file);
  *dest = (Particles){particles, count};
  return true;
}

bool read_vec3_from_json(const cJSON *item, vec3 dest) {
  if (!cJSON_IsArray(item) || cJSON_GetArraySize(item) != 3) return false;
  for (int i = 0; i < 3; i++) {
    dest[i] = cJSON_GetArrayItem(item, i)->valuedouble;
  }
  return true;
}

bool read_settings(const char *path, Settings *dest) {
  char buf[16384];
  if (!read_file(path, buf, sizeof(buf))) {
    printf("[ERROR] Failed to read the settings source\n");
    return false;
  }
  cJSON *json = cJSON_Parse(buf);
  if (!json) {
    const char *err = cJSON_GetErrorPtr();
    printf("[ERROR] Failed to parse settings json data: %s\n", err);
    return false;
  }
  cJSON *flip_ratio = cJSON_GetObjectItem(json, "flip-ratio");
  cJSON *density = cJSON_GetObjectItem(json, "density");
  cJSON *dx = cJSON_GetObjectItem(json, "dx");
  cJSON *particles = cJSON_GetObjectItem(json, "particles");
  cJSON *res_arr = cJSON_GetObjectItem(json, "res");
  cJSON *lc_arr = cJSON_GetObjectItem(json, "lc");
  vec3 res, lc;
  // clang-format off
  if (!(cJSON_IsNumber(flip_ratio) && flip_ratio->valuedouble) ||
      !(cJSON_IsNumber(density)    && density->valuedouble)    ||
      !(cJSON_IsNumber(dx)         && dx->valuedouble)         ||
      !(cJSON_IsString(particles)  && particles->valuestring)  ||
      !read_vec3_from_json(res_arr, res)                       ||
      !read_vec3_from_json(lc_arr, lc)) {
    cJSON_Delete(json);
    printf("[ERROR] Failed to parse settings json values\n");
    return false;
  }
  // clang-format on
  dest->flip_ratio = flip_ratio->valuedouble;
  dest->density = density->valuedouble;
  dest->dx = dx->valuedouble;
  strcpy(dest->particles, particles->valuestring);
  glm_vec3_copy(res, dest->res);
  glm_vec3_copy(lc, dest->lc);
  cJSON_Delete(json);
  return true;
}

// --------------------- SHADERS ---------------------

uint32_t create_shader(uint32_t shader_program, GLenum type, const char *path) {
  char buf[8192], info[512];
  int success;
  const char *source = buf;
  bool status = read_file(path, buf, sizeof(buf));
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

void uniform_set_mat4(uint32_t shader_program, const char *name, mat4 mat) {
  glUniformMatrix4fv(glGetUniformLocation(shader_program, name), 1, GL_FALSE, (float *)mat);
}

void uniform_set_vec3(uint32_t shader_program, const char *name, vec3 vec) { glUniform3fv(glGetUniformLocation(shader_program, name), 1, vec); }

// --------------------- RENDERERS ---------------------

typedef struct {
  vec3 position;
  vec3 normal;
  vec2 texture_coordinates;
} __attribute__((packed)) Vertice;

typedef struct {
  size_t size;
  GLenum type;
} Attribute;

typedef struct {
  unsigned VAO;
  unsigned VBO;
  unsigned IBO;
} Mesh;

typedef struct {
  struct {
    Vertice *buf;
    size_t len;
    size_t size;
  } vertices;

  struct {
    ivec3 *buf;
    size_t len;
    size_t size;
  } indices;
} SphereData;

void initialize_sphere_data(SphereData *data, int sectors, int stacks) {
  memset(data, 0, sizeof(*data));
  float sector_step = PI * 2.0f / (float)sectors;
  float stack_step = PI / (float)stacks;

  for (int i = 0; i <= stacks; i++) {
    float stack_angle = PI / 2.0f - stack_step * (float)i;
    float xz = cosf(stack_angle);
    float y = sinf(stack_angle);

    for (int j = 0; j <= sectors; j++) {
      if (data->vertices.len >= data->vertices.size) {
        data->vertices.size = (data->vertices.size + 1) * 2;
        data->vertices.buf = realloc(data->vertices.buf, sizeof(*data->vertices.buf) * data->vertices.size);
      }
      float sector_angle = sector_step * (float)j;
      float x = xz * sinf(sector_angle);
      float z = xz * cosf(sector_angle);
      size_t len = data->vertices.len;
      data->vertices.buf[len].position[0] = x;
      data->vertices.buf[len].position[1] = y;
      data->vertices.buf[len].position[2] = z;

      data->vertices.buf[len].normal[0] = x;
      data->vertices.buf[len].normal[1] = y;
      data->vertices.buf[len].normal[2] = z;

      float s = (float)j / (float)sectors;
      float t = (float)i / (float)stacks;
      data->vertices.buf[len].texture_coordinates[0] = s;
      data->vertices.buf[len].texture_coordinates[1] = t;
      data->vertices.len++;
    }
  }

  for (int i = 0; i < stacks; i++) {
    int k1 = i * (sectors + 1);
    int k2 = k1 + sectors + 1;
    for (int j = 0; j < sectors; j++, k1++, k2++) {
      if (data->indices.len >= data->indices.size) {
        data->indices.size = (data->indices.size + 1) * 2;
        data->indices.buf = realloc(data->indices.buf, sizeof(*data->indices.buf) * data->indices.size);
      }
      if (i) {
        size_t len = data->indices.len;
        data->indices.buf[len][0] = k1;
        data->indices.buf[len][1] = k2;
        data->indices.buf[len][2] = k1 + 1;
        data->indices.len++;
      }
      if (i != (stacks - 1)) {
        size_t len = data->indices.len;
        data->indices.buf[len][0] = k1 + 1;
        data->indices.buf[len][1] = k2;
        data->indices.buf[len][2] = k2 + 1;
        data->indices.len++;
      }
    }
  }
}

void free_sphere_data(const SphereData *data) {
  free(data->vertices.buf);
  free(data->indices.buf);
}

size_t get_sphere_vertices_size(const SphereData *data) { return sizeof(*data->vertices.buf) * data->vertices.len; }

size_t get_sphere_indices_size(const SphereData *data) { return sizeof(*data->indices.buf) * data->indices.len; }

bool initialize_mesh(Mesh *mesh, Vertice *vertices, size_t vertices_len, ivec3 *indices, size_t indices_len, Attribute *attributes, size_t attributes_len,
                     GLenum render_mode) {
  for (int i = 0; i < attributes_len; i++) {
    size_t type_size = get_opengl_type_size(attributes[i].type);
    if (type_size == 0) {
      printf("[ERROR] Invalid attribute type\n");
      return false;
    }
  }

  glGenVertexArrays(1, &mesh->VAO);
  glBindVertexArray(mesh->VAO);

  glGenBuffers(1, &mesh->VBO);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
  glBufferData(GL_ARRAY_BUFFER, vertices_len, vertices, render_mode);
  size_t stride = 0;
  size_t offset = 0;
  for (int i = 0; i < attributes_len; i++) {
    stride += get_opengl_type_size(attributes[i].type) * attributes[i].size;
  }
  for (int i = 0; i < attributes_len; i++) {
    const Attribute *attribute = &attributes[i];
    glVertexAttribPointer(i, attribute->size, attribute->type, false, stride, (void *)offset);
    glEnableVertexAttribArray(i);
    offset += get_opengl_type_size(attribute->type) * attribute->size;
  }

  if (indices) {
    glGenBuffers(1, &mesh->IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_len, indices, GL_STATIC_DRAW);
  }

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  return true;
}

void free_mesh(const Mesh *mesh) {
  glDeleteVertexArrays(1, &mesh->VAO);
  glDeleteBuffers(1, &mesh->VBO);
  glDeleteBuffers(1, &mesh->IBO);
}

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

// --------------------- CAMERA ---------------------

typedef struct {
  vec3 pos, dir;
  vec3 up, right;
  float yaw, pitch;
  float radius;
  float fov;
} Camera;

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
  camera->yaw = 0.0f;
  camera->pitch = 0.0f;
  camera->radius = 10.0f;
  update_camera_position(camera);
}

void get_camera_view_matrix(Camera *camera, mat4 view) { return glm_lookat(camera->pos, target, camera->up, view); }

// --------------------- INPUT ---------------------

float last_mouse_x = 0.0f;
float last_mouse_y = 0.0f;
bool is_first_mouse = true;

void mouse_scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
  Camera *camera = (Camera *)glfwGetWindowUserPointer(window);
  float radius = camera->radius - yoffset;
  if (radius >= 0.0f && radius <= MAX_CAMERA_RADIUS) {
    camera->radius = radius;
    update_camera_position(camera);
  }
}

void mouse_position_callback(GLFWwindow *window, double xpos, double ypos) {
  Camera *camera = (Camera *)glfwGetWindowUserPointer(window);
  if (is_first_mouse) {
    last_mouse_x = xpos;
    last_mouse_y = ypos;
    is_first_mouse = false;
  }
  float xoffset = xpos - last_mouse_x;
  float yoffset = ypos - last_mouse_y;
  last_mouse_x = xpos;
  last_mouse_y = ypos;
  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
    camera->pitch = glm_clamp(camera->pitch + yoffset, -89.0f, 89.0f);
    camera->yaw += xoffset;
    update_camera_position(camera);
  }
}

// --------------------- MAIN LOOP ---------------------

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

  Camera camera;
  initialize_camera(&camera);

  const char *settings_path = argv[1];
  GLFWwindow *window = create_window(WIDTH, HEIGHT, TITLE, &camera);
  if (!window) return -1;

  uint32_t shader_program
    = create_shader_program("/home/f01zy/Programming/Fluid Simulation/src/base.vert", "/home/f01zy/Programming/Fluid Simulation/src/base.frag");
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
      draw_sphere(&mesh, p->pos, (vec3){1.0f, 1.0f, 1.0f}, 1.0f, indices_count, shader_program);
    }

    printf("FPS: %f\n", 1.0f / dt);
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
