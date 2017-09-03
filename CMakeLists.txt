cmake_minimum_required(VERSION 2.8)

project(SC2)

add_subdirectory(ogg)
add_subdirectory(vorbis)
add_subdirectory(SDL)
set (SDL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/SDL/include")
include_directories(${SDL_INCLUDE_DIR})

add_subdirectory(libpng)
include_directories(libpng)
include_directories("${CMAKE_BINARY_DIR}/libpng")

add_subdirectory(SDL_image)
set (SDLIMAGE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/SDL_image")

add_subdirectory(sc2)
