cmake_minimum_required(VERSION 2.8)

project(SC2)

add_subdirectory(ogg)
add_subdirectory(vorbis)
add_subdirectory(SDL)
set (SDL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/SDL/include")
add_subdirectory(SDL_image)
add_subdirectory(sc2)
