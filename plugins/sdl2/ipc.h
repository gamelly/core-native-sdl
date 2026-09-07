#ifndef GECND_SDL2_IPC_H
#define GECND_SDL2_IPC_H

#include <stdint.h>
#include <stddef.h>

#define SDLIPC_VERSION 1u
#define SDLIPC_PLANES 4

enum { SDLIPC_BUFFER = 1, SDLIPC_FRAME, SDLIPC_RELEASE, SDLIPC_KEY };

typedef struct {
    uint32_t version, type, slot, width, height, format, planes;
    uint32_t stride[SDLIPC_PLANES], offset[SDLIPC_PLANES];
    uint64_t modifier;
    uint32_t key, pressed;
} sdlipc_message;

int sdlipc_send(int socket, const sdlipc_message *message, const int *fds, size_t count);
int sdlipc_receive(int socket, sdlipc_message *message, int *fds, size_t *count, int flags);

#endif
