#ifndef GECND_SDL2_IPC_H
#define GECND_SDL2_IPC_H

#include <stdint.h>

#define GECND_SDL2_ENV_SOCKET "GECND_SDL2_SOCKET"
#define GECND_SDL2_ENV_NATIVE "GECND_SDL2_NATIVE"
#define GECND_SDL2_ENV_X11KEYS "GECND_SDL2_X11_KEYS"
#define GECND_SDL2_ENV_DEBUG  "GECND_SDL2_DEBUG"
#define GECND_SDL2_ENV_SYNC   "GECND_SDL2_SYNC"
#define GECND_SDL2_SHIM_NAME  "libSDL2-2.0.so.0"

typedef enum __attribute__((packed)) {
    GECND_SDL2_PKT_NONE = 0,
    GECND_SDL2_PKT_HELLO,
    GECND_SDL2_PKT_BYE,
    GECND_SDL2_PKT_KEY,
    GECND_SDL2_PKT_QUIT,
    GECND_SDL2_PKT_WINDOW,
    GECND_SDL2_PKT_PAD,   /* flag = pressed, code = gecnd_sdl2_pad_t */

    /* Audio, sempre do filho para o host. AUDIO carrega payload depois do
     * cabecalho, entao o leitor do host le tamanho variavel; os outros dois
     * cabem no pacote fixo. */
    GECND_SDL2_PKT_AUDIO_CFG,   /* code = canais, arg = taxa em Hz     */
    GECND_SDL2_PKT_AUDIO,       /* flag = canais, code = frames, payload S16 */
    GECND_SDL2_PKT_AUDIO_STOP,
} gecnd_sdl2_pkt_type_t;

/* Virtual pad exposed by the shim. The first seven entries are SDL joystick
 * buttons b0..b6 in this exact order; the last four drive hat 0. The mapping
 * string in shim/joystick.c turns them into SDL_GameController buttons, so the
 * order here is part of the wire protocol — append, never reorder. */
typedef enum __attribute__((packed)) {
    GECND_SDL2_PAD_A = 0,  /* b0 -> "a"             */
    GECND_SDL2_PAD_B,      /* b1 -> "b"             */
    GECND_SDL2_PAD_C,      /* b2 -> "x"             */
    GECND_SDL2_PAD_D,      /* b3 -> "y"             */
    GECND_SDL2_PAD_E,      /* b4 -> "leftshoulder"  */
    GECND_SDL2_PAD_F,      /* b5 -> "rightshoulder" */
    GECND_SDL2_PAD_MENU,   /* b6 -> "start"         */
    GECND_SDL2_PAD_BUTTONS,

    GECND_SDL2_PAD_UP = GECND_SDL2_PAD_BUTTONS, /* hat 0 -> "dpup"    */
    GECND_SDL2_PAD_DOWN,                        /* hat 0 -> "dpdown"  */
    GECND_SDL2_PAD_LEFT,                        /* hat 0 -> "dpleft"  */
    GECND_SDL2_PAD_RIGHT,                       /* hat 0 -> "dpright" */
    GECND_SDL2_PAD_COUNT,
} gecnd_sdl2_pad_t;

#define GECND_SDL2_PAD_NAME "Core Native Pad"
#define GECND_SDL2_PAD_GUID "03000000000000000000006763636e64"

typedef struct {
    uint8_t  type;
    uint8_t  flag;
    uint16_t code;
    uint32_t arg;
} gecnd_sdl2_pkt_t;

#define GECND_SDL2_AUDIO_MAX_CHANNELS 2
#define GECND_SDL2_AUDIO_MAX_FRAMES   2048
#define GECND_SDL2_AUDIO_MAX_BYTES \
    (GECND_SDL2_AUDIO_MAX_FRAMES * GECND_SDL2_AUDIO_MAX_CHANNELS * 2)

/* maior mensagem que trafega no socket: cabecalho + um bloco de audio */
#define GECND_SDL2_PKT_MAX \
    (sizeof(gecnd_sdl2_pkt_t) + GECND_SDL2_AUDIO_MAX_BYTES)

#endif
