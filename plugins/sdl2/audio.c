/* Sink de audio do plugin.
 *
 * Ponto unico por onde o audio vindo do processo filho entra no core. O
 * filho nao fala com o core: ele manda PCM pelo socket e quem repassa e'
 * aqui. Hoje a fonte e' o libjack falso (jack/jack.c); amanha pode ser um
 * libasound ou libpulse falso — a fonte muda, este arquivo nao.
 *
 * Contrato com o filho: S16 interleaved, na taxa e canais anunciados pelo
 * ultimo audio_configure().
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "gecnd.h"
#include "main.h"
#include "ipc.h"

static struct {
    typeof(gamely_daemon_media_audio_configure) *configure;
    typeof(gamely_daemon_media_audio_push)      *push;
    typeof(gamely_daemon_media_audio_stop)      *stop;
    bool     tried;
    bool     ready;
    bool     warned;
    unsigned rate;
    unsigned channels;
    uint64_t frames;
} s;

static bool audio_bind(void) {
    if (s.tried) return s.ready;
    s.tried = true;

    api->registry("get", "function:gamely_daemon_media_audio_configure", (void *)&s.configure, NULL);
    api->registry("get", "function:gamely_daemon_media_audio_push",      (void *)&s.push,      NULL);
    api->registry("get", "function:gamely_daemon_media_audio_stop",      (void *)&s.stop,      NULL);

    s.ready = s.configure && s.push;
    if (!s.ready) fprintf(stderr, "[sdl2] audio service not in registry, audio dropped\n");
    return s.ready;
}

void audio_configure(unsigned rate, unsigned channels) {
    if (!audio_bind()) return;
    if (!rate || !channels) return;
    if (channels > GECND_SDL2_AUDIO_MAX_CHANNELS) channels = GECND_SDL2_AUDIO_MAX_CHANNELS;
    if (rate == s.rate && channels == s.channels) return;

    s.rate     = rate;
    s.channels = channels;
    s.frames   = 0;
    s.configure(rate, channels);
    fprintf(stderr, "[sdl2] audio %u Hz %u ch\n", rate, channels);
}

void audio_push(const int16_t *data, size_t frames) {
    if (!audio_bind() || !data || !frames) return;
    if (!s.rate) {
        /* o filho mandou PCM antes do configure; assume o comum em vez de
         * descartar, senao o primeiro bloco some sem explicacao */
        if (!s.warned) {
            fprintf(stderr, "[sdl2] audio before configure, assuming 48000 Hz stereo\n");
            s.warned = true;
        }
        audio_configure(48000, 2);
        if (!s.rate) return;
    }
    s.frames += frames;
    s.push(data, frames);
}

void audio_stop(void) {
    if (!s.ready || !s.stop) return;
    s.stop();
    s.rate = s.channels = 0;
}

void audio_reset(void) {
    audio_stop();
    s.frames = 0;
}
