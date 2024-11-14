#include "zeebo.h"

kernel_options_t kernel_option = {.width = 1280, .height = 640};

void kernel_init(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "e:f:g:Hh:m:w:")) != -1) {
        switch (opt) {
            case 'e': kernel_option.engine = optarg; break;
            case 'f': kernel_option.fullscreen = atoi(optarg); break;
            case 'g': kernel_option.game = optarg; break;
            case 'H': kernel_option.hardware = true; break;
            case 'h':
                kernel_option.height = atoi(optarg);
                kernel_option.resized = true;
                break;
            case 'm': kernel_option.media = optarg; break;
            case 'w':
                kernel_option.width = atoi(optarg);
                kernel_option.resized = true;
                break;
        }
    }

    kernel_event_callback(KERNEL_EVENT_PRE_INIT, KERNEL_EVENT_POST_INIT);
}

void kernel_update() {
    kernel_event_callback(KERNEL_EVENT_PRE_TICKET, KERNEL_EVENT_POST_TICKET);
    unsigned long dt60fps = kernel_time.ticks - kernel_time.ticks_60fps;
    if (dt60fps >= 16) {
        kernel_time.dt = dt60fps;
        kernel_time.ticks_60fps = kernel_time.ticks;
        kernel_event_callback(KERNEL_EVENT_PRE_UPDATE, KERNEL_EVENT_POST_DRAW);
    }
}

void kernel_exit() {
    kernel_event_callback(KERNEL_EVENT_PRE_EXIT, KERNEL_EVENT_POST_EXIT);
}