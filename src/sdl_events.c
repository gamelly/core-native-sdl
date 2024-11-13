#include "zeebo.h"

SDL_Window *window;
SDL_Renderer *renderer;
kernel_time_t kernel_time;

static const char *sdl_keybinding(SDL_Keycode key) {
    switch (key) {
        case SDLK_UP: return "up";
        case SDLK_DOWN: return "down";
        case SDLK_LEFT: return "left";
        case SDLK_RIGHT: return "right";
        case SDLK_z: return "a";
        case SDLK_x: return "b";
        case SDLK_c: return "c";
        case SDLK_v: return "d";
        case SDLK_RETURN: return "a";
    }
    return "_";
}

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

    const int windowpos = SDL_WINDOWPOS_UNDEFINED;
    uint32_t windowmode = kernel_option.hardware? SDL_WINDOW_OPENGL: SDL_WINDOW_SHOWN;
    window = SDL_CreateWindow(sdl_get_title(), windowpos, windowpos, 1280, 720, windowmode);

    if (!window) {
        kernel_add_error("Unable to create window:");
        kernel_add_error(SDL_GetError());
        return;
    }

    uint32_t rendermode = kernel_option.hardware? SDL_RENDERER_ACCELERATED: 0;
    renderer = SDL_CreateRenderer(window, -1, rendermode);
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
    kernel_time.ticks = SDL_GetTicks();
}

static void sdl_event_pool() {
    static SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            kernel_runtime_quit();
        } else if (event.type == SDL_KEYDOWN) {
            engine_keypress(sdl_keybinding(event.key.keysym.sym), 1);
        } else if (event.type == SDL_KEYUP) {
            engine_keypress(sdl_keybinding(event.key.keysym.sym), 0);
        }
    }
}

static void sdl_delay() {
    SDL_Delay(1);
}

static void sdl_pre_draw() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
}

void sdl_post_draw() {
    SDL_RenderPresent(renderer);
}

void sdl_install() {
    kernel_event_install(KERNEL_EVENT_POST_INIT, sdl_init);
    kernel_event_install(KERNEL_EVENT_PRE_TICKET, sdl_tickets);
    kernel_event_install(KERNEL_EVENT_TICKET, sdl_event_pool);
    kernel_event_install(KERNEL_EVENT_PRE_DRAW, sdl_pre_draw);
    kernel_event_install(KERNEL_EVENT_POST_DRAW, sdl_post_draw);
    kernel_event_install(KERNEL_EVENT_POST_TICKET, sdl_delay);
    kernel_event_install(KERNEL_EVENT_EXIT, sdl_exit);
}
