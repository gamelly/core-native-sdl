#include "zeebo.h"

extern SDL_Renderer *renderer;

static union {
    uint32_t raw;
    struct {
        uint8_t a;
        uint8_t b;
        uint8_t g;
        uint8_t r;
    } le;
    SDL_Color sdl;
} color;

/**
 * @todo move?
 */
SDL_Color sdl_get_color() {
    return color.sdl;
}

void sdl_draw_start() {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

void sdl_draw_flush() {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

/**
 * @todo correct RGB order (endiness)
 */
void sdl_draw_clear(uint32_t c, double x, double y, double w, double h) {
    color.raw = c;
    SDL_FRect rect = {x, y, w, h};
    SDL_SetRenderDrawColor(renderer, color.le.r, color.le.b, color.le.g, color.le.r);
    SDL_RenderFillRectF(renderer, &rect);
}

/**
 * @todo correct RGB order (endiness)
 */
void sdl_draw_color(uint32_t c) {
    color.raw = c;
    SDL_SetRenderDrawColor(renderer, color.le.r, color.le.g, color.le.b, color.le.a);
}

void sdl_draw_rect(uint8_t mode, double x, double y, double w, double h) {
    SDL_FRect rect = {x, y, w, h};

    if (mode) {
        SDL_RenderDrawRectF(renderer, &rect);
    } else {
        SDL_RenderFillRectF(renderer, &rect);
    }
}

void sdl_draw_line(double x1, double y1, double x2, double y2) {
    SDL_RenderDrawLineF(renderer, x1, y1, x2, y2);
}
