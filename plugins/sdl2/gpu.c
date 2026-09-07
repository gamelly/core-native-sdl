#include "gpu.h"
#include <string.h>
#include <unistd.h>

int sdlipc_gpu_init(sdlipc_gpu *gpu) {
    gpu->display = eglGetCurrentDisplay();
    if (gpu->display == EGL_NO_DISPLAY) return -1;
    const char *extensions = eglQueryString(gpu->display, EGL_EXTENSIONS);
    if (!extensions || !strstr(extensions, "EGL_ANDROID_native_fence_sync") ||
        !strstr(extensions, "EGL_EXT_image_dma_buf_import_modifiers")) return -1;
#define LOAD(field, name) gpu->field = (typeof(gpu->field))eglGetProcAddress(name); if (!gpu->field) return -1
    LOAD(create_image, "eglCreateImageKHR");
    LOAD(destroy_image, "eglDestroyImageKHR");
    LOAD(export_query, "eglExportDMABUFImageQueryMESA");
    LOAD(export_image, "eglExportDMABUFImageMESA");
    LOAD(create_sync, "eglCreateSyncKHR");
    LOAD(destroy_sync, "eglDestroySyncKHR");
    LOAD(export_fence, "eglDupNativeFenceFDANDROID");
    LOAD(wait_sync, "eglWaitSyncKHR");
    LOAD(bind_image, "glEGLImageTargetTexture2DOES");
#undef LOAD
    return 0;
}

int sdlipc_gpu_fence(sdlipc_gpu *gpu) {
    EGLSyncKHR sync = gpu->create_sync(gpu->display, EGL_SYNC_NATIVE_FENCE_ANDROID, NULL);
    if (sync == EGL_NO_SYNC_KHR) return -1;
    glFlush();
    int fd = gpu->export_fence(gpu->display, sync);
    gpu->destroy_sync(gpu->display, sync);
    return fd;
}

int sdlipc_gpu_wait(sdlipc_gpu *gpu, int fd) {
    const EGLint attributes[] = { EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fd, EGL_NONE };
    EGLSyncKHR sync = gpu->create_sync(gpu->display, EGL_SYNC_NATIVE_FENCE_ANDROID, attributes);
    if (sync == EGL_NO_SYNC_KHR) { close(fd); return -1; }
    EGLBoolean ok = gpu->wait_sync(gpu->display, sync, 0);
    gpu->destroy_sync(gpu->display, sync);
    return ok ? 0 : -1;
}

int sdlipc_gpu_export(sdlipc_gpu *gpu, GLuint texture, sdlipc_message *message, int *fds) {
    const EGLint attributes[] = { EGL_GL_TEXTURE_LEVEL_KHR, 0, EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE };
    EGLImageKHR image = gpu->create_image(gpu->display, eglGetCurrentContext(), EGL_GL_TEXTURE_2D_KHR,
                                         (EGLClientBuffer)(uintptr_t)texture, attributes);
    if (image == EGL_NO_IMAGE_KHR) return -1;
    int format, planes;
    EGLuint64KHR modifier;
    int ok = gpu->export_query(gpu->display, image, &format, &planes, &modifier);
    int strides[SDLIPC_PLANES], offsets[SDLIPC_PLANES];
    if (!ok || planes < 1 || planes > SDLIPC_PLANES) {
        gpu->destroy_image(gpu->display, image);
        return -1;
    }
    ok = gpu->export_image(gpu->display, image, fds, strides, offsets);
    gpu->destroy_image(gpu->display, image);
    if (!ok) return -1;
    message->format = format;
    message->planes = planes;
    message->modifier = modifier;
    for (int i = 0; i < planes; i++) {
        message->stride[i] = strides[i];
        message->offset[i] = offsets[i];
    }
    return 0;
}

EGLImageKHR sdlipc_gpu_import(sdlipc_gpu *gpu, const sdlipc_message *message, const int *fds) {
    static const EGLint fd_keys[] = { EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE1_FD_EXT, EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE3_FD_EXT };
    static const EGLint offset_keys[] = { EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT, EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT };
    static const EGLint pitch_keys[] = { EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT, EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT };
    static const EGLint modifier_keys[] = { EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT };
    EGLint attributes[48] = { EGL_WIDTH, message->width, EGL_HEIGHT, message->height,
                              EGL_LINUX_DRM_FOURCC_EXT, message->format };
    int n = 6;
    for (uint32_t i = 0; i < message->planes; i++) {
        attributes[n++] = fd_keys[i]; attributes[n++] = fds[i];
        attributes[n++] = offset_keys[i]; attributes[n++] = message->offset[i];
        attributes[n++] = pitch_keys[i]; attributes[n++] = message->stride[i];
        attributes[n++] = modifier_keys[i]; attributes[n++] = (uint32_t)message->modifier;
        attributes[n++] = modifier_keys[i] + 1; attributes[n++] = message->modifier >> 32;
    }
    attributes[n] = EGL_NONE;
    return gpu->create_image(gpu->display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attributes);
}
