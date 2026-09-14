#define _GNU_SOURCE
#include <dirent.h>

#include "shim.h"

/* ── surface ─────────────────────────────────────────────────────── */

static const struct {
    Uint32 format;
    int    bpp;
    Uint32 r, g, b, a;
} k_formats[] = {
    { SDL_PIXELFORMAT_ARGB8888, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 },
    { SDL_PIXELFORMAT_RGBA8888, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff },
    { SDL_PIXELFORMAT_ABGR8888, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 },
    { SDL_PIXELFORMAT_BGRA8888, 32, 0x0000ff00, 0x00ff0000, 0xff000000, 0x000000ff },
    { SDL_PIXELFORMAT_RGB888,   32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000 },
    { SDL_PIXELFORMAT_BGR888,   32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0x00000000 },
    { SDL_PIXELFORMAT_RGB24,    24, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000 },
    { SDL_PIXELFORMAT_BGR24,    24, 0x000000ff, 0x0000ff00, 0x00ff0000, 0x00000000 },
    { SDL_PIXELFORMAT_RGB565,   16, 0x0000f800, 0x000007e0, 0x0000001f, 0x00000000 },
    { SDL_PIXELFORMAT_RGB555,   16, 0x00007c00, 0x000003e0, 0x0000001f, 0x00000000 },
    { SDL_PIXELFORMAT_ARGB1555, 16, 0x00007c00, 0x000003e0, 0x0000001f, 0x00008000 },
    { SDL_PIXELFORMAT_ARGB4444, 16, 0x00000f00, 0x000000f0, 0x0000000f, 0x0000f000 },
};

static Uint32 masks_to_format(int bpp, Uint32 r, Uint32 g, Uint32 b, Uint32 a) {
    for (size_t i = 0; i < sizeof(k_formats) / sizeof(*k_formats); i++) {
        if (k_formats[i].bpp == bpp && k_formats[i].r == r && k_formats[i].g == g
                && k_formats[i].b == b && k_formats[i].a == a) {
            return k_formats[i].format;
        }
    }
    return SDL_PIXELFORMAT_UNKNOWN;
}

static void mask_split(Uint32 mask, Uint8 *shift, Uint8 *loss) {
    if (mask == 0) {
        *shift = 0;
        *loss  = 8;
        return;
    }
    *shift = (Uint8)__builtin_ctz(mask);
    *loss  = (Uint8)(8 - __builtin_popcount(mask));
}

static SDL_Surface *surface_new(void *pixels, int width, int height, int depth, int pitch,
                                Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask) {
    if (width < 0 || height < 0 || depth <= 0) {
        shim_set_error("invalid surface geometry %dx%d@%d", width, height, depth);
        return NULL;
    }

    SDL_Surface     *surface = calloc(1, sizeof(*surface));
    SDL_PixelFormat *format  = calloc(1, sizeof(*format));
    if (!surface || !format) {
        free(surface);
        free(format);
        shim_set_error("out of memory");
        return NULL;
    }

    format->format        = masks_to_format(depth, rmask, gmask, bmask, amask);
    format->BitsPerPixel  = (Uint8)depth;
    format->BytesPerPixel = (Uint8)((depth + 7) / 8);
    format->Rmask         = rmask;
    format->Gmask         = gmask;
    format->Bmask         = bmask;
    format->Amask         = amask;
    format->refcount      = 1;
    mask_split(rmask, &format->Rshift, &format->Rloss);
    mask_split(gmask, &format->Gshift, &format->Gloss);
    mask_split(bmask, &format->Bshift, &format->Bloss);
    mask_split(amask, &format->Ashift, &format->Aloss);

    surface->format    = format;
    surface->w         = width;
    surface->h         = height;
    surface->pitch     = pitch > 0 ? pitch : width * format->BytesPerPixel;
    surface->clip_rect = (SDL_Rect){ 0, 0, width, height };
    surface->refcount  = 1;

    if (pixels) {
        surface->flags  = SDL_PREALLOC;
        surface->pixels = pixels;
    } else {
        surface->pixels = calloc(1, (size_t)surface->pitch * (size_t)(height > 0 ? height : 1));
        if (!surface->pixels) {
            free(format);
            free(surface);
            shim_set_error("out of memory");
            return NULL;
        }
    }
    return surface;
}

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth, int pitch,
                                      Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
    return surface_new(pixels, width, height, depth, pitch, Rmask, Gmask, Bmask, Amask);
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
                                  Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
    (void)flags;
    return surface_new(NULL, width, height, depth, 0, Rmask, Gmask, Bmask, Amask);
}

