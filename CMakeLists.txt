cmake_minimum_required(VERSION 3.14)

project(CoreEngineSDL C)

include(FetchContent)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_SYSTEM_NAME "GNU")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist")
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/toolchain.cmake")
include_directories("${CMAKE_SOURCE_DIR}/vendor")

# SDL
set(SDL2_STATIC ON)
set(SDL2_SHARED OFF)
FetchContent_Declare(
    sdl2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL
    GIT_TAG release-2.30.6
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/SDL"
    SUBBUILD_DIR "${CMAKE_SOURCE_DIR}/CMakeFiles/_deps/sdl2-subbuild"
    BINARY_DIR "${CMAKE_SOURCE_DIR}/CMakeFiles/_deps/sdl2-build"
)
FetchContent_MakeAvailable(sdl2)

# Lua
FetchContent_Declare(
    lib_lua
    GIT_REPOSITORY https://github.com/lua/lua
    GIT_TAG v5.4.7
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/lua"
    SUBBUILD_DIR "${CMAKE_SOURCE_DIR}/CMakeFiles/_deps/lua-subbuild"
    BINARY_DIR "${CMAKE_SOURCE_DIR}/CMakeFiles/_deps/lua-build"
)
FetchContent_MakeAvailable(lib_lua)

# Engine
FetchContent_Declare(
    lib_engine
    GIT_REPOSITORY https://github.com/RodrigoDornelles/codename-videogame-engine
    GIT_TAG main
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/engine"
    SUBBUILD_DIR "${CMAKE_SOURCE_DIR}/CMakeFiles/_deps/engine-subbuild"
    BINARY_DIR "${CMAKE_SOURCE_DIR}/CMakeFiles/_deps/engine-build"
)
FetchContent_MakeAvailable(lib_engine)

# Zig
set(ZIG_VERSION "0.13.0")
set(ZIG_DOWNLOAD "http://ziglang.org/download")
if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Darwin")
    set(ZIG_DOWNLOAD "${ZIG_DOWNLOAD}/${ZIG_VERSION}/zig-macos-${CMAKE_HOST_SYSTEM_PROCESSOR}-${ZIG_VERSION}.tar.xz")
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
    set(ZIG_DOWNLOAD "${ZIG_DOWNLOAD}/${ZIG_VERSION}/zig-linux-${CMAKE_HOST_SYSTEM_PROCESSOR}-${ZIG_VERSION}.tar.xz")
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
    set(ZIG_DOWNLOAD "${ZIG_DOWNLOAD}/${ZIG_VERSION}/zig-windows-${CMAKE_HOST_SYSTEM_PROCESSOR}-${ZIG_VERSION}.zip")
endif()
FetchContent_Populate(
    compiler_zig
    URL "${ZIG_DOWNLOAD}"
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/zig"
    SUBBUILD_DIR "${CMAKE_SOURCE_DIR}/CMakeFiles/_deps/zig-subbuild"
    BINARY_DIR "${CMAKE_SOURCE_DIR}/CMakeFiles/_deps/zig-build"
)
find_program(ZIG zig PATHS "${CMAKE_SOURCE_DIR}/vendor/zig" REQUIRED)

add_executable(engine "src/main.c")
#target_link_libraries(engine PRIVATE lua) -- TODO
target_link_libraries(engine PRIVATE SDL2::SDL2)
