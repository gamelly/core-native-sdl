#include "zeebo.h"

static char *window_title = "Core Native SDL";

void sdl_set_title(char *new_title) {
    window_title = new_title;
}

char *const sdl_get_title() {
    return window_title;
}
