/* Vinculo dos botoes do core.
 *
 * Padrao: tudo sai como gamepad virtual (o shim do SDL2 expoe um pad).
 * Com ?gptk=<arquivo>, os botoes LISTADOS no arquivo passam a sair como tecla;
 * os que o arquivo nao menciona continuam gamepad. Essa e' a propriedade util
 * do formato: o .gptk existe justamente para jogos sem suporte a gamepad,
 * entao "ausente" significa naturalmente "esse ja' funciona como pad".
 *
 * Precedencia:  gamepad (padrao)  <  arquivo .gptk  <  parametro na URL
 *
 * O formato aceito e' o do GPTokeyb, com duas concessoes:
 *   - os nomes de botao do core (c, d, e, f, menu) valem como apelido dos
 *     nomes do gptk (x, y, l1, r1, start), entao arquivos dos dois mundos
 *     funcionam sem traducao;
 *   - grafias alternativas de tecla (esc/escape, enter/return, left_shift/
 *     lshift) sao aceitas, porque circulam nos arquivos reais.
 *
 * O que nao da' para honrar, e e' avisado em vez de ignorado calado: entradas
 * de mouse (o shim nao emite eventos de mouse) e de analogico (o pad virtual
 * nao tem eixos).
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <SDL_scancode.h>
#include <SDL_keycode.h>

#include "main.h"
#include "ipc.h"

/* ── teclas ──────────────────────────────────────────────────────── */

typedef struct {
    const char *name;
    uint16_t    scancode;
    uint32_t    keycode;
} key_def_t;

static const key_def_t k_keys[] = {
    { "up",          SDL_SCANCODE_UP,           SDLK_UP           },
    { "down",        SDL_SCANCODE_DOWN,         SDLK_DOWN         },
    { "left",        SDL_SCANCODE_LEFT,         SDLK_LEFT         },
    { "right",       SDL_SCANCODE_RIGHT,        SDLK_RIGHT        },
    { "space",       SDL_SCANCODE_SPACE,        SDLK_SPACE        },
    { "return",      SDL_SCANCODE_RETURN,       SDLK_RETURN       },
    { "enter",       SDL_SCANCODE_RETURN,       SDLK_RETURN       },
    { "escape",      SDL_SCANCODE_ESCAPE,       SDLK_ESCAPE       },
    { "esc",         SDL_SCANCODE_ESCAPE,       SDLK_ESCAPE       },
    { "backspace",   SDL_SCANCODE_BACKSPACE,    SDLK_BACKSPACE    },
    { "tab",         SDL_SCANCODE_TAB,          SDLK_TAB          },
    { "delete",      SDL_SCANCODE_DELETE,       SDLK_DELETE       },
    { "insert",      SDL_SCANCODE_INSERT,       SDLK_INSERT       },
    { "home",        SDL_SCANCODE_HOME,         SDLK_HOME         },
    { "end",         SDL_SCANCODE_END,          SDLK_END          },
    { "pageup",      SDL_SCANCODE_PAGEUP,       SDLK_PAGEUP       },
    { "page_up",     SDL_SCANCODE_PAGEUP,       SDLK_PAGEUP       },
    { "pagedown",    SDL_SCANCODE_PAGEDOWN,     SDLK_PAGEDOWN     },
    { "page_down",   SDL_SCANCODE_PAGEDOWN,     SDLK_PAGEDOWN     },
    { "lshift",      SDL_SCANCODE_LSHIFT,       SDLK_LSHIFT       },
    { "left_shift",  SDL_SCANCODE_LSHIFT,       SDLK_LSHIFT       },
    { "rshift",      SDL_SCANCODE_RSHIFT,       SDLK_RSHIFT       },
    { "right_shift", SDL_SCANCODE_RSHIFT,       SDLK_RSHIFT       },
    { "lctrl",       SDL_SCANCODE_LCTRL,        SDLK_LCTRL        },
    { "left_ctrl",   SDL_SCANCODE_LCTRL,        SDLK_LCTRL        },
    { "rctrl",       SDL_SCANCODE_RCTRL,        SDLK_RCTRL        },
    { "right_ctrl",  SDL_SCANCODE_RCTRL,        SDLK_RCTRL        },
    { "lalt",        SDL_SCANCODE_LALT,         SDLK_LALT         },
    { "left_alt",    SDL_SCANCODE_LALT,         SDLK_LALT         },
    { "ralt",        SDL_SCANCODE_RALT,         SDLK_RALT         },
    { "right_alt",   SDL_SCANCODE_RALT,         SDLK_RALT         },
    { "capslock",    SDL_SCANCODE_CAPSLOCK,     SDLK_CAPSLOCK     },
    { "minus",       SDL_SCANCODE_MINUS,        SDLK_MINUS        },
    { "equals",      SDL_SCANCODE_EQUALS,       SDLK_EQUALS       },
    { "comma",       SDL_SCANCODE_COMMA,        SDLK_COMMA        },
    { "period",      SDL_SCANCODE_PERIOD,       SDLK_PERIOD       },
    { "slash",       SDL_SCANCODE_SLASH,        SDLK_SLASH        },
    { "backslash",   SDL_SCANCODE_BACKSLASH,    SDLK_BACKSLASH    },
    { "semicolon",   SDL_SCANCODE_SEMICOLON,    SDLK_SEMICOLON    },
    { "quote",       SDL_SCANCODE_APOSTROPHE,   SDLK_QUOTE        },
    { "leftbracket", SDL_SCANCODE_LEFTBRACKET,  SDLK_LEFTBRACKET  },
    { "left_brace",  SDL_SCANCODE_LEFTBRACKET,  SDLK_LEFTBRACKET  },
    { "rightbracket",SDL_SCANCODE_RIGHTBRACKET, SDLK_RIGHTBRACKET },
    { "right_brace", SDL_SCANCODE_RIGHTBRACKET, SDLK_RIGHTBRACKET },
    { "backquote",   SDL_SCANCODE_GRAVE,        SDLK_BACKQUOTE    },
};

