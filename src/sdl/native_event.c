#include "zeebo.h"

static const char *get_key(SDL_Keycode key) {
    switch(key) {
        case SDLK_UP: return "up";
        case SDLK_DOWN: return "down";
        case SDLK_LEFT: return "left";
        case SDLK_RIGHT: return "right";
        case SDLK_z: return "a";
        case SDLK_x: return "b";
        case SDLK_c: return "c";
        case SDLK_v: return "d";
        case SDLK_RETURN: return "a";
    }
   return "_";
}

static void native_event_keydown(lua_State *L, SDL_Keycode key) {
    lua_getglobal(L, "native_callback_keyboard");
    lua_pushstring(L, get_key(key));
    lua_pushinteger(L, 1);
    lua_pcall(L, 2, 0, 0);
}

static void native_event_keyup(lua_State *L, SDL_Keycode key) {
    lua_getglobal(L, "native_callback_keyboard");
    lua_pushstring(L, get_key(key));
    lua_pushinteger(L, 0);
    lua_pcall(L, 2, 0, 0);
}

static void native_event_resize(lua_State *L) {
    lua_getglobal(L, "native_callback_resize");
    lua_pushnumber(L, app_width);
    lua_pushnumber(L, app_height);
    lua_pcall(L, 2, 0, 0);
}

static void native_event_fullscreen_toggle() {
    app_fullscreen = !app_fullscreen;
    if (app_fullscreen) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    else {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_RESIZABLE);
    }
}

bool native_event_pool(lua_State *L) {
    bool running = true;
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
        }
        else if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
            app_width = event.window.data1;
            app_height = event.window.data2;
            native_event_resize(L);
        }
        else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN && SDL_GetModState() & KMOD_ALT) {
            native_event_fullscreen_toggle();
        }
        else if (event.type == SDL_KEYDOWN) {
            native_event_keydown(L, event.key.keysym.sym);
        }
        else if (event.type == SDL_KEYUP) {
            native_event_keyup(L, event.key.keysym.sym);
        }
    }

    return running;
}
