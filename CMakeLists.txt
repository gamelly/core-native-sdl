cmake_minimum_required(VERSION 3.19)
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)

project(CoreEngineSDL C)
include(FetchContent)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
include_directories("${CMAKE_SOURCE_DIR}/src")
include_directories("${CMAKE_SOURCE_DIR}/vendor")

# Zig
set(ZIG_VERSION "0.13.0")
set(ZIG_DOWNLOAD "http://ziglang.org/download")
set(ZIG_CC "${CMAKE_SOURCE_DIR}/vendor/zig/zig_cc")
set(ZIG_CXX "${CMAKE_SOURCE_DIR}/vendor/zig/zig_cxx")
#set(CMAKE_C_COMPILER ${ZIG_CC} CACHE BOOL "" FORCE) -- TODO: make work
#set(CMAKE_CXX_COMPILER ${ZIG_CXX} CACHE BOOL "" FORCE)
if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Darwin")
    set(ZIG_DOWNLOAD "${ZIG_DOWNLOAD}/${ZIG_VERSION}/zig-macos-${CMAKE_HOST_SYSTEM_PROCESSOR}-${ZIG_VERSION}.tar.xz")
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
    set(ZIG_DOWNLOAD "${ZIG_DOWNLOAD}/${ZIG_VERSION}/zig-linux-${CMAKE_HOST_SYSTEM_PROCESSOR}-${ZIG_VERSION}.tar.xz")
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
    add_compile_options("-DNOT_USE_GETOPT")
    set(ZIG_DOWNLOAD "${ZIG_DOWNLOAD}/${ZIG_VERSION}/zig-windows-${CMAKE_HOST_SYSTEM_PROCESSOR}-${ZIG_VERSION}.zip")
endif()
if(NOT EXISTS ${ZIG_CC})
    FetchContent_Populate(
        compiler_zig
        URL "${ZIG_DOWNLOAD}"
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/zig"
        SUBBUILD_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/_deps/zig-subbuild"
        BINARY_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/_deps/zig-build"
    )
    file(WRITE ${ZIG_CC} "#!/bin/sh\n${CMAKE_SOURCE_DIR}/vendor/zig/zig cc \$@\n")
    file(WRITE ${ZIG_CXX} "#!/bin/sh\n${CMAKE_SOURCE_DIR}/vendor/zig/zig c++ \$@\n")
    file(CHMOD ${ZIG_CC} FILE_PERMISSIONS OWNER_EXECUTE OWNER_READ)
    file(CHMOD ${ZIG_CXX} FILE_PERMISSIONS OWNER_EXECUTE OWNER_READ)
endif()
find_program(ZIGC zig PATHS "${CMAKE_SOURCE_DIR}/vendor/zig" REQUIRED)

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
        message(FATAL_ERROR "Failed to execute xxd tiresias: ${error}")
    endif()
endif()

# Lua
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/vendor/lua/lua")
    FetchContent_Declare(
        lib_lua
        GIT_REPOSITORY https://github.com/lua/lua
        GIT_TAG v5.4.7
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/lua"
        SUBBUILD_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/_deps/lua-subbuild"
        BINARY_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/_deps/lua-build"
    )
    FetchContent_MakeAvailable(lib_lua)
    execute_process(
        COMMAND make CC=${CMAKE_SOURCE_DIR}/vendor/zig/zig_cc MYCFLAGS="-std=c99" MYLIBS="" 
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/vendor/lua"
    )
endif()
add_library(lua-static STATIC IMPORTED)
set_target_properties(lua-static PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/vendor/lua/liblua.a"
)
find_program(LUAC lua PATHS "${CMAKE_SOURCE_DIR}/vendor/lua" REQUIRED)

# Engine
set(ENGINE_HEADER "${CMAKE_SOURCE_DIR}/vendor/engine/bytecode.h")
set(ENGINE_BYTECODE "${CMAKE_SOURCE_DIR}/vendor/engine/bytecode.lua")
if(NOT EXISTS ${ENGINE_HEADER})
    FetchContent_Declare(
        lib_engine
        GIT_REPOSITORY https://github.com/RodrigoDornelles/codename-videogame-engine
        GIT_TAG develop
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/engine"
        SUBBUILD_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/_deps/engine-subbuild"
        BINARY_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/_deps/engine-build"
    )
    FetchContent_MakeAvailable(lib_engine)
    execute_process(
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/vendor/engine"
        COMMAND ${LUAC} src/cli/main.lua build --core native --bundler
    )
    file(COPY_FILE "${CMAKE_SOURCE_DIR}/vendor/engine/dist/main.lua" "${ENGINE_BYTECODE}")
    execute_process(
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/vendor"
        COMMAND  xxd -i engine/bytecode.lua
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(result EQUAL 0)
        file(WRITE ${ENGINE_HEADER} "${output}")
    else()
        message(FATAL_ERROR "Failed to execute xxd engine: ${error}")
    endif()
endif()

# Game
set(GAME_HEADER "${CMAKE_SOURCE_DIR}/vendor/game/bytecode.h")
set(GAME_BYTECODE "${CMAKE_SOURCE_DIR}/vendor/game/bytecode.lua")
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/vendor/game")
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/vendor/game")
    string(LENGTH "${GAME}" GAME_LENGTH)
    if(DEFINED GAME AND GAME_LENGTH)
        file(COPY_FILE "${GAME}" "${GAME_BYTECODE}")
        execute_process(
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/vendor"
            COMMAND  xxd -i game/bytecode.lua
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error
        )
        if(result EQUAL 0)
            file(WRITE ${GAME_HEADER} "${output}")
        else()
            message(FATAL_ERROR "Failed to execute xxd game: ${error}")
        endif()
    else()
        file(WRITE ${GAME_HEADER} "unsigned char* game_bytecode_lua = NULL;\nunsigned int game_bytecode_lua_len = 0;")
    endif()
endif()

# SDL
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL2_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_SHARED OFF CACHE BOOL "" FORCE)
set(SDL2_STATIC ON CACHE BOOL "" FORCE)
set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDL2TTF_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2TTF_SAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    lib_sdl2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL
    GIT_TAG release-2.30.6
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/SDL"
    SUBBUILD_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/_deps/sdl2-subbuild"
    BINARY_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/_deps/sdl2-build"
)
FetchContent_Declare(
    lib_sdl2_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf
    GIT_TAG release-2.22.0
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/SDL_ttf"
    SUBBUILD_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/_deps/sdl2-ttf-subbuild"
    BINARY_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/_deps/sdl2-ttf-build"
)
FetchContent_MakeAvailable(lib_sdl2 lib_sdl2_ttf)

# Curl
set(HTTP_ONLY ON CACHE BOOL "" FORCE)
set(PICKY_COMPILER OFF CACHE BOOL "" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_STATIC_CURL OFF CACHE BOOL "" FORCE)
set(ENABLE_ARES OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    lib_curl
    GIT_REPOSITORY https://github.com/curl/curl
    GIT_TAG curl-8_9_1
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/curl"
    SUBBUILD_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/_deps/curl-subbuild"
    BINARY_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/_deps/curl-build"
)
FetchContent_MakeAvailable(lib_curl)

FILE(GLOB engine_files "${CMAKE_SOURCE_DIR}/src/*/*.c")
add_executable(engine "${CMAKE_SOURCE_DIR}/src/main.c;${engine_files}")
target_link_libraries(engine PRIVATE lua-static)
target_link_libraries(engine PRIVATE SDL2::SDL2-static)
target_link_libraries(engine PRIVATE SDL2_ttf::SDL2_ttf-static)
target_link_libraries(engine PRIVATE CURL::libcurl)
