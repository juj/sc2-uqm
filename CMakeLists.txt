cmake_minimum_required(VERSION 2.8)

project(SC2)

if (MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS=1)
endif()

# -- Ogg --
add_subdirectory(ogg)
include_directories(ogg/include)

# -- Vorbis --
add_subdirectory(vorbis)
include_directories(vorbis/include)

# -- SDL --
add_subdirectory(SDL)
set(SDL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/SDL/include")
include_directories(${SDL_INCLUDE_DIR})

# -- zlib --
set(SKIP_INSTALL_ALL TRUE)
set(ZLIB_SHARED OFF)
set(ZLIB_EXAMPLES OFF)
add_subdirectory(zlib)
set(ZLIB_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/zlib;${CMAKE_BINARY_DIR}/zlib")
set(ZLIB_LIBRARIES	 "${CMAKE_CURRENT_BINARY_DIR}/zlib/Debug/zlibstaticd.lib")

# -- Libpng --
set(PNG_BUILD_ZLIB ON)
set(PNG_SHARED OFF)
set(PNG_EXECUTABLES OFF)
set(PNG_TESTS OFF)
add_subdirectory(libpng)
include_directories(libpng)
include_directories("${CMAKE_BINARY_DIR}/libpng")

# -- SDL_image --
add_subdirectory(SDL_image)
set (SDLIMAGE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/SDL_image")

# -- Main UQM project --
include_directories("sc2/src/regex")
add_subdirectory(sc2)
