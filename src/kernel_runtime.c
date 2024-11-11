#include "zeebo.h"

static bool want_quit;

bool kernel_runtime_online() {
    if (want_quit) {
        return false;
    }

    return true;
}

void kernel_runtime_quit() {
    want_quit = true;
}

int kernel_runtime_get_status() {
    if (kernel_has_error()) {
        return 1;
    }

    return 0;
}
