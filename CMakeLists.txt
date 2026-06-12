cmake_minimum_required(VERSION 3.28)
project(handmade CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SDL_STATIC ON)
add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)

add_executable(handmade code/sdl3_handmade.cpp)
target_compile_options(handmade PRIVATE -g -Wall -Wextra -O0)

target_link_libraries(handmade PRIVATE SDL3::SDL3-static)
