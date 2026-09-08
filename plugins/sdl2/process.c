#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include "main.h"
#include "ipc.h"

extern char **environ;

typedef enum {
    PROC_IDLE = 0,
    PROC_PENDING,
    PROC_SPAWNED,
    PROC_CONNECTED,
    PROC_STOPPING,
    PROC_FAILED,
} proc_phase_t;

static struct {
    proc_phase_t phase;
    pid_t        pid;
    int          listen_fd;
    int          client_fd;
    bool         term_sent;
    bool         background;
    bool         preload;
    unsigned     spawn_count;
    uint16_t     win_w;
    uint16_t     win_h;
    uint64_t     deadline_ms;
    char         sock_path[108];
    char         exec_path[PATH_MAX];
    char         shim_path[PATH_MAX];
    char         error[256];
} s = { .listen_fd = -1, .client_fd = -1, .preload = true };

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}

const char *process_error(void) {
    return s.error;
}

void process_set_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s.error, sizeof(s.error), fmt, ap);
    va_end(ap);
    fprintf(stderr, "[sdl2] %s\n", s.error);
}

/* ── sockets ─────────────────────────────────────────────────────── */

static void sockets_close(void) {
    if (s.client_fd >= 0) close(s.client_fd);
    if (s.listen_fd >= 0) close(s.listen_fd);
    s.client_fd = -1;
    s.listen_fd = -1;
    if (s.sock_path[0]) unlink(s.sock_path);
    s.sock_path[0] = '\0';
}

static bool listen_open(void) {
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf(s.sock_path, sizeof(s.sock_path), "/tmp/gecnd-sdl2-%d-%u.sock",
             (int)getpid(), ++s.spawn_count);
    unlink(s.sock_path);
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", s.sock_path);

    s.listen_fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (s.listen_fd < 0) {
        process_set_error("socket failed: %s", strerror(errno));
        return false;
    }
    if (bind(s.listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        process_set_error("bind %s failed: %s", s.sock_path, strerror(errno));
        sockets_close();
        return false;
    }
    if (listen(s.listen_fd, 1) != 0) {
        process_set_error("listen failed: %s", strerror(errno));
        sockets_close();
        return false;
    }
    return true;
}

static bool send_pkt(uint8_t type, uint8_t flag, uint16_t code, uint32_t arg) {
    if (s.client_fd < 0) return false;
    gecnd_sdl2_pkt_t pkt = { type, flag, code, arg };
    ssize_t n = send(s.client_fd, &pkt, sizeof(pkt), MSG_NOSIGNAL | MSG_DONTWAIT);
    return n == (ssize_t)sizeof(pkt);
}

static void accept_client(void) {
    if (s.listen_fd < 0 || s.client_fd >= 0) return;
    int fd = accept4(s.listen_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd >= 0) s.client_fd = fd;
}

static void read_packets(void) {
    if (s.client_fd < 0) return;
    gecnd_sdl2_pkt_t pkt;
    for (;;) {
        ssize_t n = recv(s.client_fd, &pkt, sizeof(pkt), MSG_DONTWAIT);
        if (n == (ssize_t)sizeof(pkt)) {
            switch (pkt.type) {
                case GECND_SDL2_PKT_HELLO:
                    if (s.phase == PROC_SPAWNED) {
                        s.phase      = PROC_CONNECTED;
                        s.background = true;
                        /* o filho é dono da tela: core para de desenhar */
                        api->registry("set", "core:state",
                                      (void *)(uintptr_t)GECND_FSM_RUNNING_BACKGROUND, NULL);
                    }
                    fprintf(stderr, "[sdl2] %s connected (pid %u)\n", GECND_SDL2_SHIM_NAME, pkt.arg);
                    break;
                case GECND_SDL2_PKT_WINDOW:
                    s.win_w = pkt.code;
                    s.win_h = (uint16_t)pkt.arg;
                    break;
                case GECND_SDL2_PKT_BYE:
                    close(s.client_fd);
                    s.client_fd = -1;
                    return;
                default:
                    break;
            }
            continue;
        }
        if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            close(s.client_fd);
            s.client_fd = -1;
        }
        return;
    }
}

