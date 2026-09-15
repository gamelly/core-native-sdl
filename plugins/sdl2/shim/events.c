#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "shim.h"

#define QUEUE_CAP 256

static SDL_Event       s_queue[QUEUE_CAP];
static int             s_head;
static int             s_tail;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static Uint8           s_keys[SDL_NUM_SCANCODES];
static SDL_Keymod      s_mod;
static int             s_ipc_fd = -1;
static uint64_t        s_orphan_deadline;

/* ── queue ───────────────────────────────────────────────────────── */

void shim_events_init(void) {
    pthread_mutex_lock(&s_lock);
    s_head = s_tail = 0;
    memset(s_keys, 0, sizeof(s_keys));
    s_mod = KMOD_NONE;
    pthread_mutex_unlock(&s_lock);
}

void shim_events_quit(void) {
    shim_events_init();
    shim_joystick_quit();
}

void shim_events_push(const SDL_Event *ev) {
    pthread_mutex_lock(&s_lock);
    int next = (s_head + 1) % QUEUE_CAP;
    if (next != s_tail) {
        s_queue[s_head] = *ev;
        s_head = next;
    }
    pthread_mutex_unlock(&s_lock);
}

static bool events_pop(SDL_Event *out) {
    bool ok = false;
    pthread_mutex_lock(&s_lock);
    if (s_tail != s_head) {
        *out   = s_queue[s_tail];
        s_tail = (s_tail + 1) % QUEUE_CAP;
        ok     = true;
    }
    pthread_mutex_unlock(&s_lock);
    return ok;
}

static bool events_pending(void) {
    pthread_mutex_lock(&s_lock);
    bool any = s_tail != s_head;
    pthread_mutex_unlock(&s_lock);
    return any;
}

static void mod_update(uint16_t scancode, bool pressed) {
    SDL_Keymod bit = KMOD_NONE;
    switch (scancode) {
        case SDL_SCANCODE_LSHIFT: bit = KMOD_LSHIFT; break;
        case SDL_SCANCODE_RSHIFT: bit = KMOD_RSHIFT; break;
        case SDL_SCANCODE_LCTRL:  bit = KMOD_LCTRL;  break;
        case SDL_SCANCODE_RCTRL:  bit = KMOD_RCTRL;  break;
        case SDL_SCANCODE_LALT:   bit = KMOD_LALT;   break;
        case SDL_SCANCODE_RALT:   bit = KMOD_RALT;   break;
        case SDL_SCANCODE_LGUI:   bit = KMOD_LGUI;   break;
        case SDL_SCANCODE_RGUI:   bit = KMOD_RGUI;   break;
        default: return;
    }
    if (pressed) s_mod |= bit;
    else         s_mod &= ~bit;
}

void shim_events_key(uint16_t scancode, uint32_t keycode, bool pressed) {
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type                = pressed ? SDL_KEYDOWN : SDL_KEYUP;
    ev.key.timestamp       = SDL_GetTicks();
    ev.key.windowID        = shim_window_id();
    ev.key.state           = pressed ? SDL_PRESSED : SDL_RELEASED;
    ev.key.repeat          = (pressed && scancode < SDL_NUM_SCANCODES && s_keys[scancode]) ? 1 : 0;
    ev.key.keysym.scancode = (SDL_Scancode)scancode;
    ev.key.keysym.sym      = (SDL_Keycode)keycode;
    mod_update(scancode, pressed);
    ev.key.keysym.mod      = (Uint16)s_mod;
    if (scancode < SDL_NUM_SCANCODES) s_keys[scancode] = pressed ? 1 : 0;
    shim_events_push(&ev);
}

void shim_events_window(uint8_t event, int32_t data1, int32_t data2) {
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type             = SDL_WINDOWEVENT;
    ev.window.timestamp = SDL_GetTicks();
    ev.window.windowID  = shim_window_id();
    ev.window.event     = event;
    ev.window.data1     = data1;
    ev.window.data2     = data2;
    shim_events_push(&ev);
}

static void events_quit_request(void) {
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type           = SDL_QUIT;
    ev.quit.timestamp = SDL_GetTicks();
    shim_events_push(&ev);
}

/* ── ipc client ──────────────────────────────────────────────────── */

void shim_ipc_connect(void) {
    const char *path = getenv(GECND_SDL2_ENV_SOCKET);
    if (!path || !path[0] || s_ipc_fd >= 0) return;

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);

    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (fd < 0) return;
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "[libSDL2-shim] connect %s failed: %s\n", path, strerror(errno));
        close(fd);
        return;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    s_ipc_fd = fd;
    shim_ipc_send(GECND_SDL2_PKT_HELLO, 0, 0, (uint32_t)getpid());
}

void shim_ipc_send(uint8_t type, uint8_t flag, uint16_t code, uint32_t arg) {
    if (s_ipc_fd < 0) return;
    gecnd_sdl2_pkt_t pkt = { type, flag, code, arg };
    send(s_ipc_fd, &pkt, sizeof(pkt), MSG_NOSIGNAL | MSG_DONTWAIT);
}

