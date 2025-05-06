#include "shared_memory.hpp"

#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

// Old version.
// static std::unordered_map<int32_t, std::map<int32_t, AviUtl::SharedMemoryInfo *>> g_handle_map;
static std::unordered_map<int32_t, std::map<int32_t, HANDLE>> g_handle_map;
static std::mutex g_mutex;

// AviUtl::SharedMemoryInfo *
HANDLE
SharedMemoryInternal::get_shared_mem_handle(int32_t key1, int32_t key2) {
    std::lock_guard<std::mutex> lock(g_mutex);

    auto it = g_handle_map.find(key1);
    if (it == g_handle_map.end())
        return nullptr;

    auto &handles = it->second;
    auto idx_it = handles.find(key2);
    if (idx_it == handles.end())
        return nullptr;

    return idx_it->second;
}

void
SharedMemoryInternal::set_shared_mem_handle(int32_t key1, int32_t key2, HANDLE handle) {
    std::lock_guard<std::mutex> lock(g_mutex);

    auto &handles = g_handle_map[key1];
    handles[key2] = handle;
}

void
cleanup_all(uint16_t script_id) {
    std::lock_guard<std::mutex> lock(g_mutex);

    uint32_t target_bits = static_cast<uint32_t>(script_id) & 0x3;
    std::vector<int32_t> keys_to_erase;

    for (auto &[key1, inner_map] : g_handle_map) {
        uint32_t source_bits = static_cast<uint32_t>(key1) >> 30;
        if (source_bits != target_bits)
            continue;

        for (auto &[__, handle] : inner_map) {
            if (handle && handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);

            // Old Version.
            /*
            AviUtl::SharedMemoryInfo* handle = it2->second;
            if (handle != nullptr) {
                obj_utils.delete_shared_mem(key1, handle);
            }
            */
        }

        inner_map.clear();
        keys_to_erase.push_back(key1);
    }

    for (int32_t key1 : keys_to_erase) {
        g_handle_map.erase(key1);
    }
}

void
cleanup_for_id(uint16_t script_id, uint16_t object_id) {
    std::lock_guard<std::mutex> lock(g_mutex);

    uint32_t id_t = (static_cast<uint32_t>(script_id & 0x3) << 30);
    uint32_t id_b = static_cast<uint32_t>(object_id & 0x3FFF);
    uint32_t target_bits = id_t | id_b;

    std::vector<int32_t> keys_to_erase;

    for (auto &[key1, inner_map] : g_handle_map) {
        uint32_t source_bits = static_cast<uint32_t>(key1 & (0x3 << 30 | 0x3FFF));
        if (source_bits != target_bits)
            continue;

        for (auto &[__, handle] : inner_map) {
            if (handle && handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);

            // Old Version.
            /*
            AviUtl::SharedMemoryInfo *handle = it2->second;
            if (handle != nullptr) {
                obj_utils.delete_shared_mem(key1, handle);
            }
            */
        }

        inner_map.clear();
        keys_to_erase.push_back(key1);
    }

    for (int32_t key1 : keys_to_erase) {
        g_handle_map.erase(key1);
    }
}

bool
handle_exists(int32_t key1) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_handle_map.find(key1) != g_handle_map.end())
        return true;
    else
        return false;
}

static void
cleanup_all_handle() {
    for (auto &[_, inner_map] : g_handle_map) {
        for (auto &[__, handle] : inner_map) {
            if (handle && handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);
        }

        inner_map.clear();
    }

    g_handle_map.clear();
}

extern "C" BOOL APIENTRY
DllMain([[maybe_unused]] HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_DETACH) {
        cleanup_all_handle();
    }
    return TRUE;
}