static bool key_resolve(const char *name, uint16_t *scancode, uint32_t *keycode) {
    if (!name || !name[0]) return false;

    /* faixas regulares saem por aritmetica; so' o irregular precisa de tabela */
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

/* ── botoes do core ──────────────────────────────────────────────── */

typedef struct {
    const char  *core;
    uint8_t      pad;     /* para onde vai quando nao ha' override */
    gecnd_bind_t bind;
} entry_t;

static entry_t s_map[] = {
    { "up",    GECND_SDL2_PAD_UP,    {0,0,0,0} },
    { "down",  GECND_SDL2_PAD_DOWN,  {0,0,0,0} },
    { "left",  GECND_SDL2_PAD_LEFT,  {0,0,0,0} },
    { "right", GECND_SDL2_PAD_RIGHT, {0,0,0,0} },
    { "a",     GECND_SDL2_PAD_A,     {0,0,0,0} },
    { "b",     GECND_SDL2_PAD_B,     {0,0,0,0} },
    { "c",     GECND_SDL2_PAD_C,     {0,0,0,0} },
    { "d",     GECND_SDL2_PAD_D,     {0,0,0,0} },
    { "e",     GECND_SDL2_PAD_E,     {0,0,0,0} },
    { "f",     GECND_SDL2_PAD_F,     {0,0,0,0} },
    { "menu",  GECND_SDL2_PAD_MENU,  {0,0,0,0} },
};

#define MAP_COUNT (sizeof(s_map) / sizeof(*s_map))

/* Nomes do gptk que apontam para um botao nosso. Os que coincidem (a, b e as
 * quatro direcoes) nao precisam de linha aqui: o lookup tenta o nome do core
 * primeiro. */
static const struct { const char *alias; const char *core; } k_alias[] = {
    { "x",     "c"    },
    { "y",     "d"    },
    { "l1",    "e"    },
    { "r1",    "f"    },
    { "start", "menu" },
    { "back",  "menu" },
};

/* Nomes legitimos do gptk que este plugin nao tem como honrar. Distinguir isso
 * de "nome escrito errado" evita encher o log: um .gptk real quase sempre traz
 * as oito linhas de analogico e as de mouse. */
static const char *const k_unsupported[] = {
    "l2", "r2", "l3", "r3", "guide",
    "left_analog_up", "left_analog_down", "left_analog_left", "left_analog_right",
    "right_analog_up", "right_analog_down", "right_analog_left", "right_analog_right",
};

static entry_t *entry_find(const char *name) {
    for (size_t i = 0; i < MAP_COUNT; i++) {
        if (strcasecmp(s_map[i].core, name) == 0) return &s_map[i];
    }
    for (size_t i = 0; i < sizeof(k_alias) / sizeof(*k_alias); i++) {
        if (strcasecmp(k_alias[i].alias, name) != 0) continue;
        for (size_t j = 0; j < MAP_COUNT; j++) {
            if (strcmp(s_map[j].core, k_alias[i].core) == 0) return &s_map[j];
        }
    }
    return NULL;
}

static bool is_unsupported(const char *name) {
    for (size_t i = 0; i < sizeof(k_unsupported) / sizeof(*k_unsupported); i++) {
        if (strcasecmp(k_unsupported[i], name) == 0) return true;
    }
    return false;
}

/* ── parser do .gptk ─────────────────────────────────────────────── */

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

static void gptk_apply(const char *button, const char *value, int line) {
    entry_t *e = entry_find(button);
    if (!e) {
        if (!is_unsupported(button)) {
            fprintf(stderr, "[sdl2] gptk:%d unknown button '%s'\n", line, button);
        }
        return;
    }

    /* mouse_* e mouse_movement_* sao validos no gptk mas o shim nao emite
     * eventos de mouse; avisa em vez de deixar o botao mudo sem explicacao */
    if (strncasecmp(value, "mouse", 5) == 0) {
        fprintf(stderr, "[sdl2] gptk:%d '%s = %s': mouse not supported, staying on pad\n",
                line, button, value);
        return;
    }

    uint16_t scancode = 0;
    uint32_t keycode  = 0;
    if (!key_resolve(value, &scancode, &keycode)) {
        fprintf(stderr, "[sdl2] gptk:%d '%s = %s': unknown key, staying on pad\n",
                line, button, value);
        return;
    }

    e->bind.kind     = GECND_BIND_KEY;
    e->bind.scancode = scancode;
    e->bind.keycode  = keycode;
}

static bool gptk_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[sdl2] gptk: cannot open %s\n", path);
        return false;
    }

    char line[512];
    int  n = 0;
    while (fgets(line, sizeof(line), f)) {
        n++;
        char *s = line;

        char *hash = strchr(s, '#');
        if (hash) *hash = '\0';
        s = trim(s);
        if (!*s) continue;

        char *eq = strchr(s, '=');
        if (!eq) {
            fprintf(stderr, "[sdl2] gptk:%d ignored, no '=': %s\n", n, s);
            continue;
        }
        *eq = '\0';

        char *button = trim(s);
        char *value  = trim(eq + 1);

        /* valor entre aspas aparece nos arquivos que circulam */
        size_t vlen = strlen(value);
        if (vlen >= 2 && value[0] == '"' && value[vlen - 1] == '"') {
            value[vlen - 1] = '\0';
            value++;
        }
        if (!*button || !*value) continue;

        /* linhas de ajuste do gptokeyb, nao mapeamento */
        if (strcasecmp(button, "repeat_delay") == 0 ||
            strcasecmp(button, "repeat_rate")  == 0 ||
            strcasecmp(button, "deadzone")     == 0 ||
            strncasecmp(button, "deadzone_", 9) == 0 ||
            strncasecmp(button, "mouse_", 6)    == 0 ||
            strcasecmp(button, "overlay")      == 0 ||
            strcasecmp(button, "hotkey")       == 0) {
            continue;
        }

        gptk_apply(button, value, n);
    }

    fclose(f);
    return true;
}

