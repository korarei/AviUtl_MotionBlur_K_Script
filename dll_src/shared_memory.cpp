#include "shared_memory.hpp"

#include <vector>

SharedMemory::SharedMemory(uint32_t block_bits) : block_bits(block_bits) {}
SharedMemory::~SharedMemory() { cleanup_all_handle_impl(); }

void
SharedMemory::cleanup_all_handle_impl() noexcept {
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
SharedMemory::cleanup_for_key1_mask(uint32_t match_bits, uint32_t mask) {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<uint32_t> keys_to_erase;

    for (auto &[key1, inner_map] : handle_map) {
        if ((key1 & mask) != match_bits)
            continue;

        for (auto &[__, handle] : inner_map) {
            if (handle && handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);
        }

        inner_map.clear();
        keys_to_erase.push_back(key1);
    }

    for (uint32_t key1 : keys_to_erase) {
        handle_map.erase(key1);
    }
}

bool
SharedMemory::has_key1(uint32_t key1) const {
    std::lock_guard<std::mutex> lock(mutex);

    return handle_map.find(key1) != handle_map.end();
}

bool
SharedMemory::has_key_pair(uint32_t key1, uint32_t key2) const {
    std::lock_guard<std::mutex> lock(mutex);

    auto it_key1 = handle_map.find(key1);
    if (it_key1 == handle_map.end())
        return false;

    auto &handles = it_key1->second;
    uint32_t block_id = key2 >> block_bits;
    return handles.find(block_id) != handles.end();
}

HANDLE
SharedMemory::get_shared_mem_handle(uint32_t key1, uint32_t block_id) const {
    auto it_key1 = handle_map.find(key1);
    if (it_key1 == handle_map.end())
        return nullptr;

    auto &handles = it_key1->second;
    auto it_block = handles.find(block_id);
    if (it_block == handles.end())
        return nullptr;

    return it_block->second;
}

void
SharedMemory::set_shared_mem_handle(uint32_t key1, uint32_t block_id, HANDLE handle) {
    auto &handles = handle_map[key1];
    handles[block_id] = handle;
}
