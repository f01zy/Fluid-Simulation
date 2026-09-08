cmake_minimum_required(VERSION 3.5)
project(glad_loader C)

add_library(glad STATIC src/gl.c)
target_include_directories(glad PUBLIC 
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

find_package(OpenGL REQUIRED)
target_link_libraries(glad PRIVATE OpenGL::GL)
