#include "zeebo.h"

static int native_draw_start(lua_State *L) {
    sdl_draw_start();
    return 0;
}

static int native_draw_flush(lua_State *L) {
    sdl_draw_flush();
    return 0;
}

static int native_draw_clear(lua_State *L) {
    uint32_t color = luaL_checknumber(L, 1);
    double x = luaL_checknumber(L, 2);
    double y = luaL_checknumber(L, 3);
    double w = luaL_checknumber(L, 4);
    double h = luaL_checknumber(L, 5);
    sdl_draw_clear(color, x, y, w, h);
    lua_settop(L, 0);
    return 0;
}

static int native_draw_color(lua_State *L) {
    uint32_t color = luaL_checknumber(L, 1);
    sdl_draw_color(color);
    lua_settop(L, 0);
    return 0;
}

static int native_draw_rect(lua_State *L) {
    uint8_t mode = luaL_checknumber(L, 1);
    double x = luaL_checknumber(L, 2);
    double y = luaL_checknumber(L, 3);
    double w = luaL_checknumber(L, 4);
    double h = luaL_checknumber(L, 5);
    sdl_draw_rect(mode, x, y, w, h);
    lua_settop(L, 0);
    return 0;
}

static int native_draw_line(lua_State *L) {
    double x1 = luaL_checknumber(L, 1);
    double y1 = luaL_checknumber(L, 2);
    double x2 = luaL_checknumber(L, 3);
    double y2 = luaL_checknumber(L, 4);
    sdl_draw_line(x1, y1, x2, y2);
    lua_settop(L, 0);
    return 0;
}

static int native_draw_font(lua_State *L) {
    if (lua_gettop(L) == 1) {
        sdl_text_font_size(luaL_checkinteger(L, 1));
    } else {
        sdl_text_font_size(luaL_checkinteger(L, 2));
    }
    lua_settop(L, 0);
    return 0;
}

static int native_draw_text(lua_State *L) {
    int width, height;
    int argc = lua_gettop(L);
    if (argc == 1) {
        const char *text = luaL_checkstring(L, 1);
        sdl_text_print(1, 0, 0, text, &width, &height);
        lua_pop(L, 1);
    } else if (argc == 3) {
        int x = luaL_checknumber(L, 1);
        int y = luaL_checknumber(L, 2);
        const char *text = luaL_checkstring(L, 3);
        sdl_text_print(0, x, y, text, &width, &height);
        lua_pop(L, 3);
    } else if (argc == 8) {
        int x = luaL_checknumber(L, 1);
        int y = luaL_checknumber(L, 2);
        int ox = luaL_checknumber(L, 3);
        int oy = luaL_checknumber(L, 4);
        int w = luaL_checknumber(L, 5);
        int h = luaL_checknumber(L, 6);
        int s = luaL_checknumber(L, 7);
        const char *text = luaL_checkstring(L, 8);
        int hem = w / 80;
        x = ox + ((hem)*x);
        y = oy + ((h / 24) * y);
        sdl_text_font_size(s * hem);
        sdl_text_print(0, x, y, text, &width, &height);
        lua_pop(L, 8);
    }
    lua_pushnumber(L, width);
    lua_pushnumber(L, height);
    return 2;
}

static int native_draw_image(lua_State *L) {
    lua_settop(L, 0);
    return 0;
}

static void native_draw_load() {
    int i = 0;
    lua_State *const L = lua();

    static const luaL_Reg lib[] = {{"native_draw_start", native_draw_start},   {"native_draw_flush", native_draw_flush},
                                   {"native_draw_clear", native_draw_clear},   {"native_draw_color", native_draw_color},
                                   {"native_draw_rect", native_draw_rect},     {"native_draw_line", native_draw_line},
                                   {"native_draw_font", native_draw_font},     {"native_draw_text", native_draw_text},
                                   {"native_draw_text_tui", native_draw_text}, {"native_draw_image", native_draw_image}};

    while (i < sizeof(lib) / sizeof(luaL_Reg)) {
        lua_pushcfunction(L, lib[i].func);
        lua_setglobal(L, lib[i].name);
        i = i + 1;
    }

    lua_newtable(L);
    lua_pushstring(L, "repeats");
    lua_newtable(L);
    lua_pushboolean(L, 1);
    lua_seti(L, -2, 1);
    lua_pushboolean(L, 1);
    lua_seti(L, -2, 2);
    lua_settable(L, -3);
    lua_pushstring(L, "line");
    lua_pushcfunction(L, native_draw_line);
    lua_settable(L, -3);
    lua_setglobal(L, "native_dict_poly");
}

void native_draw_install() {
    kernel_event_install(KERNEL_EVENT_PRE_INIT, native_draw_load);
}

#ifdef DOXYGEN
/**
 * @short @c std.draw.poly
 * @par Lua definition
 * @code
 * local native_dict_poly = {
 *  repeats = {true, true},
 *  line = function (x1, y1, x2, y2)
 *    native_draw_line(x1, y1, x2, y2)
 *  end
 * }
 * @endcode
 */
class native_dict_poly {
   public:
    bool repeats[2] = {true, true};
    /**
     * @param[in] x1 @c double
     * @param[in] y1 @c double
     * @param[in] x2 @c double
     * @param[in] y2 @c double
     */
    int line(lua_State *L);
};
#endif
