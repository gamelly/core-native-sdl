#ifndef GECND_SDL2_IPC_H
#define GECND_SDL2_IPC_H

#include <stdint.h>

#define GECND_SDL2_ENV_SOCKET "GECND_SDL2_SOCKET"
#define GECND_SDL2_ENV_NATIVE "GECND_SDL2_NATIVE"
#define GECND_SDL2_ENV_X11KEYS "GECND_SDL2_X11_KEYS"
#define GECND_SDL2_SHIM_NAME  "libSDL2-2.0.so.0"

typedef enum __attribute__((packed)) {
    GECND_SDL2_PKT_NONE = 0,
    GECND_SDL2_PKT_HELLO,
    GECND_SDL2_PKT_BYE,
    GECND_SDL2_PKT_KEY,
    GECND_SDL2_PKT_QUIT,
    GECND_SDL2_PKT_WINDOW,
} gecnd_sdl2_pkt_type_t;

typedef struct {
    uint8_t  type;
    uint8_t  flag;
    uint16_t code;
    uint32_t arg;
} gecnd_sdl2_pkt_t;

#endif