void shim_ipc_send_blob(uint8_t type, uint8_t flag, uint16_t code, uint32_t arg,
                        const void *payload, size_t bytes) {
    if (s_ipc_fd < 0) return;
    if (bytes > GECND_SDL2_PKT_MAX - sizeof(gecnd_sdl2_pkt_t)) return;

    /* Uma unica send() por mensagem: SOCK_SEQPACKET e' atomico por datagrama,
     * entao a thread de audio e a principal podem escrever sem lock e sem
     * risco de intercalar bytes. MSG_DONTWAIT faz o bloco ser descartado se o
     * host estiver atrasado, em vez de travar o audio. */
    uint8_t buf[GECND_SDL2_PKT_MAX];
    gecnd_sdl2_pkt_t pkt = { type, flag, code, arg };
    memcpy(buf, &pkt, sizeof(pkt));
    if (bytes && payload) memcpy(buf + sizeof(pkt), payload, bytes);
    send(s_ipc_fd, buf, sizeof(pkt) + bytes, MSG_NOSIGNAL | MSG_DONTWAIT);
}

void shim_ipc_close(void) {
    if (s_ipc_fd < 0) return;
    shim_ipc_send(GECND_SDL2_PKT_BYE, 0, 0, 0);
    close(s_ipc_fd);
    s_ipc_fd = -1;
}

static void ipc_lost(void) {
    close(s_ipc_fd);
    s_ipc_fd = -1;
    s_orphan_deadline = shim_now_ms() + 5000;
    fprintf(stderr, "[libSDL2-shim] host connection lost, requesting quit\n");
    events_quit_request();
}

void shim_ipc_pump(void) {
    if (s_ipc_fd < 0) {
        if (s_orphan_deadline && shim_now_ms() > s_orphan_deadline) _exit(0);
        return;
    }
    gecnd_sdl2_pkt_t pkt;
    for (;;) {
        ssize_t n = recv(s_ipc_fd, &pkt, sizeof(pkt), MSG_DONTWAIT);
        if (n == (ssize_t)sizeof(pkt)) {
            switch (pkt.type) {
                case GECND_SDL2_PKT_KEY:
                    if (getenv(GECND_SDL2_ENV_DEBUG)) {
                        fprintf(stderr, "[libSDL2-shim] key scancode=%u sym=%u press=%u\n",
                                pkt.code, pkt.arg, pkt.flag);
                    }
                    shim_events_key(pkt.code, pkt.arg, pkt.flag != 0);
                    break;
                case GECND_SDL2_PKT_PAD:
                    if (getenv(GECND_SDL2_ENV_DEBUG)) {
                        fprintf(stderr, "[libSDL2-shim] pad button=%u press=%u\n",
                                pkt.code, pkt.flag);
                    }
                    shim_joystick_input((uint8_t)pkt.code, pkt.flag != 0);
                    break;
                case GECND_SDL2_PKT_QUIT:
                    events_quit_request();
                    break;
                default:
                    break;
            }
            continue;
        }
        if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            ipc_lost();
        }
        return;
    }
}

void shim_events_pump(void) {
    /* Queue the pad's device-added events on the first pump, so the client has
     * it listed before any button arrives. Cheap after the first call. */
    shim_joystick_announce();
    shim_video_pump();
    shim_ipc_pump();
}

/* ── sdl api ─────────────────────────────────────────────────────── */

void SDL_PumpEvents(void) {
    shim_events_pump();
}

int SDL_PollEvent(SDL_Event *event) {
    SDL_PumpEvents();
    if (!event) return events_pending() ? 1 : 0;
    return events_pop(event) ? 1 : 0;
}

int SDL_WaitEventTimeout(SDL_Event *event, int timeout) {
    uint64_t deadline = timeout >= 0 ? shim_now_ms() + (uint64_t)timeout : 0;
    for (;;) {
        if (SDL_PollEvent(event)) return 1;
        if (timeout >= 0 && shim_now_ms() >= deadline) return 0;
        SDL_Delay(1);
    }
}

int SDL_WaitEvent(SDL_Event *event) {
    return SDL_WaitEventTimeout(event, -1);
}

int SDL_PushEvent(SDL_Event *event) {
    if (!event) return -1;
    event->common.timestamp = SDL_GetTicks();
    shim_events_push(event);
    return 1;
}

int SDL_PeepEvents(SDL_Event *events, int numevents, SDL_eventaction action,
                   Uint32 minType, Uint32 maxType) {
    if (!events || numevents <= 0) return 0;

    if (action == SDL_ADDEVENT) {
        for (int i = 0; i < numevents; i++) shim_events_push(&events[i]);
        return numevents;
    }

    SDL_Event kept[QUEUE_CAP];
    int       taken = 0;
    int       n     = 0;

    pthread_mutex_lock(&s_lock);
    while (s_tail != s_head) {
        SDL_Event ev = s_queue[s_tail];
        s_tail = (s_tail + 1) % QUEUE_CAP;

        bool match = ev.type >= minType && ev.type <= maxType && taken < numevents;
        if (match) {
            events[taken++] = ev;
            if (action == SDL_GETEVENT) continue;   /* consumed */
        }
        kept[n++] = ev;
    }
    s_head = s_tail = 0;
    for (int i = 0; i < n; i++) s_queue[s_head++] = kept[i];
    pthread_mutex_unlock(&s_lock);

    return taken;
}

