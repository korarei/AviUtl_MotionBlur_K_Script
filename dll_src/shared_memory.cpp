#include "shared_memory.hpp"

#include <vector>

std::mutex SharedMemory::mutex;

SharedMemory::SharedMemory() {}
SharedMemory::~SharedMemory() { cleanup_all_handle_impl(); }

void
SharedMemory::cleanup_all_handle_impl() {
    for (auto &[_, inner_map] : handle_map) {
        for (auto &[__, handle] : inner_map) {
            if (handle && handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);
        }

        inner_map.clear();
    }

    handle_map.clear();
}

void
SharedMemory::cleanup_all_handle() {
    std::lock_guard<std::mutex> lock(mutex);
    cleanup_all_handle_impl();
}

void
SharedMemory::cleanup_for_id(uint16_t object_id) {
    std::lock_guard<std::mutex> lock(mutex);

    uint32_t target_bits = static_cast<uint32_t>(object_id & 0x3FFF);
    std::vector<int32_t> keys_to_erase;

    for (auto &[key1, inner_map] : handle_map) {
        uint32_t source_bits = static_cast<uint32_t>(key1 & 0x3FFF);
        if (source_bits != target_bits)
            continue;

        for (auto &[__, handle] : inner_map) {
            if (handle && handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);
        }

        inner_map.clear();
        keys_to_erase.push_back(key1);
    }

    for (int32_t key1 : keys_to_erase) {
        handle_map.erase(key1);
    }
}

bool
SharedMemory::handle_exists(int32_t key1) const {
    std::lock_guard<std::mutex> lock(mutex);

    if (handle_map.find(key1) != handle_map.end())
        return true;
    else
        return false;
}

HANDLE
SharedMemory::get_shared_mem_handle(int32_t key1, int32_t key2) const {
    auto it_key1 = handle_map.find(key1);
    if (it_key1 == handle_map.end())
        return nullptr;

    auto &handles = it_key1->second;
    auto it_key2 = handles.find(key2);
    if (it_key2 == handles.end())
        return nullptr;

    return it_key2->second;
}

void
SharedMemory::set_shared_mem_handle(int32_t key1, int32_t key2, HANDLE handle) {
    auto &handles = handle_map[key1];
    handles[key2] = handle;
}
