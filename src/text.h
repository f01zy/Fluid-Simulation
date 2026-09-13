#ifndef TEXT_INCLUDED
#define TEXT_INCLUDED

#include <cglm/cglm.h>
#include <stdbool.h>
#include <stdint.h>

#include "mesh.h"

typedef struct {
  uint32_t texture;
  uint32_t advance;
  ivec2 size;
  ivec2 bearing;
} Character;

typedef struct {
  Character characters[255];
} Font;

void render_text(const Font *font, const Mesh *mesh, const char *text, vec2 pos, float scale, vec3 color, uint32_t shader_program);
bool initialize_font(Font *font, const char *path, int size);
void free_font(const Font *font);

#endif
