cmake_minimum_required(VERSION 3.22)
project(monado-inspector)

set(OpenGL_GL_PREFERENCE "GLVND")
find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

add_subdirectory(imgui)

add_executable(inspector
    ${CMAKE_CURRENT_SOURCE_DIR}/inspector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/socket-client.cpp
)

target_include_directories(inspector PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(inspector
    OpenGL::GLX
    imgui
    glfw
)