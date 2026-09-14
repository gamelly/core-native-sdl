#define _GNU_SOURCE
#include <dlfcn.h>

#include "shim.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

/* ── x11 (dlopen) ────────────────────────────────────────────────── */

typedef void         *XDisplay;
typedef unsigned long XWindow;
typedef unsigned long XAtom;
typedef unsigned long XVisualID;
typedef unsigned long XKeySym;
typedef unsigned long XCursor;
typedef unsigned long XPixmap;

typedef struct {
    unsigned long  pixel;
    unsigned short red, green, blue;
    char           flags;
    char           pad;
} XColorRaw;

typedef union {
    int  type;
    long pad[24];
} XEventRaw;

typedef struct {
    int type; unsigned long serial; int send_event; XDisplay display;
    XWindow event; XWindow window;
    int x, y; int width, height; int border_width;
    XWindow above; int override_redirect;
} XConfigureEventRaw;

typedef struct {
    int type; unsigned long serial; int send_event; XDisplay display;
    XWindow window; XAtom message_type; int format;
    union { char b[20]; short s[10]; long l[5]; } data;
} XClientMessageEventRaw;

typedef struct {
    int type; unsigned long serial; int send_event; XDisplay display;
    XWindow window; XWindow root; XWindow subwindow; unsigned long time;
    int x, y; int x_root, y_root; unsigned int state; unsigned int keycode; int same_screen;
} XKeyEventRaw;

typedef struct {
    long flags; int input; int initial_state;
    unsigned long icon_pixmap; XWindow icon_window; int icon_x, icon_y;
    unsigned long icon_mask; unsigned long window_group;
} XWMHintsRaw;

#define X_KeyPress          2
#define X_KeyRelease        3
#define X_FocusIn           9
#define X_FocusOut          10
#define X_Expose            12
#define X_UnmapNotify       18
#define X_MapNotify         19
#define X_ConfigureNotify   22
#define X_ClientMessage     33

#define X_KeyPressMask            (1L << 0)
#define X_KeyReleaseMask          (1L << 1)
#define X_ExposureMask            (1L << 15)
#define X_StructureNotifyMask     (1L << 17)
#define X_SubstructureNotifyMask  (1L << 19)
#define X_SubstructureRedirectMask (1L << 20)
#define X_FocusChangeMask         (1L << 21)

#define X_InputHint        (1L << 0)
#define X_StateHint        (1L << 1)
#define X_NormalState      1
#define X_PropModeReplace  0
#define X_XA_ATOM          4UL
#define X_NET_WM_STATE_REMOVE 0
#define X_None             0UL
#define X_CurrentTime      0UL
#define X_GrabModeAsync    1
#define X_GrabSuccess      0
#define X_PointerMotionMask (1L << 6)
#define X_ButtonPressMask   (1L << 2)
#define X_ButtonReleaseMask (1L << 3)
#define X_NET_WM_STATE_ADD    1

#define X_FOREACH(X) \
    X(XDisplay,      XOpenDisplay,        (const char *)) \
    X(int,           XCloseDisplay,       (XDisplay)) \
    X(int,           XDefaultScreen,      (XDisplay)) \
    X(XWindow,       XDefaultRootWindow,  (XDisplay)) \
    X(void *,        XDefaultVisual,      (XDisplay, int)) \
    X(XVisualID,     XVisualIDFromVisual, (void *)) \
    X(unsigned long, XBlackPixel,         (XDisplay, int)) \
    X(int,           XDisplayWidth,       (XDisplay, int)) \
    X(int,           XDisplayHeight,      (XDisplay, int)) \
    X(int,           XDisplayWidthMM,     (XDisplay, int)) \
    X(XWindow,       XCreateSimpleWindow, (XDisplay, XWindow, int, int, unsigned, unsigned, unsigned, unsigned long, unsigned long)) \
    X(int,           XDestroyWindow,      (XDisplay, XWindow)) \
    X(int,           XMapRaised,          (XDisplay, XWindow)) \
    X(int,           XUnmapWindow,        (XDisplay, XWindow)) \
    X(int,           XRaiseWindow,        (XDisplay, XWindow)) \
    X(int,           XStoreName,          (XDisplay, XWindow, const char *)) \
    X(int,           XSelectInput,        (XDisplay, XWindow, long)) \
    X(int,           XFlush,              (XDisplay)) \
    X(int,           XSync,               (XDisplay, int)) \
    X(int,           XPending,            (XDisplay)) \
    X(int,           XNextEvent,          (XDisplay, XEventRaw *)) \
    X(XAtom,         XInternAtom,         (XDisplay, const char *, int)) \
    X(int,           XSetWMProtocols,     (XDisplay, XWindow, XAtom *, int)) \
    X(int,           XSetWMHints,         (XDisplay, XWindow, XWMHintsRaw *)) \
    X(int,           XResizeWindow,       (XDisplay, XWindow, unsigned, unsigned)) \
    X(int,           XMoveWindow,         (XDisplay, XWindow, int, int)) \
    X(int,           XSendEvent,          (XDisplay, XWindow, int, long, XEventRaw *)) \
    X(int,           XChangeProperty,     (XDisplay, XWindow, XAtom, XAtom, int, int, const unsigned char *, int)) \
    X(XKeySym,       XLookupKeysym,       (XKeyEventRaw *, int)) \
    X(XCursor,       XCreateFontCursor,   (XDisplay, unsigned)) \
    X(XCursor,       XCreatePixmapCursor, (XDisplay, XPixmap, XPixmap, XColorRaw *, XColorRaw *, unsigned, unsigned)) \
    X(XPixmap,       XCreateBitmapFromData, (XDisplay, XWindow, const char *, unsigned, unsigned)) \
    X(int,           XFreePixmap,         (XDisplay, XPixmap)) \
    X(int,           XDefineCursor,       (XDisplay, XWindow, XCursor)) \
    X(int,           XUndefineCursor,     (XDisplay, XWindow)) \
    X(int,           XFreeCursor,         (XDisplay, XCursor)) \
    X(int,           XGrabPointer,        (XDisplay, XWindow, int, unsigned, int, int, XWindow, XCursor, unsigned long)) \
    X(int,           XUngrabPointer,      (XDisplay, unsigned long))

#define X_DECL(ret, name, args) static ret (*p_##name) args;
X_FOREACH(X_DECL)
#undef X_DECL

/* ── egl (dlopen) ────────────────────────────────────────────────── */

typedef EGLDisplay (*PFN_eglGetPlatformDisplay_t)(EGLenum, void *, const EGLAttrib *);
typedef EGLDisplay (*PFN_eglGetPlatformDisplayEXT_t)(EGLenum, void *, const EGLint *);

