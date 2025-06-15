#include <lua.hpp>

#include "object_motion_blur.hpp"

static luaL_Reg functions[] = {{"process_object_motion_blur", process_object_motion_blur}, {nullptr, nullptr}};

extern "C" int
luaopen_MotionBlur_K(lua_State *L) {
    luaL_register(L, "MotionBlur_K", functions);
    return 1;
}
