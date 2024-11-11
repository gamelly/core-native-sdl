#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>

typedef enum {
    KERNEL_EVENT_PRE_INIT,
    KERNEL_EVENT_INIT,
    KERNEL_EVENT_POST_INIT,
    KERNEL_EVENT_PRE_UPDATE,
    KERNEL_EVENT_UPDATE,
    KERNEL_EVENT_POST_UPDATE,
    KERNEL_EVENT_PRE_EXIT,
    KERNEL_EVENT_EXIT,
    KERNEL_EVENT_POST_EXIT,
    KERNEL_EVENT_COUNT
} kernel_event_t;

void kernel_init(int argc, char* argv[]);
void kernel_update();
void kernel_set_dt(uint8_t milis);
void kernel_exit();

//! @file src/engine_event.c
void engine_install();
void engine_keypress(const char *const , uint8_t);

//! @file src/kernel_error.c
void kernel_add_error(const char*);
bool kernel_has_error();
const char *const kernel_get_error();

//! @file src/kernel_event.c
void kernel_event_install(kernel_event_t, void*);
void kernel_event_callback(kernel_event_t, kernel_event_t);

//! @file src/kernel_runtime.c
bool kernel_runtime_online();
void kernel_runtime_quit();
int kernel_runtime_get_status();

//! @file src/lua.c
void lua_install();

void native_draw_install();
void native_http_install();
void native_json_install();

//! @file src/sdl_api.c
void sdl_set_title(char* new_title);
char *const sdl_get_title();

//! @file src/sdl_primitives.c
void sdl_install();

//! @file src/sdl_primitives.c
void sdl_draw_start();
void sdl_draw_flush();
void sdl_draw_clear(uint32_t c, double x, double y, double w, double h);
void sdl_draw_color(uint32_t c);
void sdl_draw_rect(uint8_t mode, double x, double y, double w, double h);
void sdl_draw_line(double x1, double y1, double x2, double y2);
