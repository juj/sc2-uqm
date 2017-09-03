cmake_minimum_required(VERSION 2.8)

project(uqm)

find_package (PkgConfig)
include (FindPackageHandleStandardArgs)
#set(CMAKE_FIND_LIBRARY_PREFIXES "lib")

function (find_libogg return_name)
	pkg_check_modules (PC_LIBOGG QUIET libogg)
	set (LIBOGG_DEFINITIONS ${PC_LIBOGG_CFLAGS_OTHER})

	find_path (LIBOGG_INCLUDE_DIR ogg/ogg.h
				HINTS ${PC_LIBOGG_INCLUDEDIR} ${PC_LIBOGG_INCLUDE_DIRS}
				PATH_SUFFIXES libogg)

	find_library (LIBOGG_LIBRARY NAMES ogg libogg
					HINTS ${PC_LIBOGG_LIBDIR} ${PC_LIBOGG_LIBRARY_DIRS})

	find_package_handle_standard_args (LibOgg  DEFAULT_MSG
										LIBOGG_LIBRARY LIBOGG_INCLUDE_DIR)

	mark_as_advanced (LIBOGG_INCLUDE_DIR LIBOGG_LIBRARY)

	set (LIBOGG_LIBRARIES ${LIBOGG_LIBRARY} PARENT_SCOPE)
	set (LIBOGG_INCLUDE_DIRS ${LIBOGG_INCLUDE_DIR} PARENT_SCOPE)
	set (${return_name} ${LIBOGG_FOUND} PARENT_SCOPE)
endfunction (find_libogg)

function (find_libvorbis return_name)
	pkg_check_modules (PC_LIBVORBIS QUIET libvorbis)
	set (LIBVORBIS_DEFINITIONS ${PC_LIBVORBIS_CFLAGS_OTHER})

	find_path (LIBVORBIS_INCLUDE_DIR vorbis/codec.h
				HINTS ${PC_LIBVORBIS_INCLUDEDIR} ${PC_LIBVORBIS_INCLUDE_DIRS}
				PATH_SUFFIXES libvorbis)

	find_library (LIBVORBIS_LIBRARY NAMES vorbis libvorbis
					HINTS ${PC_LIBVORBIS_LIBDIR} ${PC_LIBVORBIS_LIBRARY_DIRS})

	find_package_handle_standard_args (LibVorbis  DEFAULT_MSG
										LIBVORBIS_LIBRARY LIBVORBIS_INCLUDE_DIR)

	mark_as_advanced (LIBVORBIS_INCLUDE_DIR LIBVORBIS_LIBRARY)

	set (LIBVORBIS_LIBRARIES ${LIBVORBIS_LIBRARY} PARENT_SCOPE)
	set (LIBVORBIS_INCLUDE_DIRS ${LIBVORBIS_INCLUDE_DIR} PARENT_SCOPE)
	set (${return_name} ${LIBVORBIS_FOUND} PARENT_SCOPE)
endfunction (find_libvorbis)

function (find_libvorbisfile return_name)
	pkg_check_modules (PC_LIBVORBISFILE QUIET libvorbisfile)
	set (LIBVORBISFILE_DEFINITIONS ${PC_LIBVORBISFILE_CFLAGS_OTHER})

	find_path (LIBVORBISFILE_INCLUDE_DIR vorbis/vorbisfile.h
				HINTS ${PC_LIBVORBISFILE_INCLUDEDIR} ${PC_LIBVORBISFILE_INCLUDE_DIRS}
				PATH_SUFFIXES libvorbisfile)

	find_library (LIBVORBISFILE_LIBRARY NAMES vorbisfile libvorbisfile
					HINTS ${PC_LIBVORBISFILE_LIBDIR} ${PC_LIBVORBISFILE_LIBRARY_DIRS})

	find_package_handle_standard_args (LibVorbisFile  DEFAULT_MSG
										LIBVORBISFILE_LIBRARY LIBVORBISFILE_INCLUDE_DIR)

	mark_as_advanced (LIBVORBISFILE_INCLUDE_DIR LIBVORBISFILE_LIBRARY)

	set (LIBVORBISFILE_LIBRARIES ${LIBVORBISFILE_LIBRARY} PARENT_SCOPE)
	set (LIBVORBISFILE_INCLUDE_DIRS ${LIBVORBISFILE_INCLUDE_DIR} PARENT_SCOPE)
	set (${return_name} ${LIBVORBISFILE_FOUND} PARENT_SCOPE)
endfunction (find_libvorbisfile)

find_libogg(LIBOGG_FOUND)
find_libvorbis(LIBVORBIS_FOUND)
find_libvorbisfile(LIBVORBISFILE_FOUND)
#	if (LIBOGG_FOUND AND LIBVORBIS_FOUND)

