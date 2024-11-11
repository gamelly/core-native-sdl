#include "zeebo.h"

static uint8_t core_dt;

void kernel_init(int argc, char *argv[]) {
    kernel_event_callback(KERNEL_EVENT_PRE_INIT, KERNEL_EVENT_POST_INIT);
}

void kernel_update() {
    kernel_event_callback(KERNEL_EVENT_PRE_UPDATE, KERNEL_EVENT_POST_UPDATE);
}

void kernel_exit() {
    kernel_event_callback(KERNEL_EVENT_PRE_EXIT, KERNEL_EVENT_POST_EXIT);
}

void kernel_set_dt(uint8_t milis) {
    core_dt = milis;
}
