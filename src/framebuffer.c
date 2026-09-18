#include <cglm/cglm.h>
#include <glad/gl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "framebuffer.h"

bool create_framebuffer(vec2 screen_size, Framebuffer *dest) {
  uint32_t FBO;
  glGenFramebuffers(1, &FBO);
  glBindFramebuffer(GL_FRAMEBUFFER, FBO);

  uint32_t screen_texture;
  glGenTextures(1, &screen_texture);
  glBindTexture(GL_TEXTURE_2D, screen_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screen_size[0], screen_size[1], 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screen_texture, 0);

  uint32_t RBO;
  glGenRenderbuffers(1, &RBO);
  glBindRenderbuffer(GL_RENDERBUFFER, RBO);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, screen_size[0], screen_size[1]);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    glDeleteTextures(1, &screen_texture);
    glDeleteRenderbuffers(1, &RBO);
    printf("[ERROR] Frabuffer is not complete\n");
    return false;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  *dest = (Framebuffer){
    .screen_texture = screen_texture,
    .RBO = RBO,
    .FBO = FBO,
  };

  return true;
}
