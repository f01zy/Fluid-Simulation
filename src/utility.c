#include <glad/gl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "utility.h"

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

bool read_file(const char *path, char *buf, size_t size) {
  FILE *file = fopen(path, "r");
  if (!file) return false;
  buf[fread(buf, sizeof(char), size - 1, file)] = '\0';
  fclose(file);
  return true;
}
