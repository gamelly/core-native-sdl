#include "gecnd.h"
#include "gdmsp.h"

extern gecnd_api_t *api;

void        url_env_set(const char *url);
const char *url_env_get(const char *key);

void        keymap_configure(void);
bool        keymap_lookup(const char *name, uint16_t *scancode, uint32_t *keycode);
bool        padmap_lookup(const char *name, uint8_t *pad);

bool        process_request(const char *path, const char *shim_dir);
void        process_stop(bool force);
void        process_tick(void);
bool        process_send_key(uint16_t scancode, uint32_t keycode, bool pressed);
bool        process_send_pad(uint8_t pad, bool pressed);
bool        process_is_running(void);
gdmsp_fsm_t process_state(void);
const char *process_error(void);
void        process_set_error(const char *fmt, ...);
