#include "zeebo_engine.h"
#include "curl/include/curl/curl.h"

static size_t http_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    lua_State *L = (lua_State *)userp;
    char *body = (char *) malloc(total_size + 1);

    do {
        if (body == NULL) {
            break;
        }
        
        memcpy(body, contents, total_size);
        body[total_size] = '\0';

        lua_pushstring(L, "set");
        lua_gettable(L, -2);
        lua_pushstring(L, "body");
        lua_pushstring(L, body);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            break;
        }

        lua_pushstring(L, "set");
        lua_gettable(L, -2);
        lua_pushstring(L, "ok");
        lua_pushboolean(L, true);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            break;
        }

        lua_pushstring(L, "set");
        lua_gettable(L, -2);
        lua_pushstring(L, "status");
        lua_pushinteger(L, 200);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            break;
        }
    }
    while(0);

    if (body) {
        free(body);
    }

    return total_size; 
}

static int http_handler(lua_State *L) {
    const char *method = NULL;
    const char *url = NULL;
    CURL *curl = NULL;
    CURLcode res;

    do {
        if (!lua_istable(L, 1)) {
            luaL_error(L, "argument must be a table");
            break;
        }

        lua_pushstring(L, "method");
        lua_gettable(L, -2);
        method = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_pushstring(L, "url");
        lua_gettable(L, -2);
        url = lua_tostring(L, -1);
        lua_pop(L, 1); 

        if (method == NULL || url == NULL) {
            luaL_error(L, "missing URL or METHOD");
            break;
        }

        curl = curl_easy_init();

        if (!curl) {
            luaL_error(L, "curl not init.");
            break;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            break;
        }
    }
    while(0);

    if (curl) {
        curl_easy_cleanup(curl);
    }

    return 0;
}

void native_http_install(lua_State* L) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    lua_newtable(L);
    lua_pushstring(L, "handler");
    lua_pushcfunction(L, http_handler);
    lua_settable(L, -3);
    lua_setglobal(L, "native_dict_http");
}

void native_http_cleanup(lua_State* L) {
    curl_global_cleanup();
}