#define EGL_FOREACH(X) \
    X(EGLDisplay,   eglGetDisplay,          (EGLNativeDisplayType)) \
    X(EGLBoolean,   eglInitialize,          (EGLDisplay, EGLint *, EGLint *)) \
    X(EGLBoolean,   eglTerminate,           (EGLDisplay)) \
    X(EGLBoolean,   eglChooseConfig,        (EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *)) \
    X(EGLBoolean,   eglGetConfigAttrib,     (EGLDisplay, EGLConfig, EGLint, EGLint *)) \
    X(EGLBoolean,   eglBindAPI,             (EGLenum)) \
    X(EGLSurface,   eglCreateWindowSurface, (EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint *)) \
    X(EGLBoolean,   eglDestroySurface,      (EGLDisplay, EGLSurface)) \
    X(EGLContext,   eglCreateContext,       (EGLDisplay, EGLConfig, EGLContext, const EGLint *)) \
    X(EGLBoolean,   eglDestroyContext,      (EGLDisplay, EGLContext)) \
    X(EGLBoolean,   eglMakeCurrent,         (EGLDisplay, EGLSurface, EGLSurface, EGLContext)) \
    X(EGLBoolean,   eglSwapBuffers,         (EGLDisplay, EGLSurface)) \
    X(EGLBoolean,   eglSwapInterval,        (EGLDisplay, EGLint)) \
    X(EGLint,       eglGetError,            (void)) \
    X(const char *, eglQueryString,         (EGLDisplay, EGLint)) \
    X(EGLBoolean,   eglQuerySurface,        (EGLDisplay, EGLSurface, EGLint, EGLint *)) \
    X(void *,       eglGetProcAddress,      (const char *))

#define EGL_DECL(ret, name, args) static ret (*p_##name) args;
EGL_FOREACH(EGL_DECL)
#undef EGL_DECL

/* ── state ───────────────────────────────────────────────────────── */

#define GL_ATTR_COUNT 32

struct SDL_Cursor {
    XCursor xcursor;
    bool    owned;
};

struct SDL_Window {
    Uint32     id;
    Uint32     flags;
    int        x, y, w, h;
    int        min_w, min_h;
    char       title[256];
    XWindow    xwin;
    EGLConfig  config;
    EGLSurface surface;
    bool       mapped;
};

static struct {
    bool        ready;
    bool        failed;
    bool        has_x;
    bool        x11_keys;
    bool        create_context_ext;
    bool        attrs_set;
    bool        sync_after_swap;
    void      (*gl_finish)(void);
    bool        synthetic_focus;
    bool        cursor_shown;
    bool        grabbed;
    void       *lib_xcursor;
    XCursor     invisible;
    SDL_Cursor *cursor;
    SDL_Cursor  default_cursor;
    void       *lib_x11;
    void       *lib_egl;
    void       *lib_gl;
    XDisplay    dpy;
    int         screen;
    XAtom       wm_delete;
    XAtom       net_wm_state;
    XAtom       net_wm_fullscreen;
    EGLDisplay  edpy;
    EGLContext  current;
    SDL_Window *window;
    Uint32      next_id;
    int         swap_interval;
    int         attrs[GL_ATTR_COUNT];
} v = { .next_id = 1, .swap_interval = 1, .cursor_shown = true };

static bool video_init(void);
static void cursor_apply(void);

static void attrs_reset(void) {
    memset(v.attrs, 0, sizeof(v.attrs));
    v.attrs[SDL_GL_RED_SIZE]              = 8;
    v.attrs[SDL_GL_GREEN_SIZE]            = 8;
    v.attrs[SDL_GL_BLUE_SIZE]             = 8;
    v.attrs[SDL_GL_ALPHA_SIZE]            = 0;
    v.attrs[SDL_GL_DEPTH_SIZE]            = 16;
    v.attrs[SDL_GL_STENCIL_SIZE]          = 0;
    v.attrs[SDL_GL_DOUBLEBUFFER]          = 1;
    v.attrs[SDL_GL_CONTEXT_MAJOR_VERSION] = 2;
    v.attrs[SDL_GL_CONTEXT_MINOR_VERSION] = 1;
    v.attrs[SDL_GL_CONTEXT_PROFILE_MASK]  = 0;
}

