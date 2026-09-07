#include "ipc.h"
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int sdlipc_send(int socket, const sdlipc_message *message, const int *fds, size_t count) {
    if (count > SDLIPC_PLANES) { errno = EINVAL; return -1; }
    struct iovec io = { (void *)message, sizeof(*message) };
    union { struct cmsghdr align; char bytes[CMSG_SPACE(sizeof(int) * SDLIPC_PLANES)]; } control = {0};
    struct msghdr msg = { .msg_iov = &io, .msg_iovlen = 1 };
    if (count) {
        msg.msg_control = control.bytes;
        msg.msg_controllen = CMSG_SPACE(sizeof(int) * count);
        struct cmsghdr *c = CMSG_FIRSTHDR(&msg);
        c->cmsg_level = SOL_SOCKET;
        c->cmsg_type = SCM_RIGHTS;
        c->cmsg_len = CMSG_LEN(sizeof(int) * count);
        memcpy(CMSG_DATA(c), fds, sizeof(int) * count);
    }
    ssize_t result;
    do { result = sendmsg(socket, &msg, MSG_NOSIGNAL); } while (result < 0 && errno == EINTR);
    if (result == sizeof(*message)) return 0;
    if (result >= 0) errno = EIO;
    return -1;
}

int sdlipc_receive(int socket, sdlipc_message *message, int *fds, size_t *count, int flags) {
    struct iovec io = { message, sizeof(*message) };
    union { struct cmsghdr align; char bytes[CMSG_SPACE(sizeof(int) * SDLIPC_PLANES)]; } control = {0};
    struct msghdr msg = { .msg_iov = &io, .msg_iovlen = 1,
                         .msg_control = control.bytes, .msg_controllen = sizeof(control.bytes) };
    ssize_t result;
    *count = 0;
    do { result = recvmsg(socket, &msg, flags | MSG_CMSG_CLOEXEC); } while (result < 0 && errno == EINTR);
    if (result < 0) return -1;
    for (struct cmsghdr *c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
        if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS) continue;
        size_t n = (c->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        int *received = (int *)CMSG_DATA(c);
        for (size_t i = 0; i < n; i++) {
            if (*count < SDLIPC_PLANES) fds[(*count)++] = received[i];
            else close(received[i]);
        }
    }
    if (result != sizeof(*message) || (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) || message->version != SDLIPC_VERSION) {
        for (size_t i = 0; i < *count; i++) close(fds[i]);
        *count = 0;
        errno = result == 0 ? ECONNRESET : EPROTO;
        return -1;
    }
    return 0;
}
