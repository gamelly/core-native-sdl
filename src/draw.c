#include "zeebo_engine.h"
#include "tiresias/font.h"

//! @cond
static TTF_Font* font = NULL;
static SDL_Color current_color = {255, 255, 255, 255};
//! @endcond

static int native_draw_start(lua_State *L) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
}

static int native_draw_flush(lua_State *L) {
    SDL_RenderPresent(renderer);
}

/**
 * @short @c std.draw.clear
 * @param[in] color @c int
 */
static int native_draw_clear(lua_State *L) {
    assert(lua_gettop(L) == 1);

    int color_new = luaL_checkinteger(L, 1);
    SDL_SetRenderDrawColor(renderer,
        (color_new >> 24) & 0xFF,
        (color_new >> 16) & 0xFF, 
        (color_new >> 8) & 0xFF,
        (color_new) & 0xFF
    );

    SDL_FRect rect = {0, 0, 680, 420};

    SDL_RenderFillRectF(renderer, &rect);

    lua_pop(L, 1);

    return 0;
}

/**
 * @short @c std.draw.color
 * @param[in] color @c int
 */
static int native_draw_color(lua_State *L) {
    assert(lua_gettop(L) == 1);

    int color_new = luaL_checkinteger(L, 1);
    current_color.r = (color_new >> 24) & 0xFF;
    current_color.g = (color_new >> 16) & 0xFF;
    current_color.b = (color_new >> 8) & 0xFF;
    current_color.a = (color_new) & 0xFF;

    SDL_SetRenderDrawColor(renderer, current_color.r, current_color.g, current_color.b, current_color.a);
    
    lua_pop(L, 1);

    return 0;
}

/**
 * @short @c std.draw.rect
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
 * @short @c std.draw.line
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
    
    SDL_RenderDrawLineF(renderer, x1, y1, x2, y2);

    lua_pop(L, 4);

    return 0;
}

/**
 * @short @c std.draw.font
 * @param[in] font @c string
 * @param[in] size @c double
 */
static int native_draw_font(lua_State *L) {
    int argc = lua_gettop(L);
    const char* font_name = NULL;
    int font_size = 0;
    static SDL_RWops* rw = NULL;
    static int current_font_size = 0;
    static const char *current_font_name = NULL;
    static const char *failed_font_name = NULL;

    if (argc == 1) {
        font_size = luaL_checkinteger(L, 1);
        lua_pop(L, 1);
    } else if (argc == 2) {
        font_name = luaL_checkstring(L, 1);
        font_size = luaL_checkinteger(L, 2);
        lua_pop(L, 2);
    } else {
        luaL_error(L, "std.draw.font");
    }

    if (font && current_font_name != NULL && current_font_name != font_name) {
        TTF_CloseFont(font);
    }

    do {
        if (font_size == 0) {
            break;
        }
        
        if (font_name == current_font_name && font_size == current_font_size) {
            break;
        }

        current_font_size = font_size;

        if (font_name != NULL) {
            TTF_Font* font_new = TTF_OpenFont(font_name, font_size);
        
            if (font_new) {
                font = font_new;
                current_font_name = font_name;
                break;
            } else if (failed_font_name != font_name) {
                fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
                failed_font_name = font_name;
            }
        }

        if (font != NULL && current_font_name == NULL) {
            break;
        }

        if (rw == NULL) {
            rw = SDL_RWFromConstMem(Tiresias_ttf, Tiresias_ttf_len);
        }

        if (rw == NULL) {
            fprintf(stderr, "Failed to create SDL_RWops from memory: %s\n", SDL_GetError());
            break;
        }

        font = TTF_OpenFontRW(rw, 1, font_size);

        if (font == NULL) {
            fprintf(stderr, "Failed to load font from memory: %s\n", TTF_GetError());
            break;
        }
    }
    while(0);

    return 0;
}

/**
 * @short @c std.draw.text
 * @param[in] x @c double
 * @param[in] y @c double
 * @param[in] text @c string
 */
static int native_draw_text(lua_State *L) {
    int x, y;
    int argc = lua_gettop(L);
    const char* text = NULL;
    if (argc == 1) {
        text = luaL_checkstring(L, 1);
        lua_pop(L, 1);
    } else if(argc == 3) {
        x = (int) luaL_checknumber(L, 1);
        y = (int) luaL_checknumber(L, 2);
        text = luaL_checkstring(L, 3);
        lua_pop(L, 3);
    } 
   
    int result = 0;
    int textWidth = 0;
    int textHeight = 0;
    SDL_Surface* textSurface = NULL;
    SDL_Texture* textTexture = NULL;

    do {
        if (text == NULL) {
            break;
        }
        if (font == NULL) {
            fprintf(stderr, "Failed to select Font\n");
            break;
        }

        textSurface = TTF_RenderText_Solid(font, text, current_color);
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
        if (argc == 3) {
            SDL_Rect destRect = { x, y, textWidth, textHeight };
            SDL_RenderCopy(renderer, textTexture, NULL, &destRect);
        }

        result = 2;
    }
    while(0);

    if (textSurface) {
        SDL_FreeSurface(textSurface);
    }

    if (textTexture) {
        SDL_DestroyTexture(textTexture);
    }
    
    if (result == 2) {
        lua_pushinteger(L, textWidth);
        lua_pushinteger(L, textHeight);
    } else {
        luaL_error(L, "std.draw.text");
    }
    
    return result;
}

//! @cond
static const luaL_Reg zeebo_drawlib[] = {
    {"native_draw_start", native_draw_start},
    {"native_draw_flush", native_draw_flush},
    {"native_draw_clear", native_draw_clear},
    {"native_draw_color", native_draw_color},
    {"native_draw_rect", native_draw_rect},
    {"native_draw_line", native_draw_line},
    {"native_draw_font", native_draw_font},
    {"native_draw_text", native_draw_text}
};

const luaL_Reg *const zeebo_drawlib_list = zeebo_drawlib;
const int zeebo_drawlib_size = sizeof(zeebo_drawlib)/sizeof(luaL_Reg);
//! @endcond
