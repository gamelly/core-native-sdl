#include <stdint.h>
#include <stddef.h>

#include "gecnd.h"
#include "gdmsp.h"

extern gecnd_api_t *api;

void        url_env_set(const char *url);
const char *url_env_get(const char *key);
void        url_resolve_rel(const char *given, const char *base_file, char *out, size_t cap);

/* Por padrao todo botao do core sai como gamepad virtual. Um arquivo .gptk
 * (?gptk=...) desvia para teclado os botoes que ele lista; os ausentes seguem
 * como gamepad. Um parametro solto na URL (?a=return) sobrepoe os dois. */
typedef enum {
    GECND_BIND_PAD = 0,
    GECND_BIND_KEY,
} gecnd_bind_kind_t;

typedef struct {
    gecnd_bind_kind_t kind;
    uint8_t           pad;        /* PAD: gecnd_sdl2_pad_t */
    uint16_t          scancode;   /* KEY */
    uint32_t          keycode;
} gecnd_bind_t;

/* exec_path resolve caminho relativo de ?gptk= contra a pasta do executavel */
void        keymap_configure(const char *exec_path);
bool        keymap_bind(const char *name, gecnd_bind_t *out);

bool        process_request(const char *path, const char *shim_dir);
void        process_stop(bool force);
void        process_tick(void);
bool        process_send_key(uint16_t scancode, uint32_t keycode, bool pressed);
bool        process_send_pad(uint8_t pad, bool pressed);

bool        process_is_running(void);
gdmsp_fsm_t process_state(void);
const char *process_error(void);
void        process_set_error(const char *fmt, ...);

void        audio_configure(unsigned rate, unsigned channels);
void        audio_push(const int16_t *data, size_t frames);
void        audio_stop(void);
void        audio_reset(void);
