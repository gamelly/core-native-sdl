#include "zeebo_engine.h"

static const char *get_key(SDL_Keycode key) {
    switch(key) {
        case SDLK_UP: return "up";
        case SDLK_DOWN: return "down";
        case SDLK_LEFT: return "left";
        case SDLK_RIGHT: return "right";
        case SDLK_z: return "red";
        case SDLK_x: return "green";
        case SDLK_c: return "yellow";
        case SDLK_v: return "blue";
        case SDLK_RETURN: return "enter";
    }
   return "_";
}

void native_keyboard_keydown(lua_State *L, SDL_Keycode key) {
    lua_getglobal(L, "native_callback_keyboard");
    lua_pushstring(L, get_key(key));
    lua_pushinteger(L, 1);
    lua_pcall(L, 2, 0, 0);
}

void native_keyboard_keyup(lua_State *L, SDL_Keycode key) {
    lua_getglobal(L, "native_callback_keyboard");
    lua_pushstring(L, get_key(key));
    lua_pushinteger(L, 0);
    lua_pcall(L, 2, 0, 0);
}
