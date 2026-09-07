cmake_minimum_required(VERSION 3.19)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(libsdl2_plugin LANGUAGES C)
    add_library(gecnd_header INTERFACE)
    target_include_directories(gecnd_header INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "libsdl2_plugin requires Linux DMA-BUF and EGL native fences")
endif()

include(FetchContent)
FetchContent_Declare(gecnd_sdl2
    URL https://www.libsdl.org/release/SDL2-2.32.10.tar.gz
    URL_HASH SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
)
FetchContent_GetProperties(gecnd_sdl2)
if(NOT gecnd_sdl2_POPULATED)
    FetchContent_Populate(gecnd_sdl2)
endif()

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL_OFFSCREEN ON CACHE BOOL "" FORCE)
set(SDL_X11 OFF CACHE BOOL "" FORCE)
set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
set(SDL_KMSDRM OFF CACHE BOOL "" FORCE)
add_subdirectory("${gecnd_sdl2_SOURCE_DIR}" "${gecnd_sdl2_BINARY_DIR}" EXCLUDE_FROM_ALL)

set_property(SOURCE "${gecnd_sdl2_SOURCE_DIR}/src/video/offscreen/SDL_offscreenopengles.c"
    TARGET_DIRECTORY SDL2 APPEND PROPERTY COMPILE_DEFINITIONS OFFSCREEN_GLES_SwapWindow=gecnd_upstream_swap)
set_property(SOURCE "${gecnd_sdl2_SOURCE_DIR}/src/video/offscreen/SDL_offscreenevents.c"
    TARGET_DIRECTORY SDL2 APPEND PROPERTY COMPILE_DEFINITIONS OFFSCREEN_PumpEvents=gecnd_upstream_pump)

target_sources(SDL2 PRIVATE client.c hooks.c gpu.c ipc.c)
target_include_directories(SDL2 PRIVATE "${gecnd_sdl2_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(SDL2 PRIVATE EGL GL)
set_target_properties(SDL2 PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/sdl2")

add_library(sdl2_gecnd SHARED main.c process.c gpu.c ipc.c)
target_link_libraries(sdl2_gecnd PRIVATE gecnd_header EGL GL dl)
target_compile_options(sdl2_gecnd PRIVATE -Wall -Wextra)
set_target_properties(sdl2_gecnd PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
add_custom_target(libsdl2_plugin ALL DEPENDS sdl2_gecnd SDL2)
