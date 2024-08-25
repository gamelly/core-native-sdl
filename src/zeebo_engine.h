#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>

#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"
#include "SDL/include/SDL.h"
#include "SDL_ttf/SDL_ttf.h"

extern SDL_Window* window;
extern SDL_Renderer* renderer;

extern const luaL_Reg *const zeebo_drawlib_list;
extern const int zeebo_drawlib_size;

