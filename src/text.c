#include <freetype/freetype.h>
#include <glad/gl.h>
#include <stdbool.h>
#include <stdint.h>

#include "mesh.h"
#include "shader.h"
#include "text.h"

bool initialize_font(Font *font, const char *path, int size) {
  FT_Library ft;
  FT_Face face;
  if (FT_Init_FreeType(&ft)) return false;
  if (FT_New_Face(ft, path, 0, &face)) return false;
  FT_Set_Pixel_Sizes(face, 0, size);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  for (int i = 0; i < 255; i++) {
    if (FT_Load_Char(face, i, FT_LOAD_RENDER)) continue;
    Character *character = &font->characters[i];
    uint32_t texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, face->glyph->bitmap.width, face->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    character->texture = texture;
    character->advance = face->glyph->advance.x;
    character->size[0] = face->glyph->bitmap.width;
    character->size[1] = face->glyph->bitmap.rows;
    character->bearing[0] = face->glyph->bitmap_left;
    character->bearing[1] = face->glyph->bitmap_top;
  }

  FT_Done_Face(face);
  FT_Done_FreeType(ft);

  return true;
}

void render_text(const Font *font, const Mesh *mesh, const char *text, vec2 pos, float scale, vec3 color, uint32_t shader_program) {
  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(mesh->VAO);
  uniform_set_vec3(shader_program, "text_color", color);

  float x = pos[0];
  float y = pos[1];

  for (int i = 0; i < strlen(text); i++) {
    const Character *character = &font->characters[text[i]];
    float xpos = x + character->bearing[0] * scale;
    float ypos = y - (character->size[1] - character->bearing[1]) * scale;
    float w = character->size[0] * scale;
    float h = character->size[1] * scale;
    vec4 vertices[4] = {
      {xpos, ypos + h, 0.0f, 1.0f},
      {xpos + w, ypos + h, 1.0f, 1.0f},
      {xpos + w, ypos, 1.0f, 0.0f},
      {xpos, ypos, 0.0f, 0.0f},
    };
    glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, character->texture);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    x += (character->advance >> 6) * scale;
  }

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void free_font(const Font *font) {
  for (int i = 0; i < 255; i++) {
    glDeleteTextures(1, &font->characters[i].texture);
  }
}
