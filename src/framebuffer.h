#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <cglm/cglm.h>
#include <stdbool.h>

typedef struct {
  uint32_t screen_texture;
  uint32_t RBO;
  uint32_t FBO;
} Framebuffer;

bool create_framebuffer(vec2 screen_size, Framebuffer *dest);

#endif
