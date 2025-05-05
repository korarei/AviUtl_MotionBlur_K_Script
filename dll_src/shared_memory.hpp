#pragma once

#include <cstdint>
#include <cstring>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Old versions probably have a bug similar to exedit0.93 test.

// Old Version.
/*
#define NOMINMAX
#include <exedit.hpp>

#include "aul_utils.hpp"
*/

namespace SharedMemoryInternal {
HANDLE
get_shared_mem_handle(int32_t key1, int32_t key2);
void
set_shared_mem_handle(int32_t key1, int32_t key2, HANDLE handle);

// Old Version.
/*
AviUtl::SharedMemoryInfo *
get_shared_mem_handle(int32_t key1, int32_t key2);
void
set_shared_mem_handle(int32_t key1, int32_t key2, AviUtl::SharedMemoryInfo *handle);
*/
}  // namespace SharedMemoryInternal

template <typename T>
inline void
create_shared_mem(int32_t key1, int32_t key2, const T &val) {
    size_t size = sizeof(T);
    if (size == 0)
        return;

    HANDLE handle = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, nullptr);
    if (handle == nullptr)
        return;

    T *ptr = static_cast<T *>(MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, size));
    if (ptr == nullptr) {
        CloseHandle(handle);
        return;
    }

    std::memcpy(ptr, &val, size);
    UnmapViewOfFile(ptr);

    SharedMemoryInternal::set_shared_mem_handle(key1, key2, handle);

    // Old Version.
    /*
    AviUtl::SharedMemoryInfo *handle = SharedMemoryInternal::get_shared_mem_handle(key1, key2);
    if (handle != nullptr) {
        obj_utils.delete_shared_mem(key1, handle);
    }

    if (obj_utils.create_shared_mem(key1, key2, &handle, val))
        SharedMemoryInternal::set_shared_mem_handle(key1, key2, handle);
    */
}

template <typename T>
inline bool
get_shared_mem(int32_t key1, int32_t key2, T &val) {
    size_t size = sizeof(T);
    if (size == 0)
        return false;

    HANDLE handle = SharedMemoryInternal::get_shared_mem_handle(key1, key2);
    if (handle == nullptr)
        return false;

    T *ptr = static_cast<T *>(MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, size));
    if (ptr == nullptr) {
        return false;
    }

    std::memcpy(&val, ptr, size);
    UnmapViewOfFile(ptr);

    return true;

    // Old Version.
    /*
    AviUtl::SharedMemoryInfo *handle = SharedMemoryInternal::get_shared_mem_handle(key1, key2);
    if (handle != nullptr) {
        return obj_utils.get_shared_mem(key1, key2, handle, val);
    }
    return false;
    */
}

void
cleanup_all(uint16_t script_id);

void
cleanup_for_id(uint16_t script_id, uint16_t object_id);

bool
handle_exists(int32_t key1);

// DLL Entry.
extern "C" BOOL APIENTRY
DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);