/* ── spawn ───────────────────────────────────────────────────────── */

static bool env_is(const char *entry, const char *key) {
    size_t len = strlen(key);
    return strncmp(entry, key, len) == 0 && entry[len] == '=';
}

static char *env_join(const char *key, const char *head, const char *tail) {
    size_t cap = strlen(key) + strlen(head) + (tail ? strlen(tail) : 0) + 3;
    char  *out = malloc(cap);
    if (!out) return NULL;
    if (tail && tail[0]) {
        snprintf(out, cap, "%s=%s:%s", key, head, tail);
    } else {
        snprintf(out, cap, "%s=%s", key, head);
    }
    return out;
}

static char **env_build(void) {
    size_t count = 0;
    while (environ[count]) count++;

    char **env = calloc(count + 4, sizeof(char *));
    if (!env) return NULL;

    const char *old_ld_path = NULL;
    const char *old_preload = NULL;
    size_t      n           = 0;

    for (size_t i = 0; i < count; i++) {
        const char *e = environ[i];
        if (env_is(e, "LD_LIBRARY_PATH")) {
            old_ld_path = strchr(e, '=') + 1;
            continue;
        }
        if (env_is(e, "LD_PRELOAD")) {
            old_preload = strchr(e, '=') + 1;
            continue;
        }
        if (env_is(e, GECND_SDL2_ENV_SOCKET)) continue;
        env[n++] = strdup(e);
    }

    char shim_dir[PATH_MAX];
    snprintf(shim_dir, sizeof(shim_dir), "%s", s.shim_path);
    char *slash = strrchr(shim_dir, '/');
    if (slash) *slash = '\0';

    env[n++] = env_join("LD_LIBRARY_PATH", shim_dir, old_ld_path);
    if (s.preload) {
        env[n++] = env_join("LD_PRELOAD", s.shim_path, old_preload);
    } else if (old_preload) {
        env[n++] = env_join("LD_PRELOAD", old_preload, NULL);
    }
    env[n++] = env_join(GECND_SDL2_ENV_SOCKET, s.sock_path, NULL);
    env[n]   = NULL;
    return env;
}

static void env_free(char **env) {
    if (!env) return;
    for (size_t i = 0; env[i]; i++) free(env[i]);
    free(env);
}

static bool spawn(void) {
    if (!listen_open()) return false;

    char **env = env_build();
    if (!env) {
        process_set_error("out of memory building environment");
        sockets_close();
        return false;
    }

    bool  executable   = access(s.exec_path, X_OK) == 0;
    char *argv_direct[] = { s.exec_path, NULL };
    char *argv_shell[]  = { "/bin/sh", s.exec_path, NULL };

    pid_t pid = fork();
    if (pid < 0) {
        process_set_error("fork failed: %s", strerror(errno));
        env_free(env);
        sockets_close();
        return false;
    }
    if (pid == 0) {
        setpgid(0, 0);
        if (executable) execve(s.exec_path, argv_direct, env);
        execve("/bin/sh", argv_shell, env);
        _exit(127);
    }

    env_free(env);
    s.pid         = pid;
    s.phase       = PROC_SPAWNED;
    s.term_sent   = false;
    s.deadline_ms = now_ms() + 20000;
    fprintf(stderr, "[sdl2] spawned pid %d: %s\n", (int)pid, s.exec_path);
    return true;
}

/* filho saiu: devolve a tela pro core */
static void core_foreground(void) {
    if (!s.background) return;
    s.background = false;
    api->registry("set", "core:state", (void *)(uintptr_t)GECND_FSM_RUNNING, NULL);
}

static void on_exit_status(int status) {
    bool clean = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    int  code  = WIFEXITED(status) ? WEXITSTATUS(status) : -WTERMSIG(status);

    if (s.phase == PROC_SPAWNED) {
        process_set_error("%s exited before connecting (status %d)", s.exec_path, code);
        s.phase = PROC_FAILED;
    } else if (!clean && s.phase != PROC_STOPPING) {
        process_set_error("%s exited with status %d", s.exec_path, code);
        s.phase = PROC_FAILED;
    } else {
        fprintf(stderr, "[sdl2] pid %d exited (status %d)\n", (int)s.pid, code);
        s.phase = PROC_IDLE;
    }
    s.pid = 0;
    sockets_close();
    core_foreground();
}

