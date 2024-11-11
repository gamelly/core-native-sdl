#include "zeebo.h"

int lua_main(lua_State *L, char *file_name)
{
    int status = 1;

    do {
        if (!lua_dofileOrBuffer(L, NULL, 0, file_name)) {
            break;
        }

        int top = lua_gettop(L);

        if (top == 0) {
            status = 0;
            break;
        }

        if (lua_isnumber(L, 1)) {
            status = luaL_checkinteger(L, 1);
        }

        lua_pop(L, top);
    }
    while(0);

    return status;
}
