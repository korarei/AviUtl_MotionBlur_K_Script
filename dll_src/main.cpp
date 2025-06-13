#include <iostream>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <lua.hpp>

#include "object_motion_blur.hpp"

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.0.0"
#endif

static luaL_Reg functions[] = {{"process_object_motion_blur", process_object_motion_blur}, {nullptr, nullptr}};

extern "C" int
luaopen_MotionBlur_K(lua_State *L) {
    luaL_register(L, "MotionBlur_K", functions);
    return 1;
}

BOOL APIENTRY
DllMain([[maybe_unused]] HMODULE hModule, DWORD ul_reason_for_call, [[maybe_unused]] LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            std::cout << "[MotionBlur_K.dll][INFO] Version: " << PROJECT_VERSION << std::endl;
            break;
    }

    return TRUE;
}
