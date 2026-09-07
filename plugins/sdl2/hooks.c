#include "SDL_internal.h"
#include "video/SDL_sysvideo.h"

int gecnd_sdl2_swap(SDL_Window *window);
void gecnd_sdl2_pump(void);

int OFFSCREEN_GLES_SwapWindow(_THIS, SDL_Window *window) {
    (void)_this;
    return gecnd_sdl2_swap(window);
}

void OFFSCREEN_PumpEvents(_THIS) {
    (void)_this;
    gecnd_sdl2_pump();
}