/* Caminho relativo e' relativo a' pasta do executavel lancado, nao ao cwd do
 * host: quem escreve a URL pensa em termos da pasta do jogo. */
static void gptk_resolve(const char *given, const char *exec_path,
                         char *out, size_t cap) {
    if (given[0] == '/' || !exec_path || !exec_path[0]) {
        snprintf(out, cap, "%s", given);
        return;
    }

    char base[1024];
    snprintf(base, sizeof(base), "%s", exec_path);
    char *slash = strrchr(base, '/');
    if (!slash) {
        snprintf(out, cap, "%s", given);
        return;
    }
    *slash = '\0';
    snprintf(out, cap, "%s/%s", base, given);
}

/* ── api ─────────────────────────────────────────────────────────── */

void keymap_configure(const char *exec_path) {
    bool debug = getenv(GECND_SDL2_ENV_DEBUG) != NULL;

    for (size_t i = 0; i < MAP_COUNT; i++) {
        s_map[i].bind.kind = GECND_BIND_PAD;
        s_map[i].bind.pad  = s_map[i].pad;
    }

    const char *gptk = url_env_get("gptk");
    if (gptk && gptk[0]) {
        char path[1024];
        gptk_resolve(gptk, exec_path, path, sizeof(path));
        if (gptk_load(path)) {
            fprintf(stderr, "[sdl2] gptk loaded: %s\n", path);
        }
    }

    /* parametro solto na URL sobrepoe o arquivo: ?a=return */
    for (size_t i = 0; i < MAP_COUNT; i++) {
        const char *want = url_env_get(s_map[i].core);
        if (!want || !want[0]) continue;

        uint16_t scancode = 0;
        uint32_t keycode  = 0;
        if (!key_resolve(want, &scancode, &keycode)) {
            fprintf(stderr, "[sdl2] button '%s': unknown key '%s', staying on pad\n",
                    s_map[i].core, want);
            continue;
        }
        s_map[i].bind.kind     = GECND_BIND_KEY;
        s_map[i].bind.scancode = scancode;
        s_map[i].bind.keycode  = keycode;
    }

    if (debug) {
        for (size_t i = 0; i < MAP_COUNT; i++) {
            if (s_map[i].bind.kind == GECND_BIND_KEY) {
                fprintf(stderr, "[sdl2]   %-6s -> key scancode=%u\n",
                        s_map[i].core, s_map[i].bind.scancode);
            } else {
                fprintf(stderr, "[sdl2]   %-6s -> pad %u\n",
                        s_map[i].core, s_map[i].bind.pad);
            }
        }
    }
}

bool keymap_bind(const char *name, gecnd_bind_t *out) {
    if (!name || !out) return false;

    entry_t *e = entry_find(name);
    if (e) {
        *out = e->bind;
        return true;
    }

    /* Nome fora da tabela: o host pode mandar um botao que nao modelamos.
     * Tenta como tecla literal, que e' o que o comportamento antigo fazia. */
    memset(out, 0, sizeof(*out));
    if (!key_resolve(name, &out->scancode, &out->keycode)) return false;
    out->kind = GECND_BIND_KEY;
    return true;
}
