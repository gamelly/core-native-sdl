#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

static char *cache = NULL;

__attribute__((destructor))
static void url_env_clear(void) {
    free(cache);
    cache = NULL;
}

void url_env_set(const char *url) {
    free(cache);
    cache = NULL;
    if (url == NULL) {
        return;
    }

    size_t       length = 1;
    gecnd_lang_t ctx    = {{ "url", url }};
    while (api->lang(&ctx)) {
        if (ctx.url.kind == GECND_URL_KIND_PARAM) {
            length += ctx.url.len + 1 + ctx.url.val.len + 1;
        }
    }

    cache = malloc(length);
    if (cache == NULL) {
        return;
    }

    size_t index = 0;
    ctx.reset = 1;
    while (api->lang(&ctx)) {
        if (ctx.url.kind == GECND_URL_KIND_PARAM) {
            memcpy(cache + index, ctx.url.ptr, ctx.url.len);
            index += ctx.url.len;
            cache[index++] = '\0';
            if (ctx.url.val.len) {
                memcpy(cache + index, ctx.url.val.ptr, ctx.url.val.len);
                index += ctx.url.val.len;
            }
            cache[index++] = '\0';
        }
    }
    cache[index] = '\0';
}

const char *url_env_get(const char *key) {
    if (cache == NULL || key == NULL) {
        return NULL;
    }
    gecnd_lang_t ctx = {{ "kv:param", cache, key }};
    if (api->lang(&ctx)) {
        return ctx.result.ptr;
    }
    return NULL;
}

/* Resolve `given` contra a PASTA de `base_file`, nao contra o cwd do host:
 * quem escreve a URL pensa em termos da pasta do jogo. Caminho absoluto passa
 * intacto. Usado por ?gptk=, ?bin= e ?ld=. */
void url_resolve_rel(const char *given, const char *base_file, char *out, size_t cap) {
    if (!given || !out || cap == 0) return;

    if (given[0] == '/' || !base_file || !base_file[0]) {
        snprintf(out, cap, "%s", given);
        return;
    }

    char base[1024];
    snprintf(base, sizeof(base), "%s", base_file);
    char *slash = strrchr(base, '/');
    if (!slash) {
        snprintf(out, cap, "%s", given);
        return;
    }
    *slash = '\0';

    /* "./x" e "x" dao no mesmo; comer o "./" mantem o caminho legivel no log */
    if (given[0] == '.' && given[1] == '/') given += 2;
    snprintf(out, cap, "%s/%s", base, given);
}
