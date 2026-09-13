#include <cJSON.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "settings.h"
#include "utility.h"

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
