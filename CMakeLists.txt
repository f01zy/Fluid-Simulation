cmake_minimum_required(VERSION 3.5)
project(fluid-simulation C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(OpenGL_GL_PREFERENCE GLVND)
set(TARGET fluid-simulation)
file(GLOB SOURCE ./src/*.c)

add_executable(${TARGET} ${SOURCE})
find_package(OpenGL REQUIRED)
add_subdirectory(./third_party/cjson)
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
  cglm
  glfw
  glad
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
