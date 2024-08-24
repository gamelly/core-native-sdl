#include <stdio.h>
#include <stdbool.h>

#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"
#include "SDL/include/SDL.h"

lua_State *L;
SDL_Window* window;
SDL_Renderer* renderer;

int main(int argc, char* argv[]) {
    int status = 1;
    bool running = false;
    const int wpos = SDL_WINDOWPOS_UNDEFINED;
    const char *lua_script = "print('Hello, World!')";

    do {
        L = luaL_newstate();

        if (L == NULL) {
            fprintf(stderr, "Cannot create Lua state\n");
            break;
        }

        luaL_openlibs(L);

        if (luaL_dostring(L, lua_script) != LUA_OK) {
            fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
            break;
        }

        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
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

        SDL_Event event;
        status = 0;
        running = true;
        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }
            }

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            SDL_RenderPresent(renderer);

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
    return 0;
}
