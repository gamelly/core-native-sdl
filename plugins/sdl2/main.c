#define _GNU_SOURCE
#include "gecnd.h"
#include "gdmsp.h"
#include "gpu.h"
#include "process.h"
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static gecnd_api_t *api;
static int socket_fd = -1;
static pid_t child;
static sdlipc_gpu gpu;
static EGLImageKHR images[2];
static uint32_t widths[2], heights[2];
static int current = -1;
static GLuint texture;
static gdmsp_fsm_t state = GDMSP_FSM_IDLE;
static char library_dir[PATH_MAX];
static struct timespec stop_time;
static unsigned long frames;
static uintptr_t (*fbo_get)(void);
static void (*fbo_ensure)(int, int);
static void (*fbo_destroy)(void);
static void (*set_active)(bool, bool);
static void (*restore_context)(void);

static void release_graphics(void) {
    if (!gpu.display) return;
    set_active(false, false);
    fbo_destroy();
    for (unsigned i = 0; i < 2; i++) {
        if (images[i]) gpu.destroy_image(gpu.display, images[i]);
        images[i] = EGL_NO_IMAGE_KHR;
    }
    memset(&gpu, 0, sizeof(gpu));
    texture = 0;
    current = -1;
}

static void stop_child(void) {
    if (socket_fd >= 0) { close(socket_fd); socket_fd = -1; }
    if (child > 0) {
        kill(-child, SIGCONT);
        kill(-child, SIGTERM);
        clock_gettime(CLOCK_MONOTONIC, &stop_time);
    }
    release_graphics();
}

static void exit_child(void) {
    if (child > 0) {
        kill(-child, SIGKILL);
        while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {}
    }
}

static gdmsp_fsm_t error_state(const char *operation) {
    fprintf(stderr, "[sdl2-ipc] %s: %s (EGL=0x%x)\n", operation, strerror(errno), eglGetError());
    stop_child();
    state = GDMSP_FSM_ERROR;
    return state;
}

static gdmsp_fsm_t source(uint8_t channel, const char *url, void *usr) {
    (void)usr;
    if (channel != 0 || child > 0) return GDMSP_FSM_ERROR;
    const char *binary;
    if (!strncmp(url, "sdl://", 6)) binary = url + 6;
    else if (!strncmp(url, "sdl2://", 7)) binary = url + 7;
    else return GDMSP_FSM_ERROR;
    if (!*binary || !*library_dir) return GDMSP_FSM_ERROR;
    socket_fd = sdlipc_spawn(binary, library_dir, &child);
    if (socket_fd < 0) {
        fprintf(stderr, "[sdl2-ipc] spawn %s: %s\n", binary, strerror(errno));
        return GDMSP_FSM_ERROR;
    }
    frames = 0;
    state = GDMSP_FSM_LOADING;
    fprintf(stderr, "[sdl2-ipc] spawned pid=%ld binary=%s\n", (long)child, binary);
    return state;
}

static int present(const sdlipc_message *message, int *fds, size_t count) {
    unsigned slot = message->slot;
    if (slot >= 2) return -1;
    if (message->type == SDLIPC_BUFFER) {
        if (!message->planes || count != message->planes || count > SDLIPC_PLANES ||
            !message->width || !message->height || message->width > 16384 || message->height > 16384 || images[slot]) return -1;
        images[slot] = sdlipc_gpu_import(&gpu, message, fds);
        widths[slot] = message->width;
        heights[slot] = message->height;
        return images[slot] ? 0 : -1;
    }
    if (message->type != SDLIPC_FRAME || count != 1 || !images[slot] || current == (int)slot) return -1;
    int fence = fds[0];
    fds[0] = -1;
    if (sdlipc_gpu_wait(&gpu, fence) < 0) return -1;
    if (current >= 0) {
        fence = sdlipc_gpu_fence(&gpu);
        if (fence < 0) return -1;
        sdlipc_message release = { .version = SDLIPC_VERSION, .type = SDLIPC_RELEASE, .slot = current };
        int result = sdlipc_send(socket_fd, &release, &fence, 1);
        close(fence);
        if (result < 0) return -1;
    }
    fbo_ensure(widths[slot], heights[slot]);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_get());
    GLint attachment;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &attachment);
    texture = attachment;
    glBindTexture(GL_TEXTURE_2D, texture);
    gpu.bind_image(GL_TEXTURE_2D, images[slot]);
    glBindTexture(GL_TEXTURE_2D, 0);
    restore_context();
    if (glGetError() != GL_NO_ERROR) return -1;
    set_active(true, true);
    current = slot;
    state = GDMSP_FSM_PLAYING;
    if (++frames == 1) fprintf(stderr, "[sdl2-ipc] presenting DMA-BUF %ux%u\n", widths[slot], heights[slot]);
    return 1;
}

