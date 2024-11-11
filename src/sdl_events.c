#include "sdl.h"
#include "zeebo.h"

SDL_Window *window;
SDL_Renderer *renderer;

static void sdl_init() {
    if (kernel_has_error()) {
        return;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        kernel_add_error("Unable to initialize SDL:");
        kernel_add_error(SDL_GetError());
        return;
    }

    if (TTF_Init() < 0) {
        kernel_add_error("Unable to initialize SDL TTL");
        return;
    }

    window = SDL_CreateWindow(sdl_get_title(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_SHOWN);

    if (!window) {
        kernel_add_error("Unable to create window:");
        kernel_add_error(SDL_GetError());
        return;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        kernel_add_error("Unable to create renderer:");
        kernel_add_error(SDL_GetError());
        return;
    }
}

static void sdl_exit() {
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }

    if (window) {
        SDL_DestroyWindow(window);
    }

    SDL_Quit();
}

static void sdl_tickets() {
    static unsigned long old_ticks = 0;
    unsigned long new_ticks = SDL_GetTicks();
    kernel_set_dt(new_ticks - old_ticks);
    old_ticks = new_ticks;
}

static void sdl_event_pool() {
    static SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            kernel_runtime_quit();
        }
    }
}

static void sdl_delay() {
    SDL_Delay(16);
}

void sdl_install() {
    kernel_event_install(KERNEL_EVENT_POST_INIT, sdl_init);
    kernel_event_install(KERNEL_EVENT_PRE_UPDATE, sdl_tickets);
    kernel_event_install(KERNEL_EVENT_PRE_UPDATE, sdl_event_pool);
    kernel_event_install(KERNEL_EVENT_POST_UPDATE, sdl_delay);
    kernel_event_install(KERNEL_EVENT_EXIT, sdl_exit);
}