/* ── public ──────────────────────────────────────────────────────── */

bool process_request(const char *path, const char *shim_dir) {
    struct stat st;
    s.error[0] = '\0';

    if (!path || stat(path, &st) != 0) {
        process_set_error("not found: %s", path ? path : "(null)");
        return false;
    }
    if (!shim_dir || !shim_dir[0]) {
        process_set_error("could not resolve %s directory", GECND_SDL2_SHIM_NAME);
        return false;
    }
    snprintf(s.shim_path, sizeof(s.shim_path), "%s/%s", shim_dir, GECND_SDL2_SHIM_NAME);
    if (stat(s.shim_path, &st) != 0) {
        process_set_error("shim not found: %s", s.shim_path);
        return false;
    }

    const char *preload = url_env_get("preload");
    s.preload = !(preload && (preload[0] == '0' || preload[0] == 'n' || preload[0] == 'f'));

    snprintf(s.exec_path, sizeof(s.exec_path), "%s", path);
    s.win_w = s.win_h = 0;
    s.phase = PROC_PENDING;
    return true;
}

void process_stop(bool force) {
    if (s.phase == PROC_PENDING || s.phase == PROC_FAILED) {
        s.phase = PROC_IDLE;
    }
    if (s.pid <= 0) {
        sockets_close();
        s.phase = PROC_IDLE;
        return;
    }
    if (force) {
        kill(-s.pid, SIGKILL);
        kill(s.pid, SIGKILL);
        waitpid(s.pid, NULL, 0);
        s.pid = 0;
        sockets_close();
        s.phase = PROC_IDLE;
        core_foreground();
        return;
    }
    if (s.phase != PROC_STOPPING) {
        send_pkt(GECND_SDL2_PKT_QUIT, 0, 0, 0);
        s.phase       = PROC_STOPPING;
        s.term_sent   = false;
        s.deadline_ms = now_ms() + 1500;
    }
}

void process_tick(void) {
    if (s.phase == PROC_PENDING) {
        if (!spawn()) s.phase = PROC_FAILED;
        return;
    }
    if (s.pid <= 0) return;

    int   status = 0;
    pid_t r      = waitpid(s.pid, &status, WNOHANG);
    if (r == s.pid) {
        on_exit_status(status);
        return;
    }

    accept_client();
    read_packets();

    uint64_t now = now_ms();
    if (s.phase == PROC_SPAWNED && now > s.deadline_ms) {
        process_set_error("timeout waiting for %s to connect", GECND_SDL2_SHIM_NAME);
        kill(-s.pid, SIGKILL);
        s.phase = PROC_STOPPING;
        return;
    }
    if (s.phase == PROC_STOPPING && now > s.deadline_ms) {
        if (!s.term_sent) {
            kill(-s.pid, SIGTERM);
            s.term_sent   = true;
            s.deadline_ms = now + 2000;
        } else {
            kill(-s.pid, SIGKILL);
        }
    }
}

bool process_send_key(uint16_t scancode, uint32_t keycode, bool pressed) {
    if (s.phase != PROC_CONNECTED) return false;
    return send_pkt(GECND_SDL2_PKT_KEY, pressed ? 1 : 0, scancode, keycode);
}

bool process_is_running(void) {
    return s.phase == PROC_CONNECTED;
}

gdmsp_fsm_t process_state(void) {
    switch (s.phase) {
        case PROC_PENDING:
        case PROC_SPAWNED:   return GDMSP_FSM_LOADING;
        case PROC_CONNECTED: return GDMSP_FSM_PLAYING;
        case PROC_STOPPING:  return GDMSP_FSM_STOPPING;
        case PROC_FAILED:    return GDMSP_FSM_ERROR;
        default:             return GDMSP_FSM_IDLE;
    }
}

__attribute__((destructor))
static void process_destroy(void) {
    if (s.pid > 0) {
        kill(-s.pid, SIGKILL);
        kill(s.pid, SIGKILL);
    }
    sockets_close();
}
