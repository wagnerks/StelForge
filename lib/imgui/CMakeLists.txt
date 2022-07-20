cmake_minimum_required(VERSION 3.0)
project(Imgui)

file(GLOB_RECURSE SRC
    "${CMAKE_SOURCE_DIR}/lib/imgui/*.cpp"
    "${CMAKE_SOURCE_DIR}/lib/imgui/*.h"
)

add_library(imgui ${SRC})
include_directories(${CMAKE_SOURCE_DIR}/lib/glfw/include)
target_include_directories(imgui PUBLIC include/)
