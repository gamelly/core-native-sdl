#include "SDL_internal.h"
#include "SDL.h"
#include "events/SDL_keyboard_c.h"
#include "gpu.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

static sdlipc_gpu gpu;
static GLuint textures[2];
static int socket_fd = -1;
static int width, height;
static unsigned next_slot;
static int busy[2];

static void failed(const char *operation) {
    fprintf(stderr, "[sdl2-ipc] %s failed (errno=%d EGL=0x%x GL=0x%x)\n", operation, errno, eglGetError(), glGetError());
    _exit(EXIT_FAILURE);
}

static void connect_core(void) {
    if (socket_fd >= 0) return;
    const char *value = getenv("GECND_SDL2_FD");
    if (!value) failed("missing GECND_SDL2_FD");
    char *end;
    long fd = strtol(value, &end, 10);
    if (*end || fd < 0 || fd > 0x7fffffff) failed("invalid GECND_SDL2_FD");
    socket_fd = (int)fd;
}

static int receive_message(int flags) {
    sdlipc_message message;
    int fds[SDLIPC_PLANES];
    size_t count;
    if (sdlipc_receive(socket_fd, &message, fds, &count, flags) < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        failed("receive");
    }
    if (message.type == SDLIPC_RELEASE && message.slot < 2 && count == 1 && busy[message.slot]) {
        if (sdlipc_gpu_wait(&gpu, fds[0]) < 0) failed("release fence");
        busy[message.slot] = 0;
    } else if (message.type == SDLIPC_KEY && count == 0 && message.key < SDL_NUM_SCANCODES) {
        SDL_SendKeyboardKey(message.pressed ? SDL_PRESSED : SDL_RELEASED, (SDL_Scancode)message.key);
    } else {
        for (size_t i = 0; i < count; i++) close(fds[i]);
        failed("invalid message");
    }
    return 1;
}

void gecnd_sdl2_pump(void) {
    connect_core();
    if (!textures[0]) return;
    while (receive_message(MSG_DONTWAIT)) {}
}

int gecnd_sdl2_swap(SDL_Window *window) {
    connect_core();
    if (!textures[0]) {
        if (sdlipc_gpu_init(&gpu) < 0) failed("GPU initialization");
        eglQuerySurface(gpu.display, eglGetCurrentSurface(EGL_DRAW), EGL_WIDTH, &width);
        eglQuerySurface(gpu.display, eglGetCurrentSurface(EGL_DRAW), EGL_HEIGHT, &height);
        if (width <= 0 || height <= 0) failed("surface dimensions");
        GLint old_texture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
        glGenTextures(2, textures);
        for (unsigned slot = 0; slot < 2; slot++) {
            glBindTexture(GL_TEXTURE_2D, textures[slot]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            sdlipc_message message = { .version = SDLIPC_VERSION, .type = SDLIPC_BUFFER,
                .slot = slot, .width = width, .height = height };
            int fds[SDLIPC_PLANES];
            if (sdlipc_gpu_export(&gpu, textures[slot], &message, fds) < 0) failed("DMA-BUF export");
            int result = sdlipc_send(socket_fd, &message, fds, message.planes);
            for (uint32_t i = 0; i < message.planes; i++) close(fds[i]);
            if (result < 0) failed("buffer send");
        }
        glBindTexture(GL_TEXTURE_2D, old_texture);
        SDL_SetKeyboardFocus(window);
        fprintf(stderr, "[sdl2-ipc] GPU buffers exported: %dx%d, %s\n", width, height, glGetString(GL_RENDERER));
    }
    while (busy[next_slot]) receive_message(0);
    GLint old_texture, old_read;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, textures[next_slot]);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
    glBindTexture(GL_TEXTURE_2D, old_texture);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, old_read);
    int fence = sdlipc_gpu_fence(&gpu);
    if (fence < 0) failed("frame fence");
    sdlipc_message message = { .version = SDLIPC_VERSION, .type = SDLIPC_FRAME, .slot = next_slot };
    int result = sdlipc_send(socket_fd, &message, &fence, 1);
    close(fence);
    if (result < 0) failed("frame send");
    busy[next_slot] = 1;
    next_slot ^= 1;
    return 0;
}
