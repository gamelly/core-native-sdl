#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

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
extern SDL_Window* window;
extern SDL_Renderer* renderer;

extern const luaL_Reg *const zeebo_drawlib_list;
extern const int zeebo_drawlib_size;

void native_keyboard_keydown(lua_State *L, SDL_Keycode key);
void native_keyboard_keyup(lua_State *L, SDL_Keycode key);
//! @endcond
