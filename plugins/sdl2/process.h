#ifndef GECND_SDL2_PROCESS_H
#define GECND_SDL2_PROCESS_H
#include <sys/types.h>
int sdlipc_spawn(const char *binary, const char *library_dir, pid_t *pid);
#endif
