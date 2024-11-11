#include "sdl.h"
#include "zeebo.h"

extern SDL_Renderer *renderer;

static union {
    uint32_t raw;
    SDL_Color sdl;
} color;

void sdl_draw_start() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
}

void sdl_draw_flush() {
    SDL_RenderPresent(renderer);
}

void sdl_draw_clear(uint32_t c, double x, double y, double w, double h) {
    color.raw = c;
    SDL_FRect rect = {x, y, w, h};
    SDL_SetRenderDrawColor(renderer, color.sdl.r, color.sdl.g, color.sdl.b, color.sdl.a);
    SDL_RenderFillRectF(renderer, &rect);
}

void sdl_draw_color(uint32_t c) {
    color.raw = c;
    SDL_SetRenderDrawColor(renderer, color.sdl.r, color.sdl.g, color.sdl.b, color.sdl.a);
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
