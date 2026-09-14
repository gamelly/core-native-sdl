#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <SDL_scancode.h>
#include <SDL_keycode.h>

#include "main.h"
#include "ipc.h"

typedef struct {
    const char *name;
    uint16_t    scancode;
    uint32_t    keycode;
} key_def_t;

static const key_def_t k_keys[] = {
    { "up",        SDL_SCANCODE_UP,        SDLK_UP        },
    { "down",      SDL_SCANCODE_DOWN,      SDLK_DOWN      },
    { "left",      SDL_SCANCODE_LEFT,      SDLK_LEFT      },
    { "right",     SDL_SCANCODE_RIGHT,     SDLK_RIGHT     },
    { "space",     SDL_SCANCODE_SPACE,     SDLK_SPACE     },
    { "return",    SDL_SCANCODE_RETURN,    SDLK_RETURN    },
    { "enter",     SDL_SCANCODE_RETURN,    SDLK_RETURN    },
    { "escape",    SDL_SCANCODE_ESCAPE,    SDLK_ESCAPE    },
    { "backspace", SDL_SCANCODE_BACKSPACE, SDLK_BACKSPACE },
    { "tab",       SDL_SCANCODE_TAB,       SDLK_TAB       },
    { "delete",    SDL_SCANCODE_DELETE,    SDLK_DELETE    },
    { "insert",    SDL_SCANCODE_INSERT,    SDLK_INSERT    },
    { "home",      SDL_SCANCODE_HOME,      SDLK_HOME      },
    { "end",       SDL_SCANCODE_END,       SDLK_END       },
    { "pageup",    SDL_SCANCODE_PAGEUP,    SDLK_PAGEUP    },
    { "pagedown",  SDL_SCANCODE_PAGEDOWN,  SDLK_PAGEDOWN  },
    { "lshift",    SDL_SCANCODE_LSHIFT,    SDLK_LSHIFT    },
    { "rshift",    SDL_SCANCODE_RSHIFT,    SDLK_RSHIFT    },
    { "lctrl",     SDL_SCANCODE_LCTRL,     SDLK_LCTRL     },
    { "rctrl",     SDL_SCANCODE_RCTRL,     SDLK_RCTRL     },
    { "lalt",      SDL_SCANCODE_LALT,      SDLK_LALT      },
    { "ralt",      SDL_SCANCODE_RALT,      SDLK_RALT      },
};

typedef struct {
    const char *core;
    const char *def;
    uint16_t    scancode;
    uint32_t    keycode;
    bool        valid;
} key_map_t;

static key_map_t s_map[] = {
    { "up",    "up",        0, 0, false },
    { "down",  "down",      0, 0, false },
    { "left",  "left",      0, 0, false },
    { "right", "right",     0, 0, false },
    { "a",     "return",    0, 0, false },
    { "b",     "escape",    0, 0, false },
    { "c",     "space",     0, 0, false },
    { "d",     "n",         0, 0, false },
    { "e",     "v",         0, 0, false },
    { "f",     "b",         0, 0, false },
    { "menu",  "backspace", 0, 0, false },
};

static bool key_resolve(const char *name, uint16_t *scancode, uint32_t *keycode) {
    if (!name || !name[0]) return false;

    if (strlen(name) == 1) {
        unsigned char c = (unsigned char)tolower((unsigned char)name[0]);
        if (c >= 'a' && c <= 'z') {
            *scancode = (uint16_t)(SDL_SCANCODE_A + (c - 'a'));
            *keycode  = c;
            return true;
        }
        if (c == '0') {
            *scancode = SDL_SCANCODE_0;
            *keycode  = c;
            return true;
        }
        if (c >= '1' && c <= '9') {
            *scancode = (uint16_t)(SDL_SCANCODE_1 + (c - '1'));
            *keycode  = c;
            return true;
        }
    }

    if ((name[0] == 'f' || name[0] == 'F') && isdigit((unsigned char)name[1])) {
        int n = atoi(name + 1);
        if (n >= 1 && n <= 12) {
            *scancode = (uint16_t)(SDL_SCANCODE_F1 + (n - 1));
            *keycode  = (uint32_t)(SDLK_F1 + (n - 1));
            return true;
        }
    }

    for (size_t i = 0; i < sizeof(k_keys) / sizeof(*k_keys); i++) {
        if (strcasecmp(k_keys[i].name, name) == 0) {
            *scancode = k_keys[i].scancode;
            *keycode  = k_keys[i].keycode;
            return true;
        }
    }
    return false;
}

void keymap_configure(void) {
    for (size_t i = 0; i < sizeof(s_map) / sizeof(*s_map); i++) {
        const char *want = url_env_get(s_map[i].core);
        if (!want || !want[0]) want = s_map[i].def;
        s_map[i].valid = key_resolve(want, &s_map[i].scancode, &s_map[i].keycode);
    }
}

/* ── pad ─────────────────────────────────────────────────────────── */

typedef struct {
    const char *core;
    uint8_t     pad;
} pad_map_t;

/* Fixed: the shim owns both ends of this, so the core button IS the pad
 * button. Direction keys become the hat, the rest become b0..b6. */
static const pad_map_t k_pad[] = {
    { "up",    GECND_SDL2_PAD_UP    },
    { "down",  GECND_SDL2_PAD_DOWN  },
    { "left",  GECND_SDL2_PAD_LEFT  },
    { "right", GECND_SDL2_PAD_RIGHT },
    { "a",     GECND_SDL2_PAD_A     },
    { "b",     GECND_SDL2_PAD_B     },
    { "c",     GECND_SDL2_PAD_C     },
    { "d",     GECND_SDL2_PAD_D     },
    { "e",     GECND_SDL2_PAD_E     },
    { "f",     GECND_SDL2_PAD_F     },
    { "menu",  GECND_SDL2_PAD_MENU  },
};

bool padmap_lookup(const char *name, uint8_t *pad) {
    if (!name) return false;
    for (size_t i = 0; i < sizeof(k_pad) / sizeof(*k_pad); i++) {
        if (strcmp(k_pad[i].core, name) != 0) continue;
        *pad = k_pad[i].pad;
        return true;
    }
    return false;
}

bool keymap_lookup(const char *name, uint16_t *scancode, uint32_t *keycode) {
    if (!name) return false;
    for (size_t i = 0; i < sizeof(s_map) / sizeof(*s_map); i++) {
        if (strcmp(s_map[i].core, name) != 0) continue;
        if (!s_map[i].valid) return false;
        *scancode = s_map[i].scancode;
        *keycode  = s_map[i].keycode;
        return true;
    }
    return key_resolve(name, scancode, keycode);
}
