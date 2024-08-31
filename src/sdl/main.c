#include "zeebo_engine.h"
#include "engine/bytecode.h"
#include "game/bytecode.h"

//! @cond
SDL_Window* window;
SDL_Renderer* renderer;
//! @endcond

int sdl_main_core(lua_State *L, char* engine_file_name, char* game_file_name) {
    int i = 0;
    int status = 1;
    const int wpos = SDL_WINDOWPOS_UNDEFINED;

    do {
        native_draw_install(L);
        native_http_install(L);

        if (!lua_dofileOrBuffer(L, engine_bytecode_lua, engine_bytecode_lua_len, engine_file_name)) {
            fprintf(stderr, "Engine start failed!\n");
            break;
        }

        lua_getglobal(L, "native_callback_init");
        lua_pushnumber(L, 640);
        lua_pushnumber(L, 480);
        if (!lua_dofileOrBuffer(L, game_bytecode_lua, game_bytecode_lua_len, game_file_name)) {
            fprintf(stderr, "Game start failed!\n");
            break;
        }

        if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
            fprintf(stderr, "Lua Game loading error: %s\n", lua_tostring(L, -1));
            break;
        }

        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
            break;
        }

        if (TTF_Init() < 0) {
            fprintf(stderr, "Unable to initialize SDL TTL\n");
            break;
        }

        if (luaL_dostring(L, "native_draw_font(18)") != LUA_OK) {
            fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
            break;
        }

        window = SDL_CreateWindow("Hello World SDL", wpos, wpos, 640, 480, SDL_WINDOW_SHOWN);

        if (!window) {
            fprintf(stderr, "Unable to create window: %s\n", SDL_GetError());
            break;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            fprintf(stderr, "Unable to create renderer: %s\n", SDL_GetError());
            break;
        }

        status = 0;
        SDL_Event event;
        bool running = true;
        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }
                else if (event.type == SDL_KEYDOWN) {
                    native_keyboard_keydown(L, event.key.keysym.sym);
                }
                else if (event.type == SDL_KEYUP) {
                    native_keyboard_keyup(L, event.key.keysym.sym);
                }
            }
            status = 1;

            lua_getglobal(L, "native_callback_loop");
            lua_pushnumber(L, SDL_GetTicks());
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                fprintf(stderr, "Lua Game loop error: %s\n", lua_tostring(L, -1));
                break;
            }

            lua_getglobal(L, "native_callback_draw");
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                fprintf(stderr, "Lua Game draw error: %s\n", lua_tostring(L, -1));
                break;
            }

            status = 0;

            SDL_Delay(16);
        }
    }
    while(0);

    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    if (L) {
        lua_close(L);
    }

    SDL_Quit();
    return status;
}
