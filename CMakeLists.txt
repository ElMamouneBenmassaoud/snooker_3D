cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
project(snooker)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)

add_library(glad STATIC 3rdParty/glad/src/glad.c)
target_include_directories(glad PUBLIC 3rdParty/glad/include)

find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    add_subdirectory(3rdParty/glfw)
endif()

include_directories(
    3rdParty/glad/include
    3rdParty/glm
    3rdParty/stb
    src
    src/core
    src/scene
    src/utils
)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(snooker ${SOURCES})

target_compile_definitions(snooker PRIVATE
    PATH_TO_SHADERS="${CMAKE_SOURCE_DIR}/shaders"
    PATH_TO_ASSETS="${CMAKE_SOURCE_DIR}/assets"
)

target_link_libraries(snooker glad glfw OpenGL::GL)
