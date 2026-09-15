/* libjack.so.0 falso.
 *
 * Nao existe servidor JACK nenhum atras disto. O objetivo e' ocupar o soname
 * "libjack.so.0" para que o openal-soft escolha o backend jack — que e' o
 * PRIMEIRO da ordem de prioridade dele — e entregue o PCM aqui, de onde
 * seguimos por IPC para o host.
 *
 * Por que jack e nao alsa:
 *   - libjack.so.0 nao existe no device, entao ocupamos o nome sem conflitar
 *     com nada que hoje funcione (ao contrario de libasound.so.2, que existe
 *     e e' usada por todo o resto do sistema);
 *   - jack e' o primeiro backend tentado, entao nem precisa de ALSOFT_DRIVERS;
 *   - a API e' pequena e de callback, o que poe o relogio do lado de ca.
 *
 * Relogio: nao ha placa de som ditando o ritmo, entao a thread daqui chama o
 * process callback do cliente a cada BLOCK/rate segundos, com deadline
 * absoluto em CLOCK_MONOTONIC para nao acumular deriva. O cliente escreve
 * float32 mono por porta; convertemos para S16 interleaved e mandamos.
 *
 * Transporte: o socket ja e' aberto pelo shim do SDL2 no mesmo processo.
 * Alcancamos ele por dlsym(RTLD_DEFAULT, "shim_ipc_send_blob") em vez de
 * linkar, para que esta lib continue carregavel mesmo sem o shim presente —
 * nesse caso ela simplesmente descarta o audio em vez de falhar ao carregar.
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "jack.h"
#include "../ipc.h"

#define MAX_PORTS   GECND_SDL2_AUDIO_MAX_CHANNELS
#define BLOCK       1024u       /* frames por ciclo: ~21ms a 48 kHz */
#define DEFAULT_RATE 48000u

typedef void (*send_blob_fn)(uint8_t type, uint8_t flag, uint16_t code, uint32_t arg,
                             const void *payload, size_t bytes);

struct _jack_port {
    jack_client_t *client;
    char           name[64];
    unsigned long  flags;
    bool           used;
    float          buffer[GECND_SDL2_AUDIO_MAX_FRAMES];
};

struct _jack_client {
    char       name[64];
    bool       used;
    bool       active;
    bool       thread_up;
    pthread_t  thread;

    JackProcessCallback    process_cb;
    void                  *process_arg;
    JackBufferSizeCallback bufsize_cb;
    void                  *bufsize_arg;
    JackSampleRateCallback rate_cb;
    void                  *rate_arg;
    JackXRunCallback       xrun_cb;
    void                  *xrun_arg;

    unsigned               block;   /* frames por ciclo, ajustavel pelo cliente */

    struct _jack_port ports[MAX_PORTS];
    unsigned          nports;
};

int jack_deactivate(jack_client_t *client);   /* usada por jack_client_close */

static struct _jack_client s_clients[4];
static pthread_mutex_t     s_lock = PTHREAD_MUTEX_INITIALIZER;

/* simbolos de DADO exigidos pelo openal (ver jack.h) */
JackErrorCallback jack_error_callback;
JackErrorCallback jack_info_callback;
static send_blob_fn        s_send;
static bool                s_send_looked_up;
static unsigned            s_rate;
static bool                s_debug;

static unsigned rate_get(void) {
    if (!s_rate) {
        const char *env = getenv("GECND_AUDIO_RATE");
        long        v   = env ? strtol(env, NULL, 10) : 0;
        s_rate = (v >= 8000 && v <= 192000) ? (unsigned)v : DEFAULT_RATE;
        s_debug = getenv(GECND_SDL2_ENV_DEBUG) != NULL;
    }
    return s_rate;
}

static send_blob_fn transport(void) {
    if (!s_send_looked_up) {
        s_send_looked_up = true;
        /* RTLD_DEFAULT varre o escopo global: o shim do SDL2 entra por
         * LD_PRELOAD, entao ja esta la' quando o openal nos carrega. */
        s_send = (send_blob_fn)(uintptr_t)dlsym(RTLD_DEFAULT, "shim_ipc_send_blob");
        if (!s_send) {
            fprintf(stderr, "[libjack-shim] shim_ipc_send_blob not found, audio dropped\n");
        }
    }
    return s_send;
}

/* ── thread de audio ─────────────────────────────────────────────── */

