#define _GNU_SOURCE
#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gecnd.h"
#include "gdmsp.h"
#include "main.h"
#include "ipc.h"

gecnd_api_t *api = NULL;

static struct {
    typeof(gdmsp_control)       *control;
    typeof(gamely_input_add_cb) *add_cb;
} host;

static char s_shim_dir[PATH_MAX];

static bool host_bind(void) {
    if (host.control) return true;
    api->registry("get", "function:gdmsp_control",       (void *)&host.control, NULL);
    api->registry("get", "function:gamely_input_add_cb", (void *)&host.add_cb,  NULL);
    return host.control != NULL;
}

static void on_key(const char *name, bool pressed, int port, void *usr) {
    (void)port; (void)usr;
    uint16_t scancode;
    uint32_t keycode;
    if (keymap_lookup(name, &scancode, &keycode)) {
        process_send_key(scancode, keycode, pressed);
    }
}

static void inputs_bind(void) {
    static bool done = false;
    if (done || !host_bind() || !host.add_cb) return;
    host.add_cb("@code", (void *)on_key, NULL);
    done = true;
}

static const char *shim_dir(void) {
    if (s_shim_dir[0]) return s_shim_dir;
    Dl_info info;
    char    resolved[PATH_MAX];
    if (dladdr((void *)shim_dir, &info) && info.dli_fname && realpath(info.dli_fname, resolved)) {
        char *slash = strrchr(resolved, '/');
        if (slash) *slash = '\0';
        snprintf(s_shim_dir, sizeof(s_shim_dir), "%s", resolved);
    }
    return s_shim_dir;
}

static bool url_location(const char *url, char *out, size_t cap) {
    const char *loc = strstr(url, "://");
    if (!loc) return false;
    loc += 3;
    size_t end = strcspn(loc, "?#");
    if (end == 0) return false;
    snprintf(out, cap, "%.*s", (int)end, loc);
    return true;
}

/* ── player: sdl://path[?a=return&b=escape&shim=/dir&preload=0] ──── */

static gdmsp_fsm_t sdl2_source(uint8_t channel, const char *url, void *usr) {
    (void)channel; (void)usr;

    process_stop(true);
    url_env_set(url);
    keymap_configure();

    char path[PATH_MAX];
    if (!url_location(url, path, sizeof(path))) {
        process_set_error("malformed url: %s", url);
        return GDMSP_FSM_ERROR;
    }

    const char *shim = url_env_get("shim");
    if (!process_request(path, (shim && shim[0]) ? shim : shim_dir())) {
        return GDMSP_FSM_ERROR;
    }
    return GDMSP_FSM_LOADING;
}

static gdmsp_fsm_t sdl2_set(uint8_t channel, gdmsp_cmd_t cmd, gdmsp_value_t value, void *usr) {
    (void)channel; (void)usr; (void)value;
    switch (cmd) {
        case GDMSP_CMD_RESOURCE:
        case GDMSP_CMD_STOP:
            process_stop(false);
            break;
        case GDMSP_CMD_TICK:
            inputs_bind();
            process_tick();
            break;
        default:
            break;
    }
    return process_state();
}

static gdmsp_value_t sdl2_get(uint8_t channel, gdmsp_cmd_t cmd, void *usr) {
    (void)channel; (void)cmd; (void)usr;
    gdmsp_value_t value = {-1};
    return value;
}

static gdmsp_player_t sdl2_player = {
    .src = sdl2_source,
    .set = sdl2_set,
    .get = sdl2_get,
};

/* ── lua ffi ─────────────────────────────────────────────────────── */

static char *lua_native_sdl2_url(char *url) {
    char media_url[2048];
    if (strncmp(url, "sdl", 3) == 0) {
        snprintf(media_url, sizeof(media_url), "%s", url);
    } else {
        snprintf(media_url, sizeof(media_url), "sdl://%s", url);
    }
    if (host_bind()) host.control()->source(0, media_url);
    return NULL;
}

static char *lua_native_sdl2_exit(void) {
    if (host_bind()) host.control()->set(0, GDMSP_CMD_STOP, NULL);
    return NULL;
}

static char *lua_native_sdl2_get_error(const char **const ret) {
    *ret = process_error();
    return NULL;
}

static char *lua_native_sdl2_is_running(bool *const ret) {
    *ret = process_is_running();
    return NULL;
}

static char *lua_native_sdl2_key(char *name, bool pressed) {
    on_key(name, pressed, 0, NULL);
    return NULL;
}

void coreopen_sdl2_gecnd(gecnd_plugin_t *const plugin) {
    api = plugin->require("v1");
    api->registry("set", "lua_global_ffi:native_sdl2_url+$s+$0",        lua_native_sdl2_url,        NULL);
    api->registry("set", "lua_global_ffi:native_sdl2_exit+$0+$0",       lua_native_sdl2_exit,       NULL);
    api->registry("set", "lua_global_ffi:native_sdl2_get_error+$0+$s",  lua_native_sdl2_get_error,  NULL);
    api->registry("set", "lua_global_ffi:native_sdl2_is_running+$0+$b", lua_native_sdl2_is_running, NULL);
    api->registry("set", "lua_global_ffi:native_sdl2_key+$s$b+$0",      lua_native_sdl2_key,        NULL);
    api->registry("set", "media_player:sdl$0",  &sdl2_player, NULL);
    api->registry("set", "media_player:sdl2$0", &sdl2_player, NULL);
}
