#define _GNU_SOURCE
#include "process.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

extern char **environ;

int sdlipc_spawn(const char *binary, const char *library_dir, pid_t *pid) {
    int pair[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair) < 0) return -1;
    size_t count = 0;
    while (environ[count]) count++;
    char **environment = calloc(count + 4, sizeof(*environment));
    char *library;
    if (!environment || asprintf(&library, "LD_LIBRARY_PATH=%s", library_dir) < 0) {
        free(environment); close(pair[0]); close(pair[1]); return -1;
    }
    size_t used = 0;
    for (size_t i = 0; i < count; i++) {
        if (strncmp(environ[i], "LD_LIBRARY_PATH=", 16) &&
            strncmp(environ[i], "SDL_VIDEODRIVER=", 16) &&
            strncmp(environ[i], "GECND_SDL2_FD=", 13)) environment[used++] = environ[i];
    }
    environment[used++] = library;
    environment[used++] = "SDL_VIDEODRIVER=offscreen";
    environment[used++] = "GECND_SDL2_FD=3";
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    int error = posix_spawn_file_actions_init(&actions);
    if (error) goto fail;
    error = posix_spawnattr_init(&attributes);
    if (error) { posix_spawn_file_actions_destroy(&actions); goto fail; }
    sigset_t mask, defaults;
    sigemptyset(&mask);
    sigemptyset(&defaults);
    sigaddset(&defaults, SIGPIPE);
    sigaddset(&defaults, SIGTERM);
    if (!(error = posix_spawn_file_actions_adddup2(&actions, pair[1], 3)) &&
        !(error = posix_spawn_file_actions_addclosefrom_np(&actions, 4)) &&
        !(error = posix_spawnattr_setsigmask(&attributes, &mask)) &&
        !(error = posix_spawnattr_setsigdefault(&attributes, &defaults)) &&
        !(error = posix_spawnattr_setpgroup(&attributes, 0)) &&
        !(error = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETPGROUP))) {
        char *argv[] = { (char *)binary, NULL };
        error = posix_spawn(pid, binary, &actions, &attributes, argv, environment);
    }
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
fail:
    close(pair[1]);
    free(library);
    free(environment);
    if (error) { close(pair[0]); errno = error; return -1; }
    return pair[0];
}