static void emit(struct _jack_client *c, jack_nframes_t nframes) {
    send_blob_fn send = transport();
    if (!send || !c->nports) return;

    int16_t  pcm[GECND_SDL2_AUDIO_MAX_FRAMES * MAX_PORTS];
    unsigned ch = c->nports > MAX_PORTS ? MAX_PORTS : c->nports;

    for (jack_nframes_t f = 0; f < nframes; f++) {
        for (unsigned p = 0; p < ch; p++) {
            float v = c->ports[p].buffer[f];
            if (v >  1.0f) v =  1.0f;
            if (v < -1.0f) v = -1.0f;
            pcm[f * ch + p] = (int16_t)(v * 32767.0f);
        }
    }

    send(GECND_SDL2_PKT_AUDIO, (uint8_t)ch, (uint16_t)nframes, 0,
         pcm, (size_t)nframes * ch * sizeof(int16_t));
}

static void *audio_thread(void *arg) {
    struct _jack_client *c = arg;
    unsigned rate = rate_get();

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    while (c->active) {
        /* relido a cada ciclo: jack_set_buffer_size pode mudar em voo */
        unsigned block = c->block ? c->block : BLOCK;
        if (block > GECND_SDL2_AUDIO_MAX_FRAMES) block = GECND_SDL2_AUDIO_MAX_FRAMES;

        next.tv_nsec += (long)((1000000000.0 * block) / rate);
        while (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec  += 1;
        }
        /* deadline absoluto: um ciclo atrasado nao empurra os seguintes */
        while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL) == EINTR) {}

        if (!c->active) break;

        for (unsigned p = 0; p < c->nports; p++) {
            memset(c->ports[p].buffer, 0, block * sizeof(float));
        }
        if (c->process_cb) c->process_cb(block, c->process_arg);
        emit(c, block);
    }
    return NULL;
}

/* ── client ──────────────────────────────────────────────────────── */

static struct _jack_client *client_alloc(const char *name) {
    pthread_mutex_lock(&s_lock);
    struct _jack_client *c = NULL;
    for (size_t i = 0; i < sizeof(s_clients) / sizeof(*s_clients); i++) {
        if (!s_clients[i].used) { c = &s_clients[i]; break; }
    }
    if (c) {
        memset(c, 0, sizeof(*c));
        c->used  = true;
        c->block = BLOCK;
        snprintf(c->name, sizeof(c->name), "%s", name ? name : "client");
    }
    pthread_mutex_unlock(&s_lock);
    return c;
}

jack_client_t *jack_client_open(const char *client_name, jack_options_t options,
                                jack_status_t *status, ...) {
    (void)options;
    struct _jack_client *c = client_alloc(client_name);
    if (!c) {
        if (status) *status = JackFailure | JackServerError;
        return NULL;
    }
    if (status) *status = 0;
    if (s_debug) fprintf(stderr, "[libjack-shim] client_open '%s'\n", c->name);
    return c;
}

int jack_client_close(jack_client_t *client) {
    if (!client) return -1;
    jack_deactivate(client);
    pthread_mutex_lock(&s_lock);
    client->used = false;
    pthread_mutex_unlock(&s_lock);
    return 0;
}

int   jack_client_name_size(void)                  { return 64; }
char *jack_get_client_name(jack_client_t *client)  { return client ? client->name : NULL; }
int   jack_is_realtime(jack_client_t *client)      { (void)client; return 0; }

jack_nframes_t jack_get_sample_rate(jack_client_t *client) { (void)client; return rate_get(); }
jack_nframes_t jack_get_buffer_size(jack_client_t *client) {
    return client && client->block ? client->block : BLOCK;
}

int jack_set_buffer_size(jack_client_t *client, jack_nframes_t nframes) {
    if (!client) return -1;
    if (nframes < 64) nframes = 64;
    if (nframes > GECND_SDL2_AUDIO_MAX_FRAMES) nframes = GECND_SDL2_AUDIO_MAX_FRAMES;
    client->block = nframes;
    if (client->bufsize_cb) client->bufsize_cb(nframes, client->bufsize_arg);
    if (s_debug) fprintf(stderr, "[libjack-shim] buffer size -> %u\n", nframes);
    return 0;
}

int jack_client_real_time_priority(jack_client_t *client) { (void)client; return -1; }

const char *jack_get_version_string(void) { return "1.9.12"; }

size_t jack_port_type_size(void) { return 32; }

int jack_activate(jack_client_t *client) {
    if (!client || client->active) return client ? 0 : -1;
    client->active = true;
    if (pthread_create(&client->thread, NULL, audio_thread, client) != 0) {
        client->active = false;
        return -1;
    }
    client->thread_up = true;

    send_blob_fn send = transport();
    if (send) {
        unsigned ch = client->nports ? client->nports : 2;
        if (ch > MAX_PORTS) ch = MAX_PORTS;
        send(GECND_SDL2_PKT_AUDIO_CFG, 0, (uint16_t)ch, rate_get(), NULL, 0);
    }
    if (s_debug) {
        fprintf(stderr, "[libjack-shim] activate: %u Hz, %u ports, block %u\n",
                rate_get(), client->nports, BLOCK);
    }
    return 0;
}

