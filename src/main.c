#include "zeebo_engine.h"
#include "engine/native.h"

bool running = false;

lua_State *L;
SDL_Window* window;
SDL_Renderer* renderer;

int main(int argc, char* argv[]) {
    int i;
    int status = 1;
    bool should_close = false;
    const int wpos = SDL_WINDOWPOS_UNDEFINED;
    const char *lua_script = "print('Hello, World!')\n"
      "function native_callback_draw()\n"
      "native_draw_rect(0, 100, 200, 250, 250)\n"
      "native_draw_text(8, 8, 'helo!')\nend";

    do {
        L = luaL_newstate();

        if (L == NULL) {
            fprintf(stderr, "Cannot create Lua state\n");
            break;
        }

        luaL_openlibs(L);
        
        for(i = 0; i < zeebo_drawlib_size; i++) {
            lua_pushcfunction(L, zeebo_drawlib_list[i].func);
            lua_setglobal(L, zeebo_drawlib_list[i].name);
        }

        if (luaL_dostring(L, lua_script) != LUA_OK) {
            fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
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

        SDL_Event event;
        status = 0;
        running = true;
        while (running && !should_close) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    should_close = true;
                }
            }

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

            lua_getglobal(L, "native_callback_draw");
            lua_pcall(L, 0, 0, 0);

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
