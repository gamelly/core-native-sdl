#include "zeebo_engine.h"

int main(int argc, char* argv[]) 
{
    int opt;
    int status = 1;
    int lua_argc = 0;
    char **lua_argv = NULL;
    char *lua_path = NULL;
    char *lua_file_name = NULL;
    char *engine_file_name = NULL; 
    char *game_file_name = "game.lua";
    static lua_State *L = NULL;

#ifndef NOT_USE_GETOPT
    while ((opt = getopt(argc, argv, "g:e:P:L:")) != -1) {
        switch (opt) {
            case 'g':
                game_file_name = optarg;
                break;
            case 'e':
                engine_file_name = optarg;
                break;

            case 'P':
                lua_path = optarg;
                break;

            case 'L': 
                lua_file_name = optarg;
                lua_argc = argc - optind;
                lua_argv = &argv[optind];
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

        if (lua_path) {
            lua_addPath(L, lua_path);
        }
        
        if (lua_file_name != NULL) {
            lua_addArgs(L, lua_argc, lua_argv);
            status = lua_main(L, lua_file_name);
            break;
        }

        status = sdl_main_core(L, engine_file_name, game_file_name);
    }
    while(0);

    return status;
}