int jack_deactivate(jack_client_t *client) {
    if (!client || !client->active) return 0;
    client->active = false;
    if (client->thread_up) {
        pthread_join(client->thread, NULL);
        client->thread_up = false;
    }
    send_blob_fn send = transport();
    if (send) send(GECND_SDL2_PKT_AUDIO_STOP, 0, 0, 0, NULL, 0);
    return 0;
}

/* ── ports ───────────────────────────────────────────────────────── */

jack_port_t *jack_port_register(jack_client_t *client, const char *port_name,
                                const char *port_type, unsigned long flags,
                                unsigned long buffer_size) {
    (void)port_type; (void)buffer_size;
    if (!client || client->nports >= MAX_PORTS) return NULL;

    struct _jack_port *p = &client->ports[client->nports];
    memset(p, 0, sizeof(*p));
    p->client = client;
    p->flags  = flags;
    p->used   = true;
    snprintf(p->name, sizeof(p->name), "%.28s:%.28s", client->name,
             port_name ? port_name : "out");
    client->nports++;
    return p;
}

int jack_port_unregister(jack_client_t *client, jack_port_t *port) {
    (void)client;
    if (port) port->used = false;
    return 0;
}

void *jack_port_get_buffer(jack_port_t *port, jack_nframes_t nframes) {
    if (!port || nframes > GECND_SDL2_AUDIO_MAX_FRAMES) return NULL;
    return port->buffer;
}

const char *jack_port_name(const jack_port_t *port) { return port ? port->name : NULL; }

void jack_port_get_latency_range(jack_port_t *port, jack_latency_callback_mode_t mode,
                                 jack_latency_range_t *range) {
    (void)port; (void)mode;
    if (range) { range->min = BLOCK; range->max = BLOCK; }
}

/* O openal aborta se nao achar portas fisicas de saida, entao anunciamos duas.
 * Os nomes so precisam sobreviver ate o jack_connect, que e' no-op. */
const char **jack_get_ports(jack_client_t *client, const char *port_name_pattern,
                            const char *type_name_pattern, unsigned long flags) {
    (void)client; (void)port_name_pattern; (void)type_name_pattern; (void)flags;
    const char **list = calloc(3, sizeof(char *));
    if (!list) return NULL;
    list[0] = strdup("system:playback_1");
    list[1] = strdup("system:playback_2");
    list[2] = NULL;
    if (!list[0] || !list[1]) {
        free((void *)list[0]); free((void *)list[1]); free(list);
        return NULL;
    }
    return list;
}

void jack_free(void *ptr) {
    if (!ptr) return;
    /* jack_get_ports devolve array NULL-terminado de strings do heap */
    char **list = ptr;
    for (size_t i = 0; list[i]; i++) free(list[i]);
    free(ptr);
}

int jack_connect(jack_client_t *client, const char *source_port,
                 const char *destination_port) {
    (void)client; (void)source_port; (void)destination_port;
    return 0;
}

int jack_disconnect(jack_client_t *client, const char *source_port,
                    const char *destination_port) {
    (void)client; (void)source_port; (void)destination_port;
    return 0;
}

/* ── callbacks ───────────────────────────────────────────────────── */

int jack_set_process_callback(jack_client_t *client, JackProcessCallback cb, void *arg) {
    if (!client) return -1;
    client->process_cb  = cb;
    client->process_arg = arg;
    return 0;
}

int jack_set_buffer_size_callback(jack_client_t *client, JackBufferSizeCallback cb, void *arg) {
    if (!client) return -1;
    client->bufsize_cb  = cb;
    client->bufsize_arg = arg;
    if (cb) cb(client->block ? client->block : BLOCK, arg);
    return 0;
}

int jack_set_sample_rate_callback(jack_client_t *client, JackSampleRateCallback cb, void *arg) {
    if (!client) return -1;
    client->rate_cb  = cb;
    client->rate_arg = arg;
    if (cb) cb(rate_get(), arg);
    return 0;
}

int jack_set_xrun_callback(jack_client_t *client, JackXRunCallback cb, void *arg) {
    if (!client) return -1;
    client->xrun_cb  = cb;
    client->xrun_arg = arg;
    return 0;
}

void jack_on_shutdown(jack_client_t *client, JackShutdownCallback cb, void *arg) {
    (void)client; (void)cb; (void)arg;
}

void jack_set_error_function(JackErrorCallback func) { jack_error_callback = func; }
void jack_set_info_function(JackErrorCallback func)  { jack_info_callback  = func; }

jack_nframes_t jack_frames_since_cycle_start(const jack_client_t *client) {
    (void)client; return 0;
}

jack_time_t jack_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (jack_time_t)ts.tv_sec * 1000000u + (jack_time_t)(ts.tv_nsec / 1000);
}
