#include <GLFW/glfw3.h>
#include <stdbool.h>

#include "camera.h"
#include "defines.h"
#include "input.h"

float last_mouse_x = 0.0f;
float last_mouse_y = 0.0f;
bool is_first_mouse = true;

void mouse_scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
  Camera *camera = (Camera *)glfwGetWindowUserPointer(window);
  float radius = camera->radius - yoffset;
  if (radius > 0.0f && radius <= MAX_CAMERA_RADIUS) {
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
