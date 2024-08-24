#include "zeebo_engine.h"

/**
 * @param[in] mode @c int 0 fill, 1 frame
 * @param[in] x @c double pos X
 * @param[in] y @c double pos Y
 * @param[in] w @c double width
 * @param[in] h @c double height
 */
static int native_draw_rect(lua_State *L) {
    assert(lua_gettop(L) == 5);

    SDL_FRect rect = {
        luaL_checknumber(L, 2),
        luaL_checknumber(L, 3),
        luaL_checknumber(L, 4),
        luaL_checknumber(L, 5)
    };

    if (luaL_checkinteger(L, 1)) {
        SDL_RenderDrawRectF(renderer, &rect);
    }
    else {
        SDL_RenderFillRectF(renderer, &rect);
    }

    lua_pop(L, 5);

    return 0;
}

static const luaL_Reg zeebo_drawlib[] = {
    {"native_draw_rect", native_draw_rect}
};

const luaL_Reg *const zeebo_drawlib_list = zeebo_drawlib;
const int zeebo_drawlib_size = sizeof(zeebo_drawlib)/sizeof(luaL_Reg);
