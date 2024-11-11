#include "engine/bytecode.h"
#include "game/bytecode.h"
#include "lua.h"
#include "zeebo.h"

static void engine_init() {
    lua_State *L = lua();

    luaL_loadbuffer(L, engine_bytecode_lua, engine_bytecode_lua_len, "engine");
    lua_pcall(L, 0, 0, 0);

    lua_getglobal(L, "native_callback_init");
    lua_pushnumber(L, 1280);
    lua_pushnumber(L, 720);

    luaL_loadbuffer(L, game_bytecode_lua, game_bytecode_lua_len, "game");
    lua_pcall(L, 0, 1, 0);
    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
        kernel_add_error(lua_tostring(L, -1));
    }
}

static void engine_exit() {}

static void engine_loop() {
    lua_State *L = lua();
    lua_getglobal(L, "native_callback_loop");
    lua_pushnumber(L, 16);
    lua_pcall(L, 1, 0, 0);
}

static void engine_draw() {
    lua_State *L = lua();
    lua_getglobal(L, "native_callback_draw");
    lua_pcall(L, 0, 0, 0);
}

void engine_keypress(const char *const key, uint8_t value)
{
    lua_State *L = lua();
    lua_getglobal(L, "native_callback_keyboard");
    lua_pushstring(L, key);
    lua_pushinteger(L, value);
    lua_pcall(L, 2, 0, 0);
}

void engine_install() {
    kernel_event_install(KERNEL_EVENT_INIT, engine_init);
    kernel_event_install(KERNEL_EVENT_UPDATE, engine_loop);
    kernel_event_install(KERNEL_EVENT_UPDATE, engine_draw);
    kernel_event_install(KERNEL_EVENT_EXIT, engine_exit);
}
