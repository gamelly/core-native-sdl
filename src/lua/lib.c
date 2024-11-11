#include "zeebo.h"

void lua_addPath(lua_State *L, const char* path)
{
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    const char *old_path = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_pushfstring(L, "%s;%s/?.lua", old_path, path);
    lua_setfield(L, -2, "path");
    lua_pop(L, 1); 
}

void lua_addArgs(lua_State *L, int argc, char* argv[])
{
    int i = 0;
    lua_newtable(L);
    while(i < argc) {
        lua_pushstring(L, argv[i]);
        i = i + 1;
        lua_seti(L, -2, i);
    }
    lua_setglobal(L, "arg");
}

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
            file_buf[file_len] = '\0';

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
