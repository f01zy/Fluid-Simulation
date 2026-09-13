cmake_minimum_required(VERSION 3.5)
project(
  fluid-simulation
  VERSION 1.0.0
  DESCRIPTION "OpenGL fluid simulation using Navier-Stokes momentum equations."
  HOMEPAGE_URL "https://github.com/f01zy/Fluid-Simulation"
  LANGUAGES C
)

option(ENABLE_CJSON_TEST OFF)
option(BUILD_SHARED_LIBS OFF)
option(ENABLE_CJSON_UTILS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(OpenGL_GL_PREFERENCE GLVND)
set(TARGET fluid-simulation)
file(GLOB SOURCE ./src/*.c)

add_executable(${TARGET} ${SOURCE})
find_package(OpenGL REQUIRED)
add_subdirectory(./third_party/cjson)
add_subdirectory(./third_party/freetype)
add_subdirectory(./third_party/glfw)
add_subdirectory(./third_party/cglm)
add_subdirectory(./third_party/glad)

target_include_directories(${TARGET} PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cjson
)
target_link_libraries(${TARGET} PRIVATE 
  OpenGL::GL
  cjson
  freetype
  cglm
  glfw
  glad
)

add_custom_command(TARGET ${TARGET} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    "$<TARGET_FILE_DIR:${TARGET}>/resources"

  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders"
    "$<TARGET_FILE_DIR:${TARGET}>/shaders"
)

if (EMSCRIPTEN)
  set_target_properties(${TARGET} PROPERTIES SUFFIX ".js")
  target_link_options(${TARGET} PRIVATE
    "-sUSE_GLFW=3"
    "-sASYNCIFY"
    "-sASSERTIONS"
    "-sEXPORT_ES6=1"
    "-sMODULARIZE"
    "-sEXPORT_NAME=\"fluid_simulation\""
    "-DPLATFORM_WEB"
  )
endif()
