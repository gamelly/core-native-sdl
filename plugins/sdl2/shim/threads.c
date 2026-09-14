#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#include "shim.h"

/* ── mutex ───────────────────────────────────────────────────────── */

struct SDL_mutex {
    pthread_mutex_t handle;
};

SDL_mutex *SDL_CreateMutex(void) {
    SDL_mutex *mutex = calloc(1, sizeof(*mutex));
    if (!mutex) {
        shim_set_error("out of memory");
        return NULL;
    }
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    int rc = pthread_mutex_init(&mutex->handle, &attr);
    pthread_mutexattr_destroy(&attr);
    if (rc != 0) {
        shim_set_error("pthread_mutex_init failed: %s", strerror(rc));
        free(mutex);
        return NULL;
    }
    return mutex;
}

void SDL_DestroyMutex(SDL_mutex *mutex) {
    if (!mutex) return;
    pthread_mutex_destroy(&mutex->handle);
    free(mutex);
}

int SDL_LockMutex(SDL_mutex *mutex) {
    if (!mutex) return SDL_SetError("passed a NULL mutex");
    return pthread_mutex_lock(&mutex->handle) == 0 ? 0 : SDL_SetError("pthread_mutex_lock failed");
}

int SDL_TryLockMutex(SDL_mutex *mutex) {
    if (!mutex) return SDL_SetError("passed a NULL mutex");
    int rc = pthread_mutex_trylock(&mutex->handle);
    if (rc == 0)      return 0;
    if (rc == EBUSY)  return SDL_MUTEX_TIMEDOUT;
    return SDL_SetError("pthread_mutex_trylock failed");
}

int SDL_UnlockMutex(SDL_mutex *mutex) {
    if (!mutex) return SDL_SetError("passed a NULL mutex");
    return pthread_mutex_unlock(&mutex->handle) == 0 ? 0 : SDL_SetError("pthread_mutex_unlock failed");
}

/* ── condition variable ──────────────────────────────────────────── */

struct SDL_cond {
    pthread_cond_t handle;
};

SDL_cond *SDL_CreateCond(void) {
    SDL_cond *cond = calloc(1, sizeof(*cond));
    if (!cond) {
        shim_set_error("out of memory");
        return NULL;
    }
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    int rc = pthread_cond_init(&cond->handle, &attr);
    pthread_condattr_destroy(&attr);
    if (rc != 0) {
        shim_set_error("pthread_cond_init failed: %s", strerror(rc));
        free(cond);
        return NULL;
    }
    return cond;
}

void SDL_DestroyCond(SDL_cond *cond) {
    if (!cond) return;
    pthread_cond_destroy(&cond->handle);
    free(cond);
}

int SDL_CondSignal(SDL_cond *cond) {
    if (!cond) return SDL_SetError("passed a NULL condition variable");
    return pthread_cond_signal(&cond->handle) == 0 ? 0 : SDL_SetError("pthread_cond_signal failed");
}

int SDL_CondBroadcast(SDL_cond *cond) {
    if (!cond) return SDL_SetError("passed a NULL condition variable");
    return pthread_cond_broadcast(&cond->handle) == 0 ? 0 : SDL_SetError("pthread_cond_broadcast failed");
}

int SDL_CondWait(SDL_cond *cond, SDL_mutex *mutex) {
    if (!cond)  return SDL_SetError("passed a NULL condition variable");
    if (!mutex) return SDL_SetError("passed a NULL mutex");
    return pthread_cond_wait(&cond->handle, &mutex->handle) == 0 ? 0 : SDL_SetError("pthread_cond_wait failed");
}

int SDL_CondWaitTimeout(SDL_cond *cond, SDL_mutex *mutex, Uint32 ms) {
    if (!cond)  return SDL_SetError("passed a NULL condition variable");
    if (!mutex) return SDL_SetError("passed a NULL mutex");
    if (ms == SDL_MUTEX_MAXWAIT) return SDL_CondWait(cond, mutex);

    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec  += (time_t)(ms / 1000u);
    deadline.tv_nsec += (long)(ms % 1000u) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec  += 1;
        deadline.tv_nsec -= 1000000000L;
    }

    int rc = pthread_cond_timedwait(&cond->handle, &mutex->handle, &deadline);
    if (rc == 0)         return 0;
    if (rc == ETIMEDOUT) return SDL_MUTEX_TIMEDOUT;
    return SDL_SetError("pthread_cond_timedwait failed");
}

/* ── semaphore ───────────────────────────────────────────────────── */

struct SDL_semaphore {
    sem_t handle;
};

SDL_sem *SDL_CreateSemaphore(Uint32 initial_value) {
    SDL_sem *sem = calloc(1, sizeof(*sem));
    if (!sem) {
        shim_set_error("out of memory");
        return NULL;
    }
    if (sem_init(&sem->handle, 0, initial_value) != 0) {
        shim_set_error("sem_init failed: %s", strerror(errno));
        free(sem);
        return NULL;
    }
    return sem;
}

void SDL_DestroySemaphore(SDL_sem *sem) {
    if (!sem) return;
    sem_destroy(&sem->handle);
    free(sem);
}

