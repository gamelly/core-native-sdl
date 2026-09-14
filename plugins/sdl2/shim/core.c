#define _GNU_SOURCE
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include "shim.h"

static char     s_error[512];
static Uint32   s_inited;
static uint64_t s_start_ms;
static char    *s_clipboard;
static SDL_bool s_text_input;

uint64_t shim_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}

void shim_set_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_error, sizeof(s_error), fmt, ap);
    va_end(ap);
    fprintf(stderr, "[libSDL2-shim] %s\n", s_error);
}

/* ── init ────────────────────────────────────────────────────────── */

int SDL_InitSubSystem(Uint32 flags) {
    if (!s_start_ms) s_start_ms = shim_now_ms();
    if (!s_inited) {
        shim_events_init();
        shim_ipc_connect();
    }
    s_inited |= flags | SDL_INIT_EVENTS;
    return 0;
}

int SDL_Init(Uint32 flags) {
    return SDL_InitSubSystem(flags);
}

void SDL_QuitSubSystem(Uint32 flags) {
    s_inited &= ~flags;
}

Uint32 SDL_WasInit(Uint32 flags) {
    return flags ? (s_inited & flags) : s_inited;
}

void SDL_Quit(void) {
    shim_video_quit();
    shim_ipc_close();
    shim_events_quit();
    s_inited = 0;
}

/* ── error ───────────────────────────────────────────────────────── */

const char *SDL_GetError(void) {
    return s_error;
}

int SDL_SetError(SDL_PRINTF_FORMAT_STRING const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_error, sizeof(s_error), fmt, ap);
    va_end(ap);
    return -1;
}

void SDL_ClearError(void) {
    s_error[0] = '\0';
}

char *SDL_GetErrorMsg(char *errstr, int maxlen) {
    if (errstr && maxlen > 0) snprintf(errstr, (size_t)maxlen, "%s", s_error);
    return errstr;
}

/* ── version / hints / platform ──────────────────────────────────── */

void SDL_GetVersion(SDL_version *ver) {
    if (!ver) return;
    ver->major = SDL_MAJOR_VERSION;
    ver->minor = SDL_MINOR_VERSION;
    ver->patch = SDL_PATCHLEVEL;
}

const char *SDL_GetRevision(void) {
    return "gecnd-ipc-shim";
}

int SDL_GetRevisionNumber(void) {
    return 0;
}

const char *SDL_GetPlatform(void) {
    return "Linux";
}

SDL_bool SDL_SetHint(const char *name, const char *value) {
    (void)name; (void)value;
    return SDL_TRUE;
}

SDL_bool SDL_SetHintWithPriority(const char *name, const char *value, SDL_HintPriority priority) {
    (void)name; (void)value; (void)priority;
    return SDL_TRUE;
}

const char *SDL_GetHint(const char *name) {
    (void)name;
    return NULL;
}

SDL_bool SDL_GetHintBoolean(const char *name, SDL_bool default_value) {
    (void)name;
    return default_value;
}

/* ── time ────────────────────────────────────────────────────────── */

Uint64 SDL_GetTicks64(void) {
    if (!s_start_ms) s_start_ms = shim_now_ms();
    return shim_now_ms() - s_start_ms;
}

Uint32 SDL_GetTicks(void) {
    return (Uint32)SDL_GetTicks64();
}

Uint64 SDL_GetPerformanceCounter(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (Uint64)ts.tv_sec * 1000000000ull + (Uint64)ts.tv_nsec;
}

Uint64 SDL_GetPerformanceFrequency(void) {
    return 1000000000ull;
}

void SDL_Delay(Uint32 ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) != 0) {}
}

/* ── memory ──────────────────────────────────────────────────────── */

void *SDL_malloc(size_t size) {
    return malloc(size);
}

void *SDL_calloc(size_t nmemb, size_t size) {
    return calloc(nmemb, size);
}

void *SDL_realloc(void *mem, size_t size) {
    return realloc(mem, size);
}

void SDL_free(void *mem) {
    free(mem);
}

/* ── cpu ─────────────────────────────────────────────────────────── */

#if defined(__x86_64__) || defined(__i386__)
#define CPU_HAS(feature) (__builtin_cpu_supports(feature) ? SDL_TRUE : SDL_FALSE)
#else
#define CPU_HAS(feature) SDL_FALSE
#endif

SDL_bool SDL_HasSSE(void)    { return CPU_HAS("sse"); }
SDL_bool SDL_HasSSE2(void)   { return CPU_HAS("sse2"); }
SDL_bool SDL_HasSSE3(void)   { return CPU_HAS("sse3"); }
SDL_bool SDL_HasSSE41(void)  { return CPU_HAS("sse4.1"); }
SDL_bool SDL_HasSSE42(void)  { return CPU_HAS("sse4.2"); }
SDL_bool SDL_HasAVX(void)    { return CPU_HAS("avx"); }
SDL_bool SDL_HasAVX2(void)   { return CPU_HAS("avx2"); }
SDL_bool SDL_HasAVX512F(void){ return CPU_HAS("avx512f"); }
SDL_bool SDL_HasNEON(void)   { return SDL_FALSE; }

int SDL_GetCPUCount(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
}

int SDL_GetSystemRAM(void) {
    long pages = sysconf(_SC_PHYS_PAGES);
    long psize = sysconf(_SC_PAGESIZE);
    if (pages <= 0 || psize <= 0) return 0;
    return (int)(((Uint64)pages * (Uint64)psize) / (1024u * 1024u));
}

/* ── clipboard ───────────────────────────────────────────────────── */

char *SDL_GetClipboardText(void) {
    return strdup(s_clipboard ? s_clipboard : "");
}

int SDL_SetClipboardText(const char *text) {
    free(s_clipboard);
    s_clipboard = text ? strdup(text) : NULL;
    return 0;
}

SDL_bool SDL_HasClipboardText(void) {
    return (s_clipboard && s_clipboard[0]) ? SDL_TRUE : SDL_FALSE;
}

/* ── text input / keyboard ui ────────────────────────────────────── */

void SDL_StartTextInput(void) {
    s_text_input = SDL_TRUE;
}

void SDL_StopTextInput(void) {
    s_text_input = SDL_FALSE;
}

SDL_bool SDL_IsTextInputActive(void) {
    return s_text_input;
}

void SDL_SetTextInputRect(const SDL_Rect *rect) {
    (void)rect;
}

SDL_bool SDL_HasScreenKeyboardSupport(void) {
    return SDL_FALSE;
}

SDL_bool SDL_IsScreenKeyboardShown(SDL_Window *window) {
    (void)window;
    return SDL_FALSE;
}

/* ── mouse ───────────────────────────────────────────────────────── */

int SDL_WarpMouseGlobal(int x, int y) {
    (void)x; (void)y;
    return 0;
}

void SDL_WarpMouseInWindow(SDL_Window *window, int x, int y) {
    (void)window; (void)x; (void)y;
}

int SDL_SetRelativeMouseMode(SDL_bool enabled) {
    (void)enabled;
    return 0;
}

SDL_bool SDL_GetRelativeMouseMode(void) {
    return SDL_FALSE;
}

Uint32 SDL_GetMouseState(int *x, int *y) {
    if (x) *x = 0;
    if (y) *y = 0;
    return 0;
}

Uint32 SDL_GetGlobalMouseState(int *x, int *y) {
    return SDL_GetMouseState(x, y);
}

int SDL_CaptureMouse(SDL_bool enabled) {
    (void)enabled;
    return 0;
}
