#include "zeebo_engine.h"
#include "curl/include/curl/curl.h"

static size_t header_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    char http_status[] = "000";
    int status = 0;
    size_t total_size = size * nmemb;
    lua_State *L = (lua_State *)userp;

    do {
        if (strncmp("HTTP/", (char*) contents, 5) == 0) {
            memcpy(http_status, contents + 9, sizeof(http_status) - 1);
            status = atoi(http_status);
            lua_pushstring(L, "set");
            lua_gettable(L, -2);
            lua_pushstring(L, "status");
            lua_pushinteger(L, status);
            lua_pcall(L, 2, 0, 0);

            lua_pushstring(L, "set");
            lua_gettable(L, -2);
            lua_pushstring(L, "ok");
            lua_pushboolean(L, 200 <= status && status < 300);
            lua_pcall(L, 2, 0, 0);
            break;
        }
    }
    while(0);

    return total_size;
}

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
        lua_pcall(L, 2, 0, 0);
    }
    while(0);

    if (body) {
        free(body);
    }

    return total_size; 
}

static int http_handler(lua_State *L) {
    assert(lua_istable(L, 1));
    const char *method = NULL;
    const char *url = NULL;
    const char *error = NULL;
    CURL *curl = NULL;
    CURLcode res;

    do {
        lua_pushstring(L, "method");
        lua_gettable(L, -2);
        method = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_pushstring(L, "url");
        lua_gettable(L, -2);
        url = lua_tostring(L, -1);
        lua_pop(L, 1); 

        if (method == NULL || url == NULL) {
            error = "missing URL\n";
            break;
        }

        curl = curl_easy_init();

        if (!curl) {
            error = "failed to init curl\n";
            break;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, L);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION , header_callback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            error = curl_easy_strerror(res);
            break;
        }
    }
    while(0);

    if (error) {
        lua_pushstring(L, "set");
        lua_gettable(L, -2);
        lua_pushstring(L, "body");
        lua_pcall(L, 1, 0, 0);

        lua_pushstring(L, "set");
        lua_gettable(L, -2);
        lua_pushstring(L, "ok");
        lua_pushboolean(L, false);
        lua_pcall(L, 2, 0, 0);

        lua_pushstring(L, "set");
        lua_gettable(L, -2);
        lua_pushstring(L, "error");
        lua_pushstring(L, error);
        lua_pcall(L, 2, 0, 0);
    }

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
