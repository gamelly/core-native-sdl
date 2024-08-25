cmake_minimum_required(VERSION 3.14)

project(CoreEngineSDL C)
include(FetchContent)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_SYSTEM_NAME "GNU")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/toolchain.cmake")
include_directories("${CMAKE_SOURCE_DIR}/vendor")

# Zig
set(ZIG_VERSION "0.13.0")
set(ZIG_DOWNLOAD "http://ziglang.org/download")
if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Darwin")
    set(ZIG_DOWNLOAD "${ZIG_DOWNLOAD}/${ZIG_VERSION}/zig-macos-${CMAKE_HOST_SYSTEM_PROCESSOR}-${ZIG_VERSION}.tar.xz")
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
    set(ZIG_DOWNLOAD "${ZIG_DOWNLOAD}/${ZIG_VERSION}/zig-linux-${CMAKE_HOST_SYSTEM_PROCESSOR}-${ZIG_VERSION}.tar.xz")
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
    add_compile_options("-DNOT_USE_GETOPT")
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

# Font
set(TIRESIAS_FONT_URL "https://github.com/jgrande/ginga/raw/master/tool/ginga/fonts/Tiresias.ttf")
set(TIRESIAS_FONT_PATH "${CMAKE_SOURCE_DIR}/vendor/tiresias/Tiresias.ttf")
set(TIRESIAS_HEADER_PATH "${CMAKE_SOURCE_DIR}/vendor/tiresias/font.h")
if(NOT EXISTS ${TIRESIAS_HEADER_PATH})
    file(DOWNLOAD ${TIRESIAS_FONT_URL} ${TIRESIAS_FONT_PATH} SHOW_PROGRESS)
    execute_process(
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/vendor/tiresias"
        COMMAND xxd -i Tiresias.ttf
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(result EQUAL 0)
        file(WRITE ${TIRESIAS_HEADER_PATH} "${output}")
    else()
        message(FATAL_ERROR "Failed to execute xxd: ${error}")
    endif()
endif()

# SDL
set(SDL2_STATIC ON)
set(SDL2_SHARED OFF)
set(BUILD_SHARED_LIBS OFF)
set(SDL2TTF_SAMPLES OFF)
FetchContent_Declare(
    lib_sdl2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL
    GIT_TAG release-2.30.6
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/SDL"
    SUBBUILD_DIR "${CMAKE_SOURCE_DIR}/CMakeFiles/_deps/sdl2-subbuild"
    BINARY_DIR "${CMAKE_SOURCE_DIR}/CMakeFiles/_deps/sdl2-build"
)
FetchContent_MakeAvailable(lib_sdl2)
FetchContent_Declare(
    lib_sdl2_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf
    GIT_TAG release-2.22.0
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/SDL_ttf"
    SUBBUILD_DIR "${CMAKE_SOURCE_DIR}/CMakeFiles/_deps/sdl2-ttf-subbuild"
    BINARY_DIR "${CMAKE_SOURCE_DIR}/CMakeFiles/_deps/sdl2-ttf-build"
)
FetchContent_MakeAvailable(lib_sdl2_ttf)

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
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/vendor/lua/lua")
    execute_process(
        COMMAND make CC=./zig MYCFLAGS="-std=c99"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/vendor/lua"
    )
endif()
add_library(lua-static STATIC IMPORTED)
set_target_properties(lua-static PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/vendor/lua/liblua.a"
)

# Engine
FetchContent_Declare(
    lib_engine
    GIT_REPOSITORY https://github.com/RodrigoDornelles/codename-videogame-engine
    GIT_TAG develop
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/engine"
    SUBBUILD_DIR "${CMAKE_SOURCE_DIR}/CMakeFiles/_deps/engine-subbuild"
    BINARY_DIR "${CMAKE_SOURCE_DIR}/CMakeFiles/_deps/engine-build"
)
FetchContent_MakeAvailable(lib_engine)
set(ENGINE_HEADER "${CMAKE_SOURCE_DIR}/vendor/engine/native.h")
if(NOT EXISTS ${ENGINE_HEADER})
    execute_process(
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/vendor/engine"
        COMMAND ${CMAKE_SOURCE_DIR}/vendor/lua/lua src/cli/main.lua build --core native --bundler
    )
    execute_process(
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/vendor/engine/dist"
        COMMAND  xxd -i main.lua
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(result EQUAL 0)
        file(WRITE ${ENGINE_HEADER} "${output}")
    else()
        message(FATAL_ERROR "Failed to execute xxd: ${error}")
    endif()
endif()

FILE(GLOB engine_files "${CMAKE_SOURCE_DIR}/src/*.c")
add_executable(engine "${engine_files}")
target_link_libraries(engine PRIVATE lua-static)
target_link_libraries(engine PRIVATE SDL2::SDL2-static)
target_link_libraries(engine PRIVATE SDL2_ttf::SDL2_ttf-static)
target_compile_options(engine PRIVATE -static)