int SDL_SemWait(SDL_sem *sem) {
    if (!sem) return SDL_SetError("passed a NULL semaphore");
    while (sem_wait(&sem->handle) != 0) {
        if (errno != EINTR) return SDL_SetError("sem_wait failed");
    }
    return 0;
}

int SDL_SemTryWait(SDL_sem *sem) {
    if (!sem) return SDL_SetError("passed a NULL semaphore");
    if (sem_trywait(&sem->handle) == 0) return 0;
    return errno == EAGAIN ? SDL_MUTEX_TIMEDOUT : SDL_SetError("sem_trywait failed");
}

int SDL_SemWaitTimeout(SDL_sem *sem, Uint32 ms) {
    if (!sem) return SDL_SetError("passed a NULL semaphore");
    if (ms == SDL_MUTEX_MAXWAIT) return SDL_SemWait(sem);

    uint64_t deadline = shim_now_ms() + ms;
    for (;;) {
        if (sem_trywait(&sem->handle) == 0) return 0;
        if (errno != EAGAIN && errno != EINTR) return SDL_SetError("sem_trywait failed");
        if (shim_now_ms() >= deadline) return SDL_MUTEX_TIMEDOUT;
        SDL_Delay(1);
    }
}

int SDL_SemPost(SDL_sem *sem) {
    if (!sem) return SDL_SetError("passed a NULL semaphore");
    return sem_post(&sem->handle) == 0 ? 0 : SDL_SetError("sem_post failed");
}

Uint32 SDL_SemValue(SDL_sem *sem) {
    int value = 0;
    if (!sem || sem_getvalue(&sem->handle, &value) != 0 || value < 0) return 0;
    return (Uint32)value;
}

/* ── thread ──────────────────────────────────────────────────────── */

struct SDL_Thread {
    pthread_t         handle;
    SDL_ThreadFunction fn;
    void              *data;
    int                status;
    bool               detached;
    char               name[64];
};

static void *thread_trampoline(void *arg) {
    SDL_Thread *thread = arg;
    if (thread->name[0]) pthread_setname_np(pthread_self(), thread->name);
    thread->status = thread->fn(thread->data);
    if (thread->detached) free(thread);
    return NULL;
}

SDL_Thread *SDL_CreateThreadWithStackSize(SDL_ThreadFunction fn, const char *name,
                                          const size_t stacksize, void *data) {
    if (!fn) {
        shim_set_error("passed a NULL thread function");
        return NULL;
    }
    SDL_Thread *thread = calloc(1, sizeof(*thread));
    if (!thread) {
        shim_set_error("out of memory");
        return NULL;
    }
    thread->fn   = fn;
    thread->data = data;
    if (name) snprintf(thread->name, sizeof(thread->name), "%s", name);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (stacksize > 0) pthread_attr_setstacksize(&attr, stacksize);
    int rc = pthread_create(&thread->handle, &attr, thread_trampoline, thread);
    pthread_attr_destroy(&attr);

    if (rc != 0) {
        shim_set_error("pthread_create failed: %s", strerror(rc));
        free(thread);
        return NULL;
    }
    return thread;
}

SDL_Thread *SDL_CreateThread(SDL_ThreadFunction fn, const char *name, void *data) {
    return SDL_CreateThreadWithStackSize(fn, name, 0, data);
}

void SDL_WaitThread(SDL_Thread *thread, int *status) {
    if (!thread) return;
    pthread_join(thread->handle, NULL);
    if (status) *status = thread->status;
    free(thread);
}

void SDL_DetachThread(SDL_Thread *thread) {
    if (!thread) return;
    thread->detached = true;
    pthread_detach(thread->handle);
}

const char *SDL_GetThreadName(SDL_Thread *thread) {
    return (thread && thread->name[0]) ? thread->name : NULL;
}

SDL_threadID SDL_GetThreadID(SDL_Thread *thread) {
    return (SDL_threadID)(thread ? thread->handle : pthread_self());
}

SDL_threadID SDL_ThreadID(void) {
    return (SDL_threadID)pthread_self();
}

int SDL_SetThreadPriority(SDL_ThreadPriority priority) {
    (void)priority;
    return 0;
}

/* ── shared object loading ───────────────────────────────────────── */

void *SDL_LoadObject(const char *sofile) {
    if (!sofile) {
        shim_set_error("passed a NULL shared object name");
        return NULL;
    }
    void *handle = dlopen(sofile, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        const char *why = dlerror();
        shim_set_error("dlopen('%s') failed: %s", sofile, why ? why : "unknown error");
    }
    return handle;
}

void *SDL_LoadFunction(void *handle, const char *name) {
    if (!handle || !name) {
        shim_set_error("passed a NULL shared object handle or symbol");
        return NULL;
    }
    dlerror();
    void       *symbol = dlsym(handle, name);
    const char *why    = dlerror();
    if (why) {
        shim_set_error("dlsym('%s') failed: %s", name, why);
        return NULL;
    }
    return symbol;
}

void SDL_UnloadObject(void *handle) {
    if (handle) dlclose(handle);
}
