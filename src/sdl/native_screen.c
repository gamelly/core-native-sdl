#include "zeebo_engine.h"

void native_screen_resize(lua_State *L) {
    lua_getglobal(L, "native_callback_resize");
    lua_pushnumber(L, app_width);
    lua_pushnumber(L, app_height);
    lua_pcall(L, 2, 0, 0);
}

void native_screen_fullscreen_toggle() {
    app_fullscreen = !app_fullscreen;
    if (app_fullscreen) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    else {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_RESIZABLE);
    }
}