void SDL_FreeSurface(SDL_Surface *surface) {
    if (!surface) return;
    if (--surface->refcount > 0) return;
    if (!(surface->flags & SDL_PREALLOC)) free(surface->pixels);
    free(surface->format);
    free(surface);
}

int SDL_LockSurface(SDL_Surface *surface) {
    if (!surface) return SDL_SetError("passed a NULL surface");
    surface->locked++;
    return 0;
}

void SDL_UnlockSurface(SDL_Surface *surface) {
    if (surface && surface->locked > 0) surface->locked--;
}

SDL_bool SDL_SetClipRect(SDL_Surface *surface, const SDL_Rect *rect) {
    if (!surface) return SDL_FALSE;
    if (rect) surface->clip_rect = *rect;
    else      surface->clip_rect = (SDL_Rect){ 0, 0, surface->w, surface->h };
    return SDL_TRUE;
}

void SDL_GetClipRect(SDL_Surface *surface, SDL_Rect *rect) {
    if (surface && rect) *rect = surface->clip_rect;
}

/* ── power ───────────────────────────────────────────────────────── */

static bool sysfs_read(const char *dir, const char *file, char *out, size_t cap) {
    char path[512];
    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/%s", dir, file);
    FILE *f = fopen(path, "r");
    if (!f) return false;
    bool ok = fgets(out, (int)cap, f) != NULL;
    fclose(f);
    if (!ok) return false;
    out[strcspn(out, "\r\n")] = '\0';
    return true;
}

SDL_PowerState SDL_GetPowerInfo(int *seconds, int *percent) {
    SDL_PowerState state = SDL_POWERSTATE_NO_BATTERY;
    int            pct   = -1;
    int            secs  = -1;

    DIR *dir = opendir("/sys/class/power_supply");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            char buf[64];
            if (entry->d_name[0] == '.') continue;
            if (!sysfs_read(entry->d_name, "type", buf, sizeof(buf))) continue;
            if (strcmp(buf, "Battery") != 0) continue;

            if (sysfs_read(entry->d_name, "capacity", buf, sizeof(buf))) pct = atoi(buf);
            if (sysfs_read(entry->d_name, "status", buf, sizeof(buf))) {
                if      (strcmp(buf, "Charging") == 0) state = SDL_POWERSTATE_CHARGING;
                else if (strcmp(buf, "Full") == 0)     state = SDL_POWERSTATE_CHARGED;
                else                                   state = SDL_POWERSTATE_ON_BATTERY;
            } else {
                state = SDL_POWERSTATE_UNKNOWN;
            }
            break;
        }
        closedir(dir);
    }

    if (seconds) *seconds = secs;
    if (percent) *percent = pct;
    return state;
}

/* ── screensaver ─────────────────────────────────────────────────── */

static bool s_screensaver = true;

void SDL_EnableScreenSaver(void) {
    s_screensaver = true;
}

void SDL_DisableScreenSaver(void) {
    s_screensaver = false;
}

SDL_bool SDL_IsScreenSaverEnabled(void) {
    return s_screensaver ? SDL_TRUE : SDL_FALSE;
}

/* ── message box ─────────────────────────────────────────────────── */

static const char *messagebox_level(Uint32 flags) {
    if (flags & SDL_MESSAGEBOX_ERROR)       return "error";
    if (flags & SDL_MESSAGEBOX_WARNING)     return "warning";
    return "info";
}

int SDL_ShowSimpleMessageBox(Uint32 flags, const char *title, const char *message, SDL_Window *window) {
    (void)window;
    fprintf(stderr, "[libSDL2-shim] messagebox %s: %s — %s\n",
            messagebox_level(flags), title ? title : "", message ? message : "");
    return 0;
}

int SDL_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid) {
    if (!messageboxdata) return SDL_SetError("passed a NULL message box");

    fprintf(stderr, "[libSDL2-shim] messagebox %s: %s — %s\n",
            messagebox_level(messageboxdata->flags),
            messageboxdata->title   ? messageboxdata->title   : "",
            messageboxdata->message ? messageboxdata->message : "");

    int chosen = -1;
    for (int i = 0; i < messageboxdata->numbuttons; i++) {
        const SDL_MessageBoxButtonData *button = &messageboxdata->buttons[i];
        if (button->flags & SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT) {
            chosen = button->buttonid;
            break;
        }
        if (chosen < 0) chosen = button->buttonid;
    }
    if (buttonid) *buttonid = chosen;
    return 0;
}
