#include "zeebo_engine.h"

static TTF_Font* font = NULL;
static SDL_Color color = {255, 255, 255, 255};

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

/**
 * @param[in] x1 @c double
 * @param[in] y1 @c double
 * @param[in] x2 @c double
 * @param[in] y2 @c double
 */
static int native_draw_line(lua_State *L) {
    assert(lua_gettop(L) == 4);

    float x1 = luaL_checknumber(L, 1);
    float y1 = luaL_checknumber(L, 2);
    float x2 = luaL_checknumber(L, 3);
    float y2 = luaL_checknumber(L, 4);
    
    SDL_RenderDrawLineF(renderer, x1, y1, y2, y2);

    lua_pop(L, 4);

    return 0;
}

/**
 * @param[in] font @c string
 * @param[in] size @c double
 */
static int native_draw_font(lua_State *L) {
    assert(lua_gettop(L) == 2);

    const char* font_name = luaL_checkstring(L, 1);
    int size = luaL_checkinteger(L, 2);

    if (font) {
        TTF_CloseFont(font);
    }

    font = TTF_OpenFont("/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf", size);
    
    if (font == NULL) {
        fprintf(stderr, "Failed to load default font: %s\n", TTF_GetError());
    }

    lua_pop(L, 2);

    return 0;
}

/**
 * @param[in] x @c double
 * @param[in] y @c double
 * @param[in] text @c string
 */
static int native_draw_text(lua_State *L) {
    assert(lua_gettop(L) == 3);

    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    const char* text = luaL_checkstring(L, 3);

    int textWidth = 0;
    int textHeight = 0;
    SDL_Surface* textSurface = NULL;
    SDL_Texture* textTexture = NULL;

    do {
        running = false;

        if (font == NULL) {
            fprintf(stderr, "Failed to select Font\n");
            break;
        }

        textSurface = TTF_RenderText_Solid(font, text, color);
        if (textSurface == NULL) {
            fprintf(stderr, "Failed to create text surface: %s\n", TTF_GetError());
            break;
        }

        textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
        if (textTexture == NULL) {
            fprintf(stderr, "Failed to create text texture: %s\n", SDL_GetError());
            break;
        }

        SDL_QueryTexture(textTexture, NULL, NULL, &textWidth, &textHeight);
        SDL_Rect destRect = { x, y, textWidth, textHeight };
        SDL_RenderCopy(renderer, textTexture, NULL, &destRect);

        running = true;
    }
    while(0);

    if (textSurface) {
        SDL_FreeSurface(textSurface);
    }

    if (textTexture) {
        SDL_DestroyTexture(textTexture);
    }

    lua_pop(L, 3);
    lua_pushinteger(L, textWidth);
    lua_pushinteger(L, textHeight);
    
    return 0;
}

static const luaL_Reg zeebo_drawlib[] = {
    {"native_draw_rect", native_draw_rect},
    {"native_draw_line", native_draw_line},
    {"native_draw_font", native_draw_font},
    {"native_draw_text", native_draw_text}
};

const luaL_Reg *const zeebo_drawlib_list = zeebo_drawlib;
const int zeebo_drawlib_size = sizeof(zeebo_drawlib)/sizeof(luaL_Reg);
