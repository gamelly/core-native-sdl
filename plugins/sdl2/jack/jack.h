/* ABI minima do JACK, so o que o backend do openal-soft consome.
 *
 * Nao dependemos do <jack/jack.h> de propósito: o device nao tem JACK
 * instalado (e' justamente por isso que da' pra ocupar o soname). Os tipos
 * aqui tem que casar com a ABI real, porque quem chama foi compilado contra
 * os cabecalhos de verdade.
 */

#ifndef GECND_FAKE_JACK_H
#define GECND_FAKE_JACK_H

#include <stdint.h>

typedef struct _jack_client jack_client_t;
typedef struct _jack_port   jack_port_t;

typedef uint32_t jack_nframes_t;
typedef uint64_t jack_time_t;
typedef float    jack_default_audio_sample_t;

typedef enum {
    JackNullOption    = 0x00,
    JackNoStartServer = 0x01,
    JackUseExactName  = 0x02,
    JackServerName    = 0x04,
    JackLoadName      = 0x08,
    JackLoadInit      = 0x10,
    JackSessionID     = 0x20,
} jack_options_t;

typedef enum {
    JackFailure       = 0x01,
    JackInvalidOption = 0x02,
    JackNameNotUnique = 0x04,
    JackServerStarted = 0x08,
    JackServerFailed  = 0x10,
    JackServerError   = 0x20,
    JackNoSuchClient  = 0x40,
} jack_status_t;

enum JackPortFlags {
    JackPortIsInput    = 0x01,
    JackPortIsOutput   = 0x02,
    JackPortIsPhysical = 0x04,
    JackPortCanMonitor = 0x08,
    JackPortIsTerminal = 0x10,
};

#define JACK_DEFAULT_AUDIO_TYPE "32 bit float mono audio"

typedef int  (*JackProcessCallback)   (jack_nframes_t nframes, void *arg);
typedef int  (*JackBufferSizeCallback)(jack_nframes_t nframes, void *arg);
typedef int  (*JackSampleRateCallback)(jack_nframes_t nframes, void *arg);
typedef int  (*JackXRunCallback)      (void *arg);
typedef void (*JackShutdownCallback)  (void *arg);
typedef void (*JackErrorCallback)     (const char *msg);

/* Variaveis globais, nao funcoes: o JACK real expoe estas como dado e o
 * cliente escreve nelas. O openal pega o endereco e atribui o handler dele. */
extern JackErrorCallback jack_error_callback;
extern JackErrorCallback jack_info_callback;

typedef struct {
    jack_nframes_t min;
    jack_nframes_t max;
} jack_latency_range_t;

typedef enum {
    JackCaptureLatency,
    JackPlaybackLatency,
} jack_latency_callback_mode_t;

#endif
