#include "zeebo.h"

int app_width = 648;
int app_height = 480;
bool app_fullscreen = false;

/**
 * @mainpage
 *
 * @section build How to build
 *
 * You should only need @b cmake and @b make, the project has the recipe
 * for downloading all dependencies including cross compilers.
 * 
 * @par only engine
 * @code
 * git clone
 * cmake -Bbuild -H.
 * make -C build
 * @endcode
 *
 * @par engine + game
 * @code
 * git clone
 * cmake -Bbuild -H. -DGAME="/path/to/your/game.lua"
 * make -C build
 * @endcode
 *
 * @par custom engine + game
 * @code
 * git clone
 * cmake -Bbuild -H. -DGAME="/path/to/your/game.lua" -DENGINE="/path/to/your/engine.lua"
 * make -C build
 * @endcode
 *
 * @section run How to run
 *
 * @par simple way
 *
 * put a @c engine.exe in some path to your @c game.lua and double click the executable!
 *
 * @par passing file location flag
 * @code
 * engine.exe -g /path/to/your/game.lua
 * @endcode
 *
 * @par another flags
 * @code
 * engine.exe -f -w 1980 -h 1280 -e /path/to/your/engine.lua -g /path/to/your/game.lua
 * @endcode
 */
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
    while ((opt = getopt(argc, argv, "e:fg:h:w:P:L:")) != -1) {
        switch (opt) {
            case 'e':
                engine_file_name = optarg;
                break;
            case 'f':
                app_fullscreen = true;
                break;
            case 'g':
                game_file_name = optarg;
                break;
            case 'h':
                app_height = atoi(optarg);
                break;
            case 'w':
                app_width = atoi(optarg);
                break;
            case 'P':
                lua_path = optarg;
                break;
            case 'L': 
                lua_file_name = optarg;
                lua_argc = argc - optind;
                lua_argv = &argv[optind];
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
