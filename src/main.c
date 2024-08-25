#include "zeebo_engine.h"
#include "engine/native.h"

char *engine_file_name = NULL; 
char *game_file_name = "game.lua";
FILE *file_ptr = NULL;
char *file_buf = NULL;
size_t file_len = 0;
bool running = false;

lua_State *L;
SDL_Window* window;
SDL_Renderer* renderer;

int main(int argc, char* argv[]) {
    int i;
    int opt;
    int status = 1;
    bool should_close = false;
    const int wpos = SDL_WINDOWPOS_UNDEFINED;

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

        if (engine_file_name == NULL) {
            if (luaL_loadbuffer(L, main_lua, main_lua_len, "") != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK) {
                fprintf(stderr, "Lua Engine error: %s\n", lua_tostring(L, -1));
                break;
            }

        } else {
            file_ptr = fopen(engine_file_name, "r");

            if (file_ptr == NULL) {
                fprintf(stderr, "Cannot open engine file: %s\n", engine_file_name);
                break;
            }

            do {
                file_buf = realloc(file_buf, file_len + 4096);
                file_len += fread(file_buf + file_len, 1, 4096, file_ptr);
            }
            while(!feof(file_ptr));

            if (luaL_dostring(L, file_buf) != LUA_OK) {
                fprintf(stderr, "Lua Game syntax error: %s\n", lua_tostring(L, -1));
                break;
            }

            fclose(file_ptr);
            free(file_buf);
            file_buf = NULL;
            file_len = 0;
        }

        file_ptr = fopen(game_file_name, "r");

        if (file_ptr == NULL) {
            fprintf(stderr, "Cannot open game file: %s\n", game_file_name);
            break;
        }

        do {
            file_buf = realloc(file_buf, file_len + 4096);
            file_len += fread(file_buf + file_len, 1, 4096, file_ptr);
        }
        while(!feof(file_ptr));

        if (luaL_dostring(L, file_buf) != LUA_OK) {
            fprintf(stderr, "Lua Game syntax error: %s\n", lua_tostring(L, -1));
            break;
        }
        lua_pop(L, 1);

        lua_getglobal(L, "native_callback_init");
        lua_pushnumber(L, 640);
        lua_pushnumber(L, 480);
        lua_pushstring(L, file_buf);

        fclose(file_ptr);
        free(file_buf);

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

        SDL_Event event;
        status = 0;
        running = true;
        while (running && !should_close) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    should_close = true;
                }
            }

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