#set(CMAKE_CXX_STANDARD 11)
#add_definitions(-Wall -Wcast-qual -Wextra -Wshadow -fno-exceptions -fno-rtti -pedantic -flto -m64)

find_package(SDL REQUIRED)
find_package(SDL_image REQUIRED)

include_directories(src)
include_directories(src/libs/lua)
include_directories(${SDL_INCLUDE_DIR})
include_directories(cmake) # For pregenerated config_unix.h

add_definitions(-DDEBUG -DGFXMODULE_SDL -DUSE_INTERNAL_MIKMOD -DUSE_INTERNAL_LUA -DTHREADLIB_PTHREAD)
#add_definitions(-DDEBUG -DGFXMODULE_SDL -DHAVE_OPENGL -DUSE_INTERNAL_MIKMOD -DUSE_INTERNAL_LUA -DTHREADLIB_PTHREAD)

file(GLOB gameFiles src/*.c src/uqm/*.c src/uqm/supermelee/*.c src/libs/uio/stdio/*.c)
file(GLOB_RECURSE sourceFiles src/uqm/ships/*.c src/uqm/planets/*.c src/uqm/lua/*.c src/uqm/comm/*.c)

file(GLOB_RECURSE libFiles src/libs/luauqm/*.c src/libs/video/*.c src/libs/mikmod/*.c src/libs/time/*.c src/libs/task/*.c src/libs/strings/*.c src/libs/sound/*.c src/libs/resource/*.c src/libs/memory/*.c src/libs/math/*.c src/libs/list/*.c src/libs/input/*.c src/libs/heap/*.c src/libs/graphics/*.c src/libs/file/*.c src/libs/decomp/*.c src/libs/callback/*.c)

foreach(f lapi.c lbaselib.c lcode.c lctype.c ldebug.c ldump.c lgc.c liolib.c lmathlib.c loadlib.c lopcodes.c lparser.c lstring.c ltable.c ltm.c lvm.c lauxlib.c lbitlib.c lcorolib.c ldblib.c ldo.c lfunc.c linit.c llex.c lmem.c lobject.c loslib.c lstate.c lstrlib.c ltablib.c lundump.c lzio.c)
	list(APPEND libFiles "src/libs/lua/${f}")
endforeach()

foreach(f charhashtable.c defaultfs.c fstypes.c hashtable.c io.c mount.c paths.c uiostream.c utils.c debug.c fileblock.c gphys.c ioaux.c match.c mounttree.c physical.c uioutils.c)
	list(APPEND libFiles "src/libs/uio/${f}")
endforeach()

file(GLOB logFiles src/libs/log/msgbox_stub.c src/libs/log/uqmlog.c)
file(GLOB threadFiles src/libs/threads/*.c src/libs/threads/pthread/*.c)

if (EMSCRIPTEN)
	set(CMAKE_EXECUTABLE_SUFFIX ".html")

#	set(linkFlags "-s DISABLE_EXCEPTION_CATCHING=1 -s TOTAL_MEMORY=2032MB --shell-file ${CMAKE_CURRENT_LIST_DIR}/src/tiny_chess_shell.html --js-library ${CMAKE_CURRENT_LIST_DIR}/src/library_unicode.js")
	set(linkFlags "${linkFlags} -s USE_PTHREADS=1 -s DISABLE_EXCEPTION_CATCHING=1 -s TOTAL_MEMORY=2032MB -s PTHREAD_POOL_SIZE=4 -s ASMFS=1 -s PROXY_TO_PTHREAD=1 -s OFFSCREENCANVAS_SUPPORT=0 -s OFFSCREEN_FRAMEBUFFER=1 --threadprofiler --profiling-funcs")

	set(linkFlags "${linkFlags} -s DISABLE_EXCEPTION_CATCHING=1 -s TOTAL_MEMORY=256MB")
#	set(linkFlagsDebug "-s GL_DEBUG=1 -g2")
endif()

set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

#set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${linkFlagsDebug}") #TODO why does this not work?
set(CMAKE_EXE_LINKER_FLAGS "${linkFlagsDebug} ${linkFlags}")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${linkFlagsDebug} ${linkFlags}")

add_executable(uqm ${sourceFiles} ${gameFiles} ${libFiles} ${threadFiles} ${logFiles})

set_target_properties(uqm PROPERTIES LINK_FLAGS_DEBUG "${linkFlagsDebug} ${linkFlags}")
set_target_properties(uqm PROPERTIES LINK_FLAGS_RELEASE "${linkFlags}")

target_link_libraries(uqm ${SDL_LIBRARY})
target_link_libraries(uqm ${SDLIMAGE_LIBRARY})
target_link_libraries(uqm ${LIBOGG_LIBRARIES})
target_link_libraries(uqm ${LIBVORBIS_LIBRARIES})
target_link_libraries(uqm ${LIBVORBISFILE_LIBRARIES})
target_link_libraries(uqm m)
