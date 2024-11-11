#include "zeebo.h"

static void (*core_event_callback[KERNEL_EVENT_COUNT][255])(void);
static uint8_t core_event_size[KERNEL_EVENT_COUNT];

void kernel_event_callback(kernel_event_t start, kernel_event_t end) {
    while (start <= end) {
        uint8_t i = 0;
        uint8_t j = core_event_size[start];
        while (i < j) { core_event_callback[start][i++](); }
        start++;
    }
}

void kernel_event_install(kernel_event_t event_id, void *event_func) {
    uint8_t event_index = core_event_size[event_id]++;
    core_event_callback[event_id][event_index] = event_func;
}