static gdmsp_fsm_t tick(void) {
    if (child <= 0) return state;
    int status;
    pid_t result = waitpid(child, &status, WNOHANG);
    if (result == child) {
        fprintf(stderr, "[sdl2-ipc] child exited status=%d frames=%lu\n", status, frames);
        child = 0;
        stop_child();
        if (state != GDMSP_FSM_STOPPING && (!WIFEXITED(status) || WEXITSTATUS(status))) state = GDMSP_FSM_ERROR;
        else state = GDMSP_FSM_IDLE;
        return state;
    }
    if (result < 0) return error_state("waitpid");
    if (socket_fd < 0) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - stop_time.tv_sec >= 2) kill(-child, SIGKILL);
        return state;
    }
    if (state == GDMSP_FSM_PAUSED) return state;
    if (!fbo_get) {
#define BIND(field, name) api->registry("get", "function:" name, (void *)&field, NULL); if (!field) return error_state("missing " name)
        BIND(fbo_ensure, "ge_hw_fbo_ensure");
        BIND(fbo_destroy, "ge_hw_fbo_destroy");
        BIND(set_active, "ge_hw_set_active");
        BIND(restore_context, "ge_hw_restore_context");
        BIND(fbo_get, "ge_hw_fbo_get");
#undef BIND
    }
    if (!gpu.display && sdlipc_gpu_init(&gpu) < 0) {
        memset(&gpu, 0, sizeof(gpu));
        return error_state("requires current EGL context and DMA-BUF/native-fence support");
    }
    for (unsigned i = 0; i < 3; i++) {
        sdlipc_message message;
        int fds[SDLIPC_PLANES];
        size_t count;
        if (sdlipc_receive(socket_fd, &message, fds, &count, MSG_DONTWAIT) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return error_state("receive");
        }
        int received = present(&message, fds, count);
        for (size_t j = 0; j < count; j++) if (fds[j] >= 0) close(fds[j]);
        if (received < 0) return error_state("invalid GPU frame");
        if (received == 1) break;
    }
    return state;
}

static gdmsp_fsm_t command(uint8_t channel, gdmsp_cmd_t cmd, gdmsp_value_t value, void *usr) {
    (void)value; (void)usr;
    if (channel != 0) return GDMSP_FSM_ERROR;
    switch (cmd) {
        case GDMSP_CMD_TICK: return tick();
        case GDMSP_CMD_RESOURCE:
        case GDMSP_CMD_STOP:
            stop_child();
            state = child > 0 ? GDMSP_FSM_STOPPING : GDMSP_FSM_IDLE;
            break;
        case GDMSP_CMD_PAUSE:
            if (child > 0 && state == GDMSP_FSM_PLAYING && kill(-child, SIGSTOP) == 0) state = GDMSP_FSM_PAUSED;
            break;
        case GDMSP_CMD_PLAY:
            if (child > 0 && state == GDMSP_FSM_PAUSED && kill(-child, SIGCONT) == 0) state = GDMSP_FSM_PLAYING;
            break;
        default: break;
    }
    return state;
}

static gdmsp_value_t get(uint8_t channel, gdmsp_cmd_t cmd, void *usr) {
    (void)channel; (void)cmd; (void)usr;
    return (gdmsp_value_t){ .i64 = -1 };
}

static void on_key(uint32_t key, bool pressed, int port) {
    (void)port;
    if (socket_fd < 0 || (state != GDMSP_FSM_PLAYING && state != GDMSP_FSM_LOADING)) return;
    sdlipc_message message = { .version = SDLIPC_VERSION, .type = SDLIPC_KEY, .key = key, .pressed = pressed };
    if (sdlipc_send(socket_fd, &message, NULL, 0) < 0) error_state("input send");
}

void coreopen_sdl2_gecnd(gecnd_plugin_t *plugin) {
    api = plugin->require("v1");
    Dl_info location;
    char path[PATH_MAX];
    if (!dladdr((void *)coreopen_sdl2_gecnd, &location) || !realpath(location.dli_fname, path)) return;
    char *slash = strrchr(path, '/');
    *slash = '\0';
    if (snprintf(library_dir, sizeof(library_dir), "%s/sdl2", path) >= (int)sizeof(library_dir)) { library_dir[0] = '\0'; return; }
#define BIND(field, name) api->registry("get", "function:" name, (void *)&field, NULL); if (!field) return
    typeof(gamely_daemon_input_add_keycode) *add_keycode = NULL;
    typeof(gamely_input_add_cb) *add_cb = NULL;
    BIND(add_keycode, "gamely_daemon_input_add_keycode");
    BIND(add_cb, "gamely_input_add_cb");
#undef BIND
    static const struct { const char *name; uint32_t code; } keys[] = {
        { "up", 82 }, { "down", 81 }, { "left", 80 }, { "right", 79 },
        { "a", 40 }, { "b", 44 }, { "c", 224 }, { "d", 41 }, { "menu", 41 }
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(*keys); i++) add_keycode("sdl2_ipc", keys[i].name, keys[i].code);
    if (!add_cb(":sdl2_ipc", on_key, NULL)) return;
    static gdmsp_player_t player = { .src = source, .set = command, .get = get };
    api->registry("set", "media_player:sdl$0", &player, NULL);
    api->registry("set", "media_player:sdl2$0", &player, NULL);
    atexit(exit_child);
}
