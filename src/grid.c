#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "defines.h"
#include "grid.h"
#include "settings.h"

void zero_out_farray(const fArray *x) {
  size_t size = x->nx * x->ny * x->nz * sizeof(float);
  memset(x->data, 0, size);
}

void zero_out_usarray(const usArray *x) {
  size_t size = x->nx * x->ny * x->nz * sizeof(uint16_t);
  memset(x->data, 0, size);
}

void farray_copy(const fArray *source, const fArray *dest) {
  size_t size = source->nx * source->ny * source->nz * sizeof(*source->data);
  memcpy(dest->data, source->data, size);
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

void zero_out_velocities(const Grid *grid) {
  const fArray *arrs[] = {&grid->u, &grid->v, &grid->w, &grid->fu, &grid->fv, &grid->fw};
  for (int i = 0; i < sizeof(arrs) / sizeof(*arrs); i++) {
    zero_out_farray(arrs[i]);
  }
}

void get_indices(const vec3 shifted_pos, float dx, ivec3 dest) {
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
  for (int i = AXIS_X; i <= AXIS_Z; i++) {
    if (cnst != i) dest[i] -= half;
  }
}

float dot(const fArray *a, const fArray *b) {
  float ans = 0.0f;
  for (int i = 0; i < a->nx; i++) {
    for (int j = 0; j < a->ny; j++) {
      for (int k = 0; k < a->nz; k++) {
        size_t t = IX(i, j, k, a->ny, a->nz);
        ans += a->data[t] * b->data[t];
      }
    }
  }
  return ans;
}

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

float trilinear_interpolation(const vec3 shifted_pos, float dx, const fArray *v) {
  ivec3 indices;
  vec3 weights;
  get_indices(shifted_pos, dx, indices);
  get_weights(shifted_pos, dx, indices, weights);

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
  vec3 yz, xz, xy;
  half_shift(shifted_pos, dx, AXIS_X, yz);
  half_shift(shifted_pos, dx, AXIS_Y, xz);
  half_shift(shifted_pos, dx, AXIS_Z, xy);
  dest[0] = trilinear_interpolation(yz, dx, u);
  dest[1] = trilinear_interpolation(xz, dx, v);
  dest[2] = trilinear_interpolation(xy, dx, w);
}

void interpolate_curr_velocities(const Grid *grid, const vec3 pos, vec3 dest) {
  interpolate_velocities(pos, grid->lc, grid->dx, &grid->u, &grid->v, &grid->w, dest);
}

void interpolate_old_velocities(const Grid *grid, const vec3 pos, vec3 dest) {
  interpolate_velocities(pos, grid->lc, grid->dx, &grid->fu, &grid->fv, &grid->fw, dest);
}

void contribute(float weight, float particle_vel, const fArray *grid_vels, const fArray *grid_wgts, size_t i, size_t j, size_t k) {
  grid_vels->data[IX(i, j, k, grid_vels->ny, grid_vels->nz)] += weight * particle_vel;
  grid_wgts->data[IX(i, j, k, grid_vels->ny, grid_vels->nz)] += weight;
}

void splat(const vec3 shifted_pos, float dx, float particle_vel, const fArray *grid_vels, const fArray *grid_wgts) {
  ivec3 indices;
  vec3 weights;
  get_indices(shifted_pos, dx, indices);
  get_weights(shifted_pos, dx, indices, weights);

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
  size_t unx = grid->u.nx, uny = grid->u.ny, unz = grid->u.nz;
  size_t vnx = grid->v.nx, vny = grid->v.ny, vnz = grid->v.nz;
  size_t wnx = grid->w.nx, wny = grid->w.ny, wnz = grid->w.nz;
  for (int j = 0; j < ny; j++) {
    for (int k = 0; k < nz; k++) {
      u[IX(0, j, k, uny, unz)] = u[IX(1, j, k, uny, unz)] = u[IX(nx - 1, j, k, uny, unz)] = u[IX(nx, j, k, uny, unz)] = 0.0f;
      v[IX(0, j, k, vny, vnz)] = v[IX(1, j, k, vny, vnz)];
      w[IX(0, j, k, wny, wnz)] = w[IX(1, j, k, wny, wnz)];
      v[IX(nx - 1, j, k, vny, vnz)] = v[IX(nx - 2, j, k, vny, vnz)];
      w[IX(nx - 1, j, k, wny, wnz)] = w[IX(nx - 2, j, k, wny, wnz)];
    }
  }
  for (int i = 0; i < nx; i++) {
    for (int k = 0; k < nz; k++) {
      v[IX(i, 0, k, vny, vnz)] = v[IX(i, 1, k, vny, vnz)] = v[IX(i, ny - 1, k, vny, vnz)] = v[IX(i, ny, k, vny, vnz)] = 0.0f;
      u[IX(i, 0, k, uny, unz)] = u[IX(i, 1, k, uny, unz)];
      w[IX(i, 0, k, wny, wnz)] = w[IX(i, 1, k, wny, wnz)];
      u[IX(i, ny - 1, k, uny, unz)] = u[IX(i, ny - 2, k, uny, unz)];
      w[IX(i, ny - 1, k, wny, wnz)] = w[IX(i, ny - 2, k, wny, wnz)];
    }
  }
  for (int i = 0; i < nx; i++) {
    for (int j = 0; j < ny; j++) {
      w[IX(i, j, 0, wny, wnz)] = w[IX(i, j, 1, wny, wnz)] = w[IX(i, j, nz - 1, wny, wnz)] = w[IX(i, j, nz, wny, wnz)] = 0.0f;
      u[IX(i, j, 0, uny, unz)] = u[IX(i, j, 1, uny, unz)];
      v[IX(i, j, 0, vny, vnz)] = v[IX(i, j, 1, vny, vnz)];
      u[IX(i, j, nz - 1, uny, unz)] = u[IX(i, j, nz - 2, uny, unz)];
      v[IX(i, j, nz - 1, vny, vnz)] = v[IX(i, j, nz - 2, vny, vnz)];
    }
  }
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

void make_residual_from_divergence(const Grid *grid) {
  size_t nx = grid->nx, ny = grid->ny, nz = grid->nz;
  const fArray *u = &grid->u, *v = &grid->v, *w = &grid->w;
  for (int i = 1; i < nx - 1; i++) {
    for (int j = 1; j < ny - 1; j++) {
      for (int k = 1; k < nz - 1; k++) {
        size_t t = IX(i, j, k, ny, nz);
        if (grid->l.data[t] != MATERIAL_FLUID) {
          grid->r.data[t] = 0.0f;
          continue;
        }
        float du_dx = u->data[IX(i + 1, j, k, u->ny, u->nz)] - u->data[IX(i, j, k, u->ny, u->nz)];
        float dv_dy = v->data[IX(i, j + 1, k, v->ny, v->nz)] - v->data[IX(i, j, k, v->ny, v->nz)];
        float dw_dz = w->data[IX(i, j, k + 1, w->ny, w->nz)] - w->data[IX(i, j, k, w->ny, w->nz)];
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
  if (fabsf(sigma) < EPSILON) return;

  for (int i = 0; i < PRESSURE_ITERS; i++) {
    a_times_d(&grid->d, &grid->n, &grid->q);
    float d_dot_q = dot(&grid->d, &grid->q);
    if (fabsf(d_dot_q) < EPSILON) break;
    float alpha = sigma / d_dot_q;
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
        if (l->data[t] == MATERIAL_SOLID) continue;
        float curr_pressure = p->data[t];
        if (l->data[IX(i - 1, j, k, ny, nz)] != MATERIAL_SOLID) u->data[IX(i, j, k, u->ny, u->nz)] -= curr_pressure - p->data[IX(i - 1, j, k, ny, nz)];
        if (l->data[IX(i, j - 1, k, ny, nz)] != MATERIAL_SOLID) v->data[IX(i, j, k, v->ny, v->nz)] -= curr_pressure - p->data[IX(i, j - 1, k, ny, nz)];
        if (l->data[IX(i, j, k - 1, ny, nz)] != MATERIAL_SOLID) w->data[IX(i, j, k, w->ny, w->nz)] -= curr_pressure - p->data[IX(i, j, k - 1, ny, nz)];
      }
    }
  }
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
  initialize_usarray(nx, ny, nz, &grid->n);

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

  normalize(&grid->u, &grid->fu, (vec2){0, grid->u.nx}, (vec2){0, grid->u.ny}, (vec2){0, grid->u.nz});
  normalize(&grid->v, &grid->fv, (vec2){0, grid->v.nx}, (vec2){0, grid->v.ny}, (vec2){0, grid->v.nz});
  normalize(&grid->w, &grid->fw, (vec2){0, grid->w.nx}, (vec2){0, grid->w.ny}, (vec2){0, grid->w.nz});
  farray_copy(&grid->u, &grid->fu);
  farray_copy(&grid->v, &grid->fv);
  farray_copy(&grid->w, &grid->fw);
  handle_boundaries(grid);
}

void grid_to_particles(const Grid *grid, const Particle *particle, float flip_ratio, vec3 dest) {
  vec3 old_vel, new_vel;
  interpolate_old_velocities(grid, particle->pos, old_vel);
  interpolate_curr_velocities(grid, particle->pos, new_vel);
  glm_vec3_sub((float *)particle->vel, old_vel, dest);
  glm_vec3_scale(dest, flip_ratio, dest);
  glm_vec3_add(dest, new_vel, dest);
}

void advect(const Grid *grid, const vec3 pos, float dt, vec3 dest) {
  vec3 interpolation;
  interpolate_velocities(pos, grid->lc, grid->dx, &grid->u, &grid->v, &grid->w, interpolation);
  vec3 new_pos;
  glm_vec3_scale(interpolation, dt, new_pos);
  glm_vec3_add(new_pos, (float *)pos, new_pos);
  clamp_to_non_solid_cells(new_pos, grid->lc, grid->uc, grid->dx, dest);
}

void apply_gravity(const Grid *grid, float dt) {
  const fArray *v = &grid->v;
  for (int i = 0; i < v->nx; i++) {
    for (int j = 0; j < v->ny; j++) {
      for (int k = 0; k < v->nz; k++) {
        v->data[IX(i, j, k, v->ny, v->nz)] -= G * dt;
      }
    }
  }
  handle_boundaries(grid);
}

void project_pressure(Grid *grid) {
  make_neighbour_material_info(&grid->l, &grid->n);
  _project_pressure(grid);
  substract_pressure_gradient(grid);
}