static void events_filter(Uint32 minType, Uint32 maxType) {
    SDL_Event kept[QUEUE_CAP];
    int       n = 0;
    pthread_mutex_lock(&s_lock);
    while (s_tail != s_head) {
        SDL_Event ev = s_queue[s_tail];
        s_tail = (s_tail + 1) % QUEUE_CAP;
        if (ev.type < minType || ev.type > maxType) kept[n++] = ev;
    }
    s_head = s_tail = 0;
    for (int i = 0; i < n; i++) s_queue[s_head++] = kept[i];
    pthread_mutex_unlock(&s_lock);
}

void SDL_FlushEvent(Uint32 type) {
    events_filter(type, type);
}

void SDL_FlushEvents(Uint32 minType, Uint32 maxType) {
    events_filter(minType, maxType);
}

SDL_bool SDL_HasEvents(Uint32 minType, Uint32 maxType) {
    SDL_bool any = SDL_FALSE;
    pthread_mutex_lock(&s_lock);
    for (int i = s_tail; i != s_head; i = (i + 1) % QUEUE_CAP) {
        if (s_queue[i].type >= minType && s_queue[i].type <= maxType) {
            any = SDL_TRUE;
            break;
        }
    }
    pthread_mutex_unlock(&s_lock);
    return any;
}

SDL_bool SDL_HasEvent(Uint32 type) {
    return SDL_HasEvents(type, type);
}

Uint8 SDL_EventState(Uint32 type, int state) {
    (void)type; (void)state;
    return SDL_ENABLE;
}

void SDL_SetEventFilter(SDL_EventFilter filter, void *userdata) {
    (void)filter; (void)userdata;
}

void SDL_AddEventWatch(SDL_EventFilter filter, void *userdata) {
    (void)filter; (void)userdata;
}

void SDL_DelEventWatch(SDL_EventFilter filter, void *userdata) {
    (void)filter; (void)userdata;
}

Uint32 SDL_RegisterEvents(int numevents) {
    static Uint32 next = SDL_USEREVENT;
    if (numevents <= 0 || next + (Uint32)numevents > SDL_LASTEVENT) return (Uint32)-1;
    Uint32 first = next;
    next += (Uint32)numevents;
    return first;
}

const Uint8 *SDL_GetKeyboardState(int *numkeys) {
    if (numkeys) *numkeys = SDL_NUM_SCANCODES;
    return s_keys;
}

SDL_Keymod SDL_GetModState(void) {
    return s_mod;
}

void SDL_SetModState(SDL_Keymod modstate) {
    s_mod = modstate;
}

SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode scancode) {
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) return 'a' + (scancode - SDL_SCANCODE_A);
    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) return '1' + (scancode - SDL_SCANCODE_1);
    if (scancode == SDL_SCANCODE_0) return '0';
    if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12) return SDLK_F1 + (scancode - SDL_SCANCODE_F1);
    switch (scancode) {
        case SDL_SCANCODE_RETURN:    return SDLK_RETURN;
        case SDL_SCANCODE_ESCAPE:    return SDLK_ESCAPE;
        case SDL_SCANCODE_BACKSPACE: return SDLK_BACKSPACE;
        case SDL_SCANCODE_TAB:       return SDLK_TAB;
        case SDL_SCANCODE_SPACE:     return SDLK_SPACE;
        default:                     return SDL_SCANCODE_TO_KEYCODE(scancode);
    }
}

SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode key) {
    if (key >= 'a' && key <= 'z') return (SDL_Scancode)(SDL_SCANCODE_A + (key - 'a'));
    if (key >= '1' && key <= '9') return (SDL_Scancode)(SDL_SCANCODE_1 + (key - '1'));
    if (key == '0') return SDL_SCANCODE_0;
    if (key & SDLK_SCANCODE_MASK) return (SDL_Scancode)(key & ~SDLK_SCANCODE_MASK);
    switch (key) {
        case SDLK_RETURN:    return SDL_SCANCODE_RETURN;
        case SDLK_ESCAPE:    return SDL_SCANCODE_ESCAPE;
        case SDLK_BACKSPACE: return SDL_SCANCODE_BACKSPACE;
        case SDLK_TAB:       return SDL_SCANCODE_TAB;
        case SDLK_SPACE:     return SDL_SCANCODE_SPACE;
        default:             return SDL_SCANCODE_UNKNOWN;
    }
}

const char *SDL_GetScancodeName(SDL_Scancode scancode) {
    static char name[16];
    snprintf(name, sizeof(name), "Scancode %d", (int)scancode);
    return name;
}

const char *SDL_GetKeyName(SDL_Keycode key) {
    static char name[16];
    if (key >= 0x20 && key < 0x7f) {
        snprintf(name, sizeof(name), "%c", (char)((key >= 'a' && key <= 'z') ? key - 32 : key));
    } else {
        snprintf(name, sizeof(name), "Key %d", (int)key);
    }
    return name;
}
