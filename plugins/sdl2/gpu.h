#ifndef GECND_SDL2_GPU_H
#define GECND_SDL2_GPU_H

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "ipc.h"

typedef struct {
    EGLDisplay display;
    PFNEGLCREATEIMAGEKHRPROC create_image;
    PFNEGLDESTROYIMAGEKHRPROC destroy_image;
    PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC export_query;
    PFNEGLEXPORTDMABUFIMAGEMESAPROC export_image;
    PFNEGLCREATESYNCKHRPROC create_sync;
    PFNEGLDESTROYSYNCKHRPROC destroy_sync;
    PFNEGLDUPNATIVEFENCEFDANDROIDPROC export_fence;
    PFNEGLWAITSYNCKHRPROC wait_sync;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC bind_image;
} sdlipc_gpu;

int sdlipc_gpu_init(sdlipc_gpu *gpu);
int sdlipc_gpu_fence(sdlipc_gpu *gpu);
int sdlipc_gpu_wait(sdlipc_gpu *gpu, int fd);
EGLImageKHR sdlipc_gpu_import(sdlipc_gpu *gpu, const sdlipc_message *message, const int *fds);
int sdlipc_gpu_export(sdlipc_gpu *gpu, GLuint texture, sdlipc_message *message, int *fds);

#endif
