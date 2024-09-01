#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

//! @cond
#ifndef NOT_USE_GETOPT
#include <unistd.h>
#endif
//! @endcond

#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"
#include "SDL/include/SDL.h"
#include "SDL_ttf/SDL_ttf.h"

//! @cond
extern int app_width;
extern int app_height;
extern bool app_fullscreen;

extern SDL_Window* window;
extern SDL_Renderer* renderer;

//! @file src/curl/native_http.c
void native_http_install(lua_State* L);
void native_http_cleanup(lua_State* L);

//! @file src/lua/main.c
int lua_main(lua_State *L, char *file_name);

//! @file src/lua/lib.c
void lua_addPath(lua_State *L, const char* path);
void lua_addArgs(lua_State *L, int argc, char* argv[]);
bool lua_dofileOrBuffer(lua_State *L, const char* buffer, size_t buflen, const char* file_name);

//! @file src/sdl/main.c
int sdl_main_core(lua_State *L, char* engine_file_name, char* game_file_name);

//! @file src/sdl/native_draw.c
void native_draw_install(lua_State *L);

//! @file src/sdl/native_event.c
bool native_draw_pool(lua_State *L);

//! @endcond
