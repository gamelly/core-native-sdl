#include "zeebo.h"

extern int luaopen_cjson(lua_State* L);

void native_json_install(lua_State* L) {
    luaopen_cjson(L);
    lua_setglobal(L, "native_dict_json");

    lua_getglobal(L, "native_dict_json");
    lua_pushstring(L, "encode_empty_table_as_object");
    lua_gettable(L, -2);
    lua_pushboolean(L, false);
    lua_pcall(L, 1, 0, 0);

    lua_getglobal(L, "native_dict_json");
    lua_pushstring(L, "encode_skip_unsupported_value_types");
    lua_gettable(L, -2);
    lua_pushboolean(L, true);
    lua_pcall(L, 1, 0, 0);
}

#ifdef DOXYGEN
/**
 * @defgroup api
 * @{
 */

/**
 * @short @c std.json
 *
 * @li https://github.com/openresty/lua-cjson
 *
 * @par Lua definition
 * @code
 * local native_dict_json = {
 *  decode = function (x) end,
 *  encode = function (x) end
 * }
 * @endcode
 */
class native_dict_json {
public:
    /**
     * @short @c std.json.encode
     * @param [in] x @c any
     * @return @c string
     */
    int encode(lua_State* L);
    /**
     * @short @c std.json.decode
     * @param [in] x @c string
     * @return @c any
     */
    int decode(lua_State* L);
};

/**
 * @}
 */

#endif