static bool x11_load(void) {
    v.lib_x11 = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
    if (!v.lib_x11) return false;
#define X_LOAD(ret, name, args) \
    p_##name = (ret (*) args)dlsym(v.lib_x11, #name); \
    if (!p_##name) { shim_set_error("libX11 missing %s", #name); return false; }
    X_FOREACH(X_LOAD)
#undef X_LOAD
    return true;
}

static bool egl_load(void) {
    v.lib_egl = dlopen("libEGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
    if (!v.lib_egl) v.lib_egl = dlopen("libEGL.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!v.lib_egl) {
        shim_set_error("libEGL not available: %s", dlerror());
        return false;
    }
#define EGL_LOAD(ret, name, args) \
    p_##name = (ret (*) args)dlsym(v.lib_egl, #name); \
    if (!p_##name) { shim_set_error("libEGL missing %s", #name); return false; }
    EGL_FOREACH(EGL_LOAD)
#undef EGL_LOAD
    return true;
}

static EGLDisplay egl_display_open(void) {
    if (v.has_x) {
        PFN_eglGetPlatformDisplay_t get = (PFN_eglGetPlatformDisplay_t)dlsym(v.lib_egl, "eglGetPlatformDisplay");
        if (get) {
            EGLDisplay d = get(EGL_PLATFORM_X11_KHR, v.dpy, NULL);
            if (d != EGL_NO_DISPLAY) return d;
        }
        PFN_eglGetPlatformDisplayEXT_t get_ext = (PFN_eglGetPlatformDisplayEXT_t)p_eglGetProcAddress("eglGetPlatformDisplayEXT");
        if (get_ext) {
            EGLDisplay d = get_ext(EGL_PLATFORM_X11_KHR, v.dpy, NULL);
            if (d != EGL_NO_DISPLAY) return d;
        }
        return p_eglGetDisplay((EGLNativeDisplayType)v.dpy);
    }
    return p_eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

static bool video_init(void) {
    if (v.ready) return true;
    if (v.failed) return false;

    if (!v.attrs_set) {
        attrs_reset();
        v.attrs_set = true;
    }

    v.sync_after_swap = getenv(GECND_SDL2_ENV_SYNC) != NULL;

    const char *native = getenv(GECND_SDL2_ENV_NATIVE);
    const char *keys   = getenv(GECND_SDL2_ENV_X11KEYS);
    bool want_x = native ? (strcmp(native, "x11") == 0) : (getenv("DISPLAY") != NULL);
    v.x11_keys = (keys && keys[0] == '1') || getenv(GECND_SDL2_ENV_SOCKET) == NULL;
    v.synthetic_focus = !v.x11_keys;

    if (want_x && x11_load()) {
        v.dpy = p_XOpenDisplay(NULL);
        if (v.dpy) {
            v.has_x             = true;
            v.screen            = p_XDefaultScreen(v.dpy);
            v.wm_delete         = p_XInternAtom(v.dpy, "WM_DELETE_WINDOW", 0);
            v.net_wm_state      = p_XInternAtom(v.dpy, "_NET_WM_STATE", 0);
            v.net_wm_fullscreen = p_XInternAtom(v.dpy, "_NET_WM_STATE_FULLSCREEN", 0);
        }
    }

    if (!egl_load()) {
        v.failed = true;
        return false;
    }

    v.edpy = egl_display_open();
    if (v.edpy == EGL_NO_DISPLAY) {
        shim_set_error("eglGetDisplay failed (0x%x)", p_eglGetError());
        v.failed = true;
        return false;
    }
    EGLint major = 0, minor = 0;
    if (!p_eglInitialize(v.edpy, &major, &minor)) {
        shim_set_error("eglInitialize failed (0x%x)", p_eglGetError());
        v.failed = true;
        return false;
    }

    const char *exts = p_eglQueryString(v.edpy, EGL_EXTENSIONS);
    v.create_context_ext = (major > 1 || (major == 1 && minor >= 5))
                        || (exts && strstr(exts, "EGL_KHR_create_context"));

    fprintf(stderr, "[libSDL2-shim] video ready: %s, EGL %d.%d, %s\n",
            v.has_x ? "x11" : "native", major, minor,
            p_eglQueryString(v.edpy, EGL_VENDOR));
    v.ready = true;
    return true;
}

static void desktop_size(int *w, int *h) {
    if (v.has_x) {
        *w = p_XDisplayWidth(v.dpy, v.screen);
        *h = p_XDisplayHeight(v.dpy, v.screen);
        return;
    }
    const char *ew = getenv("GECND_SDL2_WIDTH");
    const char *eh = getenv("GECND_SDL2_HEIGHT");
    *w = ew ? atoi(ew) : 1920;
    *h = eh ? atoi(eh) : 1080;
    if (*w <= 0) *w = 1920;
    if (*h <= 0) *h = 1080;
}

uint32_t shim_window_id(void) {
    return v.window ? v.window->id : 0;
}

/* ── x11 events ──────────────────────────────────────────────────── */

static bool x11_keysym_to_sdl(XKeySym ks, uint16_t *scancode, uint32_t *keycode) {
    if (ks >= 'a' && ks <= 'z') { *scancode = (uint16_t)(SDL_SCANCODE_A + (ks - 'a')); *keycode = (uint32_t)ks; return true; }
    if (ks >= 'A' && ks <= 'Z') { *scancode = (uint16_t)(SDL_SCANCODE_A + (ks - 'A')); *keycode = (uint32_t)(ks + 32); return true; }
    if (ks >= '1' && ks <= '9') { *scancode = (uint16_t)(SDL_SCANCODE_1 + (ks - '1')); *keycode = (uint32_t)ks; return true; }
    if (ks == '0')              { *scancode = SDL_SCANCODE_0; *keycode = '0'; return true; }
    if (ks >= 0xffbe && ks <= 0xffc9) {
        *scancode = (uint16_t)(SDL_SCANCODE_F1 + (ks - 0xffbe));
        *keycode  = (uint32_t)(SDLK_F1 + (ks - 0xffbe));
        return true;
    }
    switch (ks) {
        case 0x0020: *scancode = SDL_SCANCODE_SPACE;     *keycode = SDLK_SPACE;     return true;
        case 0xff0d: *scancode = SDL_SCANCODE_RETURN;    *keycode = SDLK_RETURN;    return true;
        case 0xff1b: *scancode = SDL_SCANCODE_ESCAPE;    *keycode = SDLK_ESCAPE;    return true;
        case 0xff08: *scancode = SDL_SCANCODE_BACKSPACE; *keycode = SDLK_BACKSPACE; return true;
        case 0xff09: *scancode = SDL_SCANCODE_TAB;       *keycode = SDLK_TAB;       return true;
        case 0xff52: *scancode = SDL_SCANCODE_UP;        *keycode = SDLK_UP;        return true;
        case 0xff54: *scancode = SDL_SCANCODE_DOWN;      *keycode = SDLK_DOWN;      return true;
        case 0xff51: *scancode = SDL_SCANCODE_LEFT;      *keycode = SDLK_LEFT;      return true;
        case 0xff53: *scancode = SDL_SCANCODE_RIGHT;     *keycode = SDLK_RIGHT;     return true;
        case 0xff50: *scancode = SDL_SCANCODE_HOME;      *keycode = SDLK_HOME;      return true;
        case 0xff57: *scancode = SDL_SCANCODE_END;       *keycode = SDLK_END;       return true;
        case 0xff55: *scancode = SDL_SCANCODE_PAGEUP;    *keycode = SDLK_PAGEUP;    return true;
        case 0xff56: *scancode = SDL_SCANCODE_PAGEDOWN;  *keycode = SDLK_PAGEDOWN;  return true;
        case 0xff63: *scancode = SDL_SCANCODE_INSERT;    *keycode = SDLK_INSERT;    return true;
        case 0xffff: *scancode = SDL_SCANCODE_DELETE;    *keycode = SDLK_DELETE;    return true;
        case 0xffe1: *scancode = SDL_SCANCODE_LSHIFT;    *keycode = SDLK_LSHIFT;    return true;
        case 0xffe2: *scancode = SDL_SCANCODE_RSHIFT;    *keycode = SDLK_RSHIFT;    return true;
        case 0xffe3: *scancode = SDL_SCANCODE_LCTRL;     *keycode = SDLK_LCTRL;     return true;
        case 0xffe4: *scancode = SDL_SCANCODE_RCTRL;     *keycode = SDLK_RCTRL;     return true;
        case 0xffe9: *scancode = SDL_SCANCODE_LALT;      *keycode = SDLK_LALT;      return true;
        case 0xffea: *scancode = SDL_SCANCODE_RALT;      *keycode = SDLK_RALT;      return true;
        default:     return false;
    }
}

void shim_video_pump(void) {
    if (!v.has_x || !v.window) return;
    SDL_Window *win = v.window;

    while (p_XPending(v.dpy) > 0) {
        XEventRaw ev;
        p_XNextEvent(v.dpy, &ev);
        switch (ev.type) {
            case X_ClientMessage: {
                XClientMessageEventRaw *cm = (XClientMessageEventRaw *)&ev;
                if ((XAtom)cm->data.l[0] == v.wm_delete) {
                    shim_events_window(SDL_WINDOWEVENT_CLOSE, 0, 0);
                    SDL_Event q;
                    memset(&q, 0, sizeof(q));
                    q.type = SDL_QUIT;
                    q.quit.timestamp = SDL_GetTicks();
                    shim_events_push(&q);
                }
                break;
            }
            case X_ConfigureNotify: {
                XConfigureEventRaw *ce = (XConfigureEventRaw *)&ev;
                if (ce->width != win->w || ce->height != win->h) {
                    win->w = ce->width;
                    win->h = ce->height;
                    shim_events_window(SDL_WINDOWEVENT_SIZE_CHANGED, win->w, win->h);
                    shim_events_window(SDL_WINDOWEVENT_RESIZED, win->w, win->h);
                    shim_ipc_send(GECND_SDL2_PKT_WINDOW, 0, (uint16_t)win->w, (uint32_t)win->h);
                }
                if (!ce->send_event && (ce->x != win->x || ce->y != win->y)) {
                    win->x = ce->x;
                    win->y = ce->y;
                    shim_events_window(SDL_WINDOWEVENT_MOVED, win->x, win->y);
                }
                break;
            }
            case X_Expose:
                shim_events_window(SDL_WINDOWEVENT_EXPOSED, 0, 0);
                break;
            case X_FocusIn:
                if (v.synthetic_focus) break;
                win->flags |= SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS;
                shim_events_window(SDL_WINDOWEVENT_FOCUS_GAINED, 0, 0);
                break;
            case X_FocusOut:
                if (v.synthetic_focus) break;
                win->flags &= ~(Uint32)(SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);
                shim_events_window(SDL_WINDOWEVENT_FOCUS_LOST, 0, 0);
                break;
            case X_MapNotify:
                win->mapped = true;
                win->flags |= SDL_WINDOW_SHOWN;
                shim_events_window(SDL_WINDOWEVENT_SHOWN, 0, 0);
                break;
            case X_UnmapNotify:
                win->mapped = false;
                shim_events_window(SDL_WINDOWEVENT_HIDDEN, 0, 0);
                break;
            case X_KeyPress:
            case X_KeyRelease: {
                if (!v.x11_keys) break;
                XKeyEventRaw *ke = (XKeyEventRaw *)&ev;
                uint16_t scancode;
                uint32_t keycode;
                if (x11_keysym_to_sdl(p_XLookupKeysym(ke, 0), &scancode, &keycode)) {
                    shim_events_key(scancode, keycode, ev.type == X_KeyPress);
                }
                break;
            }
            default:
                break;
        }
    }
}

/* ── gl attributes ───────────────────────────────────────────────── */

int SDL_GL_SetAttribute(SDL_GLattr attr, int value) {
    if (!v.attrs_set) {
        attrs_reset();
        v.attrs_set = true;
    }
    if ((int)attr < 0 || (int)attr >= GL_ATTR_COUNT) {
        shim_set_error("unknown GL attribute %d", (int)attr);
        return -1;
    }
    v.attrs[attr] = value;
    return 0;
}

int SDL_GL_GetAttribute(SDL_GLattr attr, int *value) {
    if ((int)attr < 0 || (int)attr >= GL_ATTR_COUNT || !value) return -1;
    *value = v.attrs[attr];
    if (v.window && v.window->config) {
        EGLint q = 0;
        switch (attr) {
            case SDL_GL_RED_SIZE:     p_eglGetConfigAttrib(v.edpy, v.window->config, EGL_RED_SIZE,     &q); *value = q; break;
            case SDL_GL_GREEN_SIZE:   p_eglGetConfigAttrib(v.edpy, v.window->config, EGL_GREEN_SIZE,   &q); *value = q; break;
            case SDL_GL_BLUE_SIZE:    p_eglGetConfigAttrib(v.edpy, v.window->config, EGL_BLUE_SIZE,    &q); *value = q; break;
            case SDL_GL_ALPHA_SIZE:   p_eglGetConfigAttrib(v.edpy, v.window->config, EGL_ALPHA_SIZE,   &q); *value = q; break;
            case SDL_GL_DEPTH_SIZE:   p_eglGetConfigAttrib(v.edpy, v.window->config, EGL_DEPTH_SIZE,   &q); *value = q; break;
            case SDL_GL_STENCIL_SIZE: p_eglGetConfigAttrib(v.edpy, v.window->config, EGL_STENCIL_SIZE, &q); *value = q; break;
            default: break;
        }
    }
    return 0;
}

void SDL_GL_ResetAttributes(void) {
    attrs_reset();
    v.attrs_set = true;
}

/* ── surface / context ───────────────────────────────────────────── */

static bool is_es(void) {
    return (v.attrs[SDL_GL_CONTEXT_PROFILE_MASK] & SDL_GL_CONTEXT_PROFILE_ES) != 0;
}

static EGLConfig config_choose(bool strict) {
    EGLint attribs[32];
    int    n = 0;

    attribs[n++] = EGL_RENDERABLE_TYPE;
    attribs[n++] = is_es() ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT;
    attribs[n++] = EGL_SURFACE_TYPE;
    attribs[n++] = EGL_WINDOW_BIT;
    attribs[n++] = EGL_RED_SIZE;
    attribs[n++] = v.attrs[SDL_GL_RED_SIZE];
    attribs[n++] = EGL_GREEN_SIZE;
    attribs[n++] = v.attrs[SDL_GL_GREEN_SIZE];
    attribs[n++] = EGL_BLUE_SIZE;
    attribs[n++] = v.attrs[SDL_GL_BLUE_SIZE];
    if (strict) {
        attribs[n++] = EGL_ALPHA_SIZE;
        attribs[n++] = v.attrs[SDL_GL_ALPHA_SIZE];
        attribs[n++] = EGL_DEPTH_SIZE;
        attribs[n++] = v.attrs[SDL_GL_DEPTH_SIZE];
        attribs[n++] = EGL_STENCIL_SIZE;
        attribs[n++] = v.attrs[SDL_GL_STENCIL_SIZE];
        if (v.attrs[SDL_GL_MULTISAMPLEBUFFERS]) {
            attribs[n++] = EGL_SAMPLE_BUFFERS;
            attribs[n++] = 1;
            attribs[n++] = EGL_SAMPLES;
            attribs[n++] = v.attrs[SDL_GL_MULTISAMPLESAMPLES];
        }
    } else {
        attribs[n++] = EGL_DEPTH_SIZE;
        attribs[n++] = 1;
    }
    attribs[n++] = EGL_NONE;

    EGLConfig configs[64];
    EGLint    count = 0;
    if (!p_eglChooseConfig(v.edpy, attribs, configs, 64, &count) || count < 1) return NULL;

    if (v.has_x) {
        XVisualID want = p_XVisualIDFromVisual(p_XDefaultVisual(v.dpy, v.screen));
        for (EGLint i = 0; i < count; i++) {
            EGLint vid = 0;
            if (p_eglGetConfigAttrib(v.edpy, configs[i], EGL_NATIVE_VISUAL_ID, &vid) && (XVisualID)vid == want) {
                return configs[i];
            }
        }
    }
    return configs[0];
}

static bool surface_create(SDL_Window *win) {
    win->config = config_choose(true);
    if (!win->config) win->config = config_choose(false);
    if (!win->config) {
        shim_set_error("no EGL config matches requested attributes");
        return false;
    }
    if (!p_eglBindAPI(is_es() ? EGL_OPENGL_ES_API : EGL_OPENGL_API)) {
        shim_set_error("eglBindAPI failed (0x%x)", p_eglGetError());
        return false;
    }
    EGLNativeWindowType native = v.has_x ? (EGLNativeWindowType)win->xwin : (EGLNativeWindowType)0;
    win->surface = p_eglCreateWindowSurface(v.edpy, win->config, native, NULL);
    if (win->surface == EGL_NO_SURFACE) {
        shim_set_error("eglCreateWindowSurface failed (0x%x)", p_eglGetError());
        return false;
    }
    return true;
}

static void surface_destroy(SDL_Window *win) {
    if (win->surface == EGL_NO_SURFACE || !win->surface) return;
    p_eglMakeCurrent(v.edpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    p_eglDestroySurface(v.edpy, win->surface);
    win->surface = EGL_NO_SURFACE;
}

SDL_GLContext SDL_GL_CreateContext(SDL_Window *window) {
    if (!window || !window->surface) {
        shim_set_error("window has no OpenGL surface");
        return NULL;
    }

    EGLint attribs[16];
    int    n     = 0;
    int    major = v.attrs[SDL_GL_CONTEXT_MAJOR_VERSION];
    int    minor = v.attrs[SDL_GL_CONTEXT_MINOR_VERSION];
    int    flags = v.attrs[SDL_GL_CONTEXT_FLAGS];

    if (is_es()) {
        attribs[n++] = EGL_CONTEXT_CLIENT_VERSION;
        attribs[n++] = major > 0 ? major : 2;
    } else if (v.create_context_ext) {
        attribs[n++] = EGL_CONTEXT_MAJOR_VERSION;
        attribs[n++] = major;
        attribs[n++] = EGL_CONTEXT_MINOR_VERSION;
        attribs[n++] = minor;
        if (major >= 3) {
            attribs[n++] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
            attribs[n++] = (v.attrs[SDL_GL_CONTEXT_PROFILE_MASK] & SDL_GL_CONTEXT_PROFILE_CORE)
                         ? EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT
                         : EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT;
        }
        if (flags & SDL_GL_CONTEXT_DEBUG_FLAG) {
            attribs[n++] = EGL_CONTEXT_OPENGL_DEBUG;
            attribs[n++] = EGL_TRUE;
        }
        if (flags & SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG) {
            attribs[n++] = EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE;
            attribs[n++] = EGL_TRUE;
        }
    }
    attribs[n++] = EGL_NONE;

    EGLContext share = v.attrs[SDL_GL_SHARE_WITH_CURRENT_CONTEXT] && v.current ? v.current : EGL_NO_CONTEXT;
    EGLContext ctx   = p_eglCreateContext(v.edpy, window->config, share, attribs);
    if (ctx == EGL_NO_CONTEXT) {
        shim_set_error("eglCreateContext %d.%d failed (0x%x)", major, minor, p_eglGetError());
        return NULL;
    }
    if (!p_eglMakeCurrent(v.edpy, window->surface, window->surface, ctx)) {
        shim_set_error("eglMakeCurrent failed (0x%x)", p_eglGetError());
        p_eglDestroyContext(v.edpy, ctx);
        return NULL;
    }
    v.current = ctx;

    if (!v.lib_gl) {
        v.lib_gl = dlopen(is_es() ? "libGLESv2.so.2" : "libGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
    }
    if (v.sync_after_swap && !v.gl_finish) {
        v.gl_finish = (void (*)(void))SDL_GL_GetProcAddress("glFinish");
        fprintf(stderr, "[libSDL2-shim] sync after swap %s\n", v.gl_finish ? "on" : "unavailable");
    }
    fprintf(stderr, "[libSDL2-shim] GL context %d.%d created (%s)\n", major, minor, is_es() ? "GLES" : "GL");
    return (SDL_GLContext)ctx;
}

int SDL_GL_MakeCurrent(SDL_Window *window, SDL_GLContext context) {
    if (!v.ready) return -1;
    EGLSurface surf = (window && window->surface) ? window->surface : EGL_NO_SURFACE;
    if (!p_eglMakeCurrent(v.edpy, surf, surf, (EGLContext)context)) {
        shim_set_error("eglMakeCurrent failed (0x%x)", p_eglGetError());
        return -1;
    }
    v.current = (EGLContext)context;
    return 0;
}

void SDL_GL_DeleteContext(SDL_GLContext context) {
    if (!v.ready || !context) return;
    if (v.current == (EGLContext)context) {
        p_eglMakeCurrent(v.edpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        v.current = EGL_NO_CONTEXT;
    }
    p_eglDestroyContext(v.edpy, (EGLContext)context);
}

SDL_GLContext SDL_GL_GetCurrentContext(void) {
    return (SDL_GLContext)v.current;
}

SDL_Window *SDL_GL_GetCurrentWindow(void) {
    return v.window;
}

void SDL_GL_SwapWindow(SDL_Window *window) {
    if (!window || !window->surface) return;
    p_eglSwapBuffers(v.edpy, window->surface);
    /* Drivers that ignore eglSwapInterval let the client queue frames ahead of
     * the GPU, and every queued frame pins its command and vertex buffers. On a
     * device where those come out of a small CMA pool that is fatal, so this
     * makes the swap wait. */
    if (v.sync_after_swap && v.gl_finish) v.gl_finish();
}

int SDL_GL_SetSwapInterval(int interval) {
    if (!v.ready) return -1;
    if (interval < 0) interval = 1;
    if (!p_eglSwapInterval(v.edpy, interval)) {
        fprintf(stderr, "[libSDL2-shim] eglSwapInterval(%d) failed (0x%x); frames may queue up\n",
                interval, p_eglGetError());
        shim_set_error("eglSwapInterval failed (0x%x)", p_eglGetError());
        return -1;
    }
    fprintf(stderr, "[libSDL2-shim] swap interval %d\n", interval);
    v.swap_interval = interval;
    return 0;
}

int SDL_GL_GetSwapInterval(void) {
    return v.swap_interval;
}

void *SDL_GL_GetProcAddress(const char *proc) {
    if (!proc) return NULL;
    void *p = NULL;
    if (p_eglGetProcAddress) p = p_eglGetProcAddress(proc);
    /* eglGetProcAddress is only required to resolve extensions, so core GLES
     * entry points come from whatever is already mapped into the process. */
    if (!p) p = dlsym(RTLD_DEFAULT, proc);
    if (!p) {
        static const char *const libs[] = {
            "libGLESv2.so.2", "libGLESv2.so", "libGL.so.1", "libGL.so",
        };
        for (size_t i = 0; !p && i < sizeof(libs) / sizeof(*libs); i++) {
            if (!v.lib_gl) v.lib_gl = dlopen(libs[i], RTLD_LAZY | RTLD_GLOBAL);
            if (v.lib_gl) p = dlsym(v.lib_gl, proc);
            if (!p && v.lib_gl) {
                dlclose(v.lib_gl);
                v.lib_gl = NULL;
            }
        }
    }
    return p;
}

int SDL_GL_LoadLibrary(const char *path) {
    (void)path;
    return video_init() ? 0 : -1;
}

void SDL_GL_UnloadLibrary(void) {
}

SDL_bool SDL_GL_ExtensionSupported(const char *extension) {
    (void)extension;
    return SDL_FALSE;
}

void SDL_GL_GetDrawableSize(SDL_Window *window, int *w, int *h) {
    if (!window) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    EGLint sw = window->w, sh = window->h;
    if (window->surface) {
        p_eglQuerySurface(v.edpy, window->surface, EGL_WIDTH,  &sw);
        p_eglQuerySurface(v.edpy, window->surface, EGL_HEIGHT, &sh);
    }
    if (w) *w = sw;
    if (h) *h = sh;
}

/* ── window ──────────────────────────────────────────────────────── */

static void x11_fullscreen(SDL_Window *win, bool enable) {
    if (!v.has_x || !win->xwin) return;
    if (!win->mapped) {
        if (enable) {
            p_XChangeProperty(v.dpy, win->xwin, v.net_wm_state, X_XA_ATOM, 32, X_PropModeReplace,
                              (const unsigned char *)&v.net_wm_fullscreen, 1);
        }
        return;
    }
    XEventRaw ev;
    memset(&ev, 0, sizeof(ev));
    XClientMessageEventRaw *cm = (XClientMessageEventRaw *)&ev;
    cm->type         = X_ClientMessage;
    cm->display      = v.dpy;
    cm->window       = win->xwin;
    cm->message_type = v.net_wm_state;
    cm->format       = 32;
    cm->data.l[0]    = enable ? X_NET_WM_STATE_ADD : X_NET_WM_STATE_REMOVE;
    cm->data.l[1]    = (long)v.net_wm_fullscreen;
    cm->data.l[2]    = 0;
    cm->data.l[3]    = 1;
    p_XSendEvent(v.dpy, p_XDefaultRootWindow(v.dpy), 0,
                 X_SubstructureRedirectMask | X_SubstructureNotifyMask, &ev);
    p_XFlush(v.dpy);
}

SDL_Window *SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags) {
    if (!video_init()) return NULL;
    if (v.window) {
        shim_set_error("only one window is supported");
        return NULL;
    }

    SDL_Window *win = calloc(1, sizeof(*win));
    if (!win) return NULL;

    int dw, dh;
    desktop_size(&dw, &dh);
    if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
        w = dw;
        h = dh;
    }
    if (w <= 0) w = 640;
    if (h <= 0) h = 480;
    if (SDL_WINDOWPOS_ISUNDEFINED(x) || SDL_WINDOWPOS_ISCENTERED(x)) x = (dw - w) / 2;
    if (SDL_WINDOWPOS_ISUNDEFINED(y) || SDL_WINDOWPOS_ISCENTERED(y)) y = (dh - h) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    win->id    = v.next_id++;
    win->flags = flags | SDL_WINDOW_SHOWN;
    win->x = x; win->y = y;
    win->w = w; win->h = h;
    snprintf(win->title, sizeof(win->title), "%s", title ? title : "");

    if (v.has_x) {
        XWindow root  = p_XDefaultRootWindow(v.dpy);
        unsigned long black = p_XBlackPixel(v.dpy, v.screen);
        win->xwin = p_XCreateSimpleWindow(v.dpy, root, x, y, (unsigned)w, (unsigned)h, 0, black, black);
        if (!win->xwin) {
            shim_set_error("XCreateSimpleWindow failed");
            free(win);
            return NULL;
        }
        p_XStoreName(v.dpy, win->xwin, win->title);
        p_XSelectInput(v.dpy, win->xwin, X_StructureNotifyMask | X_ExposureMask | X_FocusChangeMask
                                         | X_KeyPressMask | X_KeyReleaseMask);
        p_XSetWMProtocols(v.dpy, win->xwin, &v.wm_delete, 1);

        XWMHintsRaw hints;
        memset(&hints, 0, sizeof(hints));
        hints.flags         = X_InputHint | X_StateHint;
        hints.input         = v.x11_keys ? 1 : 0;
        hints.initial_state = X_NormalState;
        p_XSetWMHints(v.dpy, win->xwin, &hints);

        if (flags & SDL_WINDOW_FULLSCREEN) x11_fullscreen(win, true);
        if (!(flags & SDL_WINDOW_HIDDEN)) {
            p_XMapRaised(v.dpy, win->xwin);
            win->mapped = true;
        }
        p_XSync(v.dpy, 0);
    }

    if (flags & SDL_WINDOW_OPENGL) {
        if (!surface_create(win)) {
            if (win->xwin) p_XDestroyWindow(v.dpy, win->xwin);
            free(win);
            return NULL;
        }
    }

    v.window = win;
    if (!v.cursor) v.cursor = &v.default_cursor;
    cursor_apply();

    /* Under the core the X window never takes focus (WM input hint is off) and
     * keys arrive over IPC, so report focus ourselves — games that pause when
     * unfocused would otherwise never run. */
    if (v.synthetic_focus) {
        win->flags |= SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS;
        shim_events_window(SDL_WINDOWEVENT_SHOWN, 0, 0);
        shim_events_window(SDL_WINDOWEVENT_FOCUS_GAINED, 0, 0);
    }

    shim_ipc_send(GECND_SDL2_PKT_WINDOW, 0, (uint16_t)w, (uint32_t)h);
    fprintf(stderr, "[libSDL2-shim] window %dx%d '%s' flags=0x%x\n", w, h, win->title, flags);
    return win;
}

void SDL_DestroyWindow(SDL_Window *window) {
    if (!window) return;
    surface_destroy(window);
    if (v.has_x && window->xwin) {
        p_XDestroyWindow(v.dpy, window->xwin);
        p_XFlush(v.dpy);
    }
    if (v.window == window) v.window = NULL;
    free(window);
}

void shim_video_quit(void) {
    if (v.window) SDL_DestroyWindow(v.window);
    if (v.has_x && v.invisible) p_XFreeCursor(v.dpy, v.invisible);
    v.invisible = X_None;
    v.cursor    = NULL;
    if (v.ready && v.edpy != EGL_NO_DISPLAY) {
        p_eglMakeCurrent(v.edpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        p_eglTerminate(v.edpy);
    }
    if (v.has_x && v.dpy) p_XCloseDisplay(v.dpy);
    v.edpy    = EGL_NO_DISPLAY;
    v.current = EGL_NO_CONTEXT;
    v.dpy     = NULL;
    v.has_x   = false;
    v.ready   = false;
    v.failed  = false;
}

void SDL_GetWindowSize(SDL_Window *window, int *w, int *h) {
    if (w) *w = window ? window->w : 0;
    if (h) *h = window ? window->h : 0;
}

void SDL_GetWindowPosition(SDL_Window *window, int *x, int *y) {
    if (x) *x = window ? window->x : 0;
    if (y) *y = window ? window->y : 0;
}

void SDL_SetWindowSize(SDL_Window *window, int w, int h) {
    if (!window || w <= 0 || h <= 0) return;
    window->w = w;
    window->h = h;
    if (v.has_x && window->xwin) {
        p_XResizeWindow(v.dpy, window->xwin, (unsigned)w, (unsigned)h);
        p_XFlush(v.dpy);
    }
}

void SDL_SetWindowPosition(SDL_Window *window, int x, int y) {
    if (!window) return;
    int dw, dh;
    desktop_size(&dw, &dh);
    if (SDL_WINDOWPOS_ISUNDEFINED(x) || SDL_WINDOWPOS_ISCENTERED(x)) x = (dw - window->w) / 2;
    if (SDL_WINDOWPOS_ISUNDEFINED(y) || SDL_WINDOWPOS_ISCENTERED(y)) y = (dh - window->h) / 2;
    window->x = x;
    window->y = y;
    if (v.has_x && window->xwin) {
        p_XMoveWindow(v.dpy, window->xwin, x, y);
        p_XFlush(v.dpy);
    }
}

void SDL_SetWindowTitle(SDL_Window *window, const char *title) {
    if (!window) return;
    snprintf(window->title, sizeof(window->title), "%s", title ? title : "");
    if (v.has_x && window->xwin) {
        p_XStoreName(v.dpy, window->xwin, window->title);
        p_XFlush(v.dpy);
    }
}

const char *SDL_GetWindowTitle(SDL_Window *window) {
    return window ? window->title : "";
}

int SDL_SetWindowFullscreen(SDL_Window *window, Uint32 flags) {
    if (!window) return -1;
    bool enable = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    window->flags &= ~(Uint32)SDL_WINDOW_FULLSCREEN_DESKTOP;
    window->flags |= flags & SDL_WINDOW_FULLSCREEN_DESKTOP;
    x11_fullscreen(window, enable);
    if (!v.has_x && enable) {
        int dw, dh;
        desktop_size(&dw, &dh);
        window->w = dw;
        window->h = dh;
    }
    return 0;
}

void SDL_SetWindowResizable(SDL_Window *window, SDL_bool resizable) {
    if (!window) return;
    if (resizable) window->flags |= SDL_WINDOW_RESIZABLE;
    else           window->flags &= ~(Uint32)SDL_WINDOW_RESIZABLE;
}

void SDL_SetWindowMinimumSize(SDL_Window *window, int min_w, int min_h) {
    if (!window) return;
    window->min_w = min_w;
    window->min_h = min_h;
}

void SDL_GetWindowMinimumSize(SDL_Window *window, int *w, int *h) {
    if (w) *w = window ? window->min_w : 0;
    if (h) *h = window ? window->min_h : 0;
}

void SDL_SetWindowBordered(SDL_Window *window, SDL_bool bordered) {
    (void)window; (void)bordered;
}

void SDL_SetWindowIcon(SDL_Window *window, SDL_Surface *icon) {
    (void)window; (void)icon;
}

void SDL_ShowWindow(SDL_Window *window) {
    if (!window) return;
    window->flags |= SDL_WINDOW_SHOWN;
    if (v.has_x && window->xwin) {
        p_XMapRaised(v.dpy, window->xwin);
        p_XFlush(v.dpy);
    }
}

void SDL_HideWindow(SDL_Window *window) {
    if (!window) return;
    window->flags &= ~(Uint32)SDL_WINDOW_SHOWN;
    if (v.has_x && window->xwin) {
        p_XUnmapWindow(v.dpy, window->xwin);
        p_XFlush(v.dpy);
    }
}

void SDL_RaiseWindow(SDL_Window *window) {
    if (!window) return;
    if (v.has_x && window->xwin) {
        p_XRaiseWindow(v.dpy, window->xwin);
        p_XFlush(v.dpy);
    }
}

void SDL_MinimizeWindow(SDL_Window *window) { (void)window; }
void SDL_MaximizeWindow(SDL_Window *window) { (void)window; }
void SDL_RestoreWindow(SDL_Window *window)  { (void)window; }

Uint32 SDL_GetWindowFlags(SDL_Window *window) {
    return window ? window->flags : 0;
}

Uint32 SDL_GetWindowID(SDL_Window *window) {
    return window ? window->id : 0;
}

SDL_Window *SDL_GetWindowFromID(Uint32 id) {
    return (v.window && v.window->id == id) ? v.window : NULL;
}

int SDL_GetWindowDisplayIndex(SDL_Window *window) {
    (void)window;
    return 0;
}

Uint32 SDL_GetWindowPixelFormat(SDL_Window *window) {
    (void)window;
    return SDL_PIXELFORMAT_RGB888;
}

/* ── cursor / grab / focus ───────────────────────────────────────── */

typedef struct {
    unsigned int version, size, width, height, xhot, yhot, delay;
    unsigned int *pixels;
} XcursorImageRaw;

static struct {
    XcursorImageRaw *(*create)(int, int);
    XCursor          (*load)(XDisplay, const XcursorImageRaw *);
    void             (*destroy)(XcursorImageRaw *);
} xcursor;

static bool xcursor_bind(void) {
    if (xcursor.create) return true;
    if (!v.lib_xcursor) v.lib_xcursor = dlopen("libXcursor.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!v.lib_xcursor) return false;
    xcursor.create  = (typeof(xcursor.create)) dlsym(v.lib_xcursor, "XcursorImageCreate");
    xcursor.load    = (typeof(xcursor.load))   dlsym(v.lib_xcursor, "XcursorImageLoadCursor");
    xcursor.destroy = (typeof(xcursor.destroy))dlsym(v.lib_xcursor, "XcursorImageDestroy");
    return xcursor.create && xcursor.load && xcursor.destroy;
}

static const unsigned k_system_cursors[SDL_NUM_SYSTEM_CURSORS] = {
    [SDL_SYSTEM_CURSOR_ARROW]     = 68,
    [SDL_SYSTEM_CURSOR_IBEAM]     = 152,
    [SDL_SYSTEM_CURSOR_WAIT]      = 150,
    [SDL_SYSTEM_CURSOR_CROSSHAIR] = 34,
    [SDL_SYSTEM_CURSOR_WAITARROW] = 150,
    [SDL_SYSTEM_CURSOR_SIZENWSE]  = 134,
    [SDL_SYSTEM_CURSOR_SIZENESW]  = 136,
    [SDL_SYSTEM_CURSOR_SIZEWE]    = 108,
    [SDL_SYSTEM_CURSOR_SIZENS]    = 116,
    [SDL_SYSTEM_CURSOR_SIZEALL]   = 52,
    [SDL_SYSTEM_CURSOR_NO]        = 88,
    [SDL_SYSTEM_CURSOR_HAND]      = 60,
};

static XCursor cursor_invisible(void) {
    if (v.invisible || !v.has_x) return v.invisible;
    static const char blank[8] = {0};
    XPixmap   pixmap = p_XCreateBitmapFromData(v.dpy, p_XDefaultRootWindow(v.dpy), blank, 8, 8);
    XColorRaw black  = {0};
    v.invisible = p_XCreatePixmapCursor(v.dpy, pixmap, pixmap, &black, &black, 0, 0);
    p_XFreePixmap(v.dpy, pixmap);
    return v.invisible;
}

static void cursor_apply(void) {
    if (!v.has_x || !v.window || !v.window->xwin) return;
    if (!v.cursor_shown) {
        p_XDefineCursor(v.dpy, v.window->xwin, cursor_invisible());
    } else if (v.cursor && v.cursor->xcursor) {
        p_XDefineCursor(v.dpy, v.window->xwin, v.cursor->xcursor);
    } else {
        p_XUndefineCursor(v.dpy, v.window->xwin);
    }
    p_XFlush(v.dpy);
}

SDL_Cursor *SDL_CreateSystemCursor(SDL_SystemCursor id) {
    if (!video_init()) return NULL;
    SDL_Cursor *cursor = calloc(1, sizeof(*cursor));
    if (!cursor) {
        shim_set_error("out of memory");
        return NULL;
    }
    if (v.has_x && id >= 0 && id < SDL_NUM_SYSTEM_CURSORS) {
        cursor->xcursor = p_XCreateFontCursor(v.dpy, k_system_cursors[id]);
        cursor->owned   = cursor->xcursor != X_None;
    }
    return cursor;
}

SDL_Cursor *SDL_CreateColorCursor(SDL_Surface *surface, int hot_x, int hot_y) {
    if (!video_init()) return NULL;
    SDL_Cursor *cursor = calloc(1, sizeof(*cursor));
    if (!cursor) {
        shim_set_error("out of memory");
        return NULL;
    }
    if (!v.has_x || !surface || !surface->pixels || surface->format->BytesPerPixel != 4
            || !xcursor_bind()) {
        return cursor;
    }

    XcursorImageRaw *image = xcursor.create(surface->w, surface->h);
    if (!image) return cursor;
    image->xhot = (unsigned)(hot_x < 0 ? 0 : hot_x);
    image->yhot = (unsigned)(hot_y < 0 ? 0 : hot_y);

    const SDL_PixelFormat *fmt = surface->format;
    for (int y = 0; y < surface->h; y++) {
        const Uint32 *row = (const Uint32 *)((const Uint8 *)surface->pixels + (size_t)y * (size_t)surface->pitch);
        for (int x = 0; x < surface->w; x++) {
            Uint32 px = row[x];
            Uint32 r  = fmt->Rmask ? ((px & fmt->Rmask) >> fmt->Rshift) << fmt->Rloss : 0;
            Uint32 g  = fmt->Gmask ? ((px & fmt->Gmask) >> fmt->Gshift) << fmt->Gloss : 0;
            Uint32 b  = fmt->Bmask ? ((px & fmt->Bmask) >> fmt->Bshift) << fmt->Bloss : 0;
            Uint32 a  = fmt->Amask ? ((px & fmt->Amask) >> fmt->Ashift) << fmt->Aloss : 255;
            r = r * a / 255;
            g = g * a / 255;
            b = b * a / 255;
            image->pixels[(size_t)y * (size_t)surface->w + (size_t)x] =
                (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    cursor->xcursor = xcursor.load(v.dpy, image);
    cursor->owned   = cursor->xcursor != X_None;
    xcursor.destroy(image);
    return cursor;
}

SDL_Cursor *SDL_GetDefaultCursor(void) {
    return &v.default_cursor;
}

SDL_Cursor *SDL_GetCursor(void) {
    return v.cursor ? v.cursor : &v.default_cursor;
}

void SDL_SetCursor(SDL_Cursor *cursor) {
    if (cursor) v.cursor = cursor;
    cursor_apply();
}

void SDL_FreeCursor(SDL_Cursor *cursor) {
    if (!cursor || cursor == &v.default_cursor) return;
    if (v.cursor == cursor) {
        v.cursor = &v.default_cursor;
        cursor_apply();
    }
    if (cursor->owned && v.has_x) p_XFreeCursor(v.dpy, cursor->xcursor);
    free(cursor);
}

int SDL_ShowCursor(int toggle) {
    int previous = v.cursor_shown ? SDL_ENABLE : SDL_DISABLE;
    if (toggle < 0) return previous;
    v.cursor_shown = toggle != SDL_DISABLE;
    cursor_apply();
    return previous;
}

/* Pointer grab only happens when this process owns the keyboard (standalone
 * mode); under the core the child must not steal the user's pointer. */
void SDL_SetWindowGrab(SDL_Window *window, SDL_bool grabbed) {
    if (!window) return;
    v.grabbed = grabbed == SDL_TRUE;
    if (grabbed) window->flags |= SDL_WINDOW_INPUT_GRABBED;
    else         window->flags &= ~(Uint32)SDL_WINDOW_INPUT_GRABBED;

    if (!v.has_x || !window->xwin || !v.x11_keys) return;
    if (grabbed) {
        p_XGrabPointer(v.dpy, window->xwin, 1,
                       X_PointerMotionMask | X_ButtonPressMask | X_ButtonReleaseMask,
                       X_GrabModeAsync, X_GrabModeAsync, window->xwin, X_None, X_CurrentTime);
    } else {
        p_XUngrabPointer(v.dpy, X_CurrentTime);
    }
    p_XFlush(v.dpy);
}

SDL_bool SDL_GetWindowGrab(SDL_Window *window) {
    if (!window) return SDL_FALSE;
    return (window->flags & SDL_WINDOW_INPUT_GRABBED) ? SDL_TRUE : SDL_FALSE;
}

SDL_Window *SDL_GetGrabbedWindow(void) {
    return v.grabbed ? v.window : NULL;
}

SDL_Window *SDL_GetKeyboardFocus(void) {
    if (v.window && (v.window->flags & SDL_WINDOW_INPUT_FOCUS)) return v.window;
    return NULL;
}

SDL_Window *SDL_GetMouseFocus(void) {
    if (v.window && (v.window->flags & SDL_WINDOW_MOUSE_FOCUS)) return v.window;
    return NULL;
}

/* ── displays ────────────────────────────────────────────────────── */

static const int k_modes[][2] = {
    { 3840, 2160 }, { 2560, 1440 }, { 1920, 1080 }, { 1600, 900 },
    { 1366, 768 },  { 1280, 720 },  { 1024, 768 },  { 800, 600 }, { 640, 480 },
};

static void mode_fill(SDL_DisplayMode *mode, int w, int h) {
    mode->format       = SDL_PIXELFORMAT_RGB888;
    mode->w            = w;
    mode->h            = h;
    mode->refresh_rate = 60;
    mode->driverdata   = NULL;
}

static int modes_build(SDL_DisplayMode *out, int cap) {
    int dw, dh;
    desktop_size(&dw, &dh);
    int n = 0;
    mode_fill(&out[n++], dw, dh);
    for (size_t i = 0; i < sizeof(k_modes) / sizeof(*k_modes) && n < cap; i++) {
        int w = k_modes[i][0], h = k_modes[i][1];
        if (w > dw || h > dh || (w == dw && h == dh)) continue;
        mode_fill(&out[n++], w, h);
    }
    return n;
}

int SDL_GetNumVideoDisplays(void) {
    return video_init() ? 1 : 0;
}

const char *SDL_GetDisplayName(int displayIndex) {
    (void)displayIndex;
    return "gecnd";
}

int SDL_GetDesktopDisplayMode(int displayIndex, SDL_DisplayMode *mode) {
    (void)displayIndex;
    if (!mode || !video_init()) return -1;
    int dw, dh;
    desktop_size(&dw, &dh);
    mode_fill(mode, dw, dh);
    return 0;
}

int SDL_GetCurrentDisplayMode(int displayIndex, SDL_DisplayMode *mode) {
    return SDL_GetDesktopDisplayMode(displayIndex, mode);
}

int SDL_GetNumDisplayModes(int displayIndex) {
    (void)displayIndex;
    if (!video_init()) return -1;
    SDL_DisplayMode modes[16];
    return modes_build(modes, 16);
}

int SDL_GetDisplayMode(int displayIndex, int modeIndex, SDL_DisplayMode *mode) {
    (void)displayIndex;
    if (!mode || !video_init()) return -1;
    SDL_DisplayMode modes[16];
    int n = modes_build(modes, 16);
    if (modeIndex < 0 || modeIndex >= n) {
        shim_set_error("display mode index %d out of range", modeIndex);
        return -1;
    }
    *mode = modes[modeIndex];
    return 0;
}

SDL_DisplayMode *SDL_GetClosestDisplayMode(int displayIndex, const SDL_DisplayMode *mode, SDL_DisplayMode *closest) {
    (void)displayIndex;
    if (!mode || !closest) return NULL;
    mode_fill(closest, mode->w, mode->h);
    return closest;
}

int SDL_GetDisplayDPI(int displayIndex, float *ddpi, float *hdpi, float *vdpi) {
    (void)displayIndex;
    if (!video_init()) return -1;
    float dpi = 96.0f;
    if (v.has_x) {
        int mm = p_XDisplayWidthMM(v.dpy, v.screen);
        int px = p_XDisplayWidth(v.dpy, v.screen);
        if (mm > 0 && px > 0) dpi = (float)px / ((float)mm / 25.4f);
    }
    if (ddpi) *ddpi = dpi;
    if (hdpi) *hdpi = dpi;
    if (vdpi) *vdpi = dpi;
    return 0;
}

int SDL_GetDisplayBounds(int displayIndex, SDL_Rect *rect) {
    (void)displayIndex;
    if (!rect || !video_init()) return -1;
    int dw, dh;
    desktop_size(&dw, &dh);
    rect->x = 0;
    rect->y = 0;
    rect->w = dw;
    rect->h = dh;
    return 0;
}

int SDL_GetDisplayUsableBounds(int displayIndex, SDL_Rect *rect) {
    return SDL_GetDisplayBounds(displayIndex, rect);
}

SDL_DisplayOrientation SDL_GetDisplayOrientation(int displayIndex) {
    (void)displayIndex;
    return SDL_ORIENTATION_UNKNOWN;
}

int SDL_GetWindowDisplayMode(SDL_Window *window, SDL_DisplayMode *mode) {
    if (!window || !mode) return -1;
    mode_fill(mode, window->w, window->h);
    return 0;
}

int SDL_SetWindowDisplayMode(SDL_Window *window, const SDL_DisplayMode *mode) {
    (void)window; (void)mode;
    return 0;
}

const char *SDL_GetCurrentVideoDriver(void) {
    return v.has_x ? "x11" : "gecnd";
}

int SDL_GetNumVideoDrivers(void) {
    return 1;
}

const char *SDL_GetVideoDriver(int index) {
    (void)index;
    return SDL_GetCurrentVideoDriver();
}

int SDL_VideoInit(const char *driver_name) {
    (void)driver_name;
    return video_init() ? 0 : -1;
}

void SDL_VideoQuit(void) {
    shim_video_quit();
}
