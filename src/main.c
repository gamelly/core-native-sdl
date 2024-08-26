#include "zeebo_engine.h"
#include "engine/bytecode.h"
#include "game/bytecode.h"

//! @cond
char *engine_file_name = NULL; 
char *game_file_name = "game.lua";

lua_State *L;
SDL_Window* window;
SDL_Renderer* renderer;
//! @endcond

bool lua_dofileOrBuffer(lua_State *L, const char* buffer, size_t buflen, const char* file_name) {
    bool succes = false;
    FILE *file_ptr = NULL;
    char *file_buf = NULL;
    size_t file_len = 0;
    
    do {
        if (file_name != NULL) {
            file_ptr = fopen(file_name, "r");
            if (file_ptr == NULL) {
                fprintf(stderr, "Cannot open file: %s\n", file_name);
                if (buffer == NULL) {
                    break;
                }
            }
        }

        if (file_ptr != NULL) {
            do {
                file_buf = realloc(file_buf, file_len + 4096);
                file_len += fread(file_buf + file_len, 1, 4096, file_ptr);
            }
            while(!feof(file_ptr));

            if (luaL_dostring(L, file_buf) != LUA_OK) {
                fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
                break;
            }
        }
        else{
            if (buffer == NULL) {
                fprintf(stderr, "Bytecode is not preloaded.\n");
                break;
            }
            if (luaL_loadbuffer(L, buffer, buflen, "") != LUA_OK || lua_pcall(L, 0, 1, 0) != LUA_OK) {
                fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
                break;
            }
            if (lua_isnil(L, 1)) {
                lua_pop(L, 1);
            }
        }
        succes = true;
    }
    while (0);
    
    if (file_ptr) {
        fclose(file_ptr);
    }
    if (file_buf) {
        free(file_buf);
    }

    return succes;
}


int main(int argc, char* argv[]) {
    int i;
    int opt;
    int status = 1;
    const int wpos = SDL_WINDOWPOS_UNDEFINED;

#ifndef NOT_USE_GETOPT
    while ((opt = getopt(argc, argv, "g:e:")) != -1) {
        switch (opt) {
            case 'g':
                game_file_name = optarg;
                break;
            case 'e':
                engine_file_name = optarg;
                break;
        }
    }
#endif

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
