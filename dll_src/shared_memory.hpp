#pragma once

#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <unordered_map>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

class SharedMemory {
public:
    SharedMemory(uint32_t block_bits = 0u);
    ~SharedMemory();

    void cleanup_all_handle();
    void cleanup_for_key1_mask(uint32_t match_bits, uint32_t mask);
    bool has_key1(uint32_t key1) const;
    bool has_key_pair(uint32_t key1, uint32_t key2) const;

    template <typename T>
    void write(uint32_t key1, uint32_t key2, const T &val) {
        std::lock_guard<std::mutex> lock(mutex);

        const size_t size = sizeof(T);
        const size_t total_size = size << block_bits;

        uint32_t block_id, block_offset;
        calc_block_pos(key2, block_id, block_offset);

        HANDLE old_handle = get_shared_mem_handle(key1, block_id);
        HANDLE new_handle = nullptr;
        bool is_newly_created = false;

        if (old_handle == nullptr) {
            new_handle = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, total_size, nullptr);
            if (new_handle == nullptr)
                return;

            is_newly_created = true;
        } else {
            new_handle = old_handle;
        }

        T *ptr = static_cast<T *>(MapViewOfFile(new_handle, FILE_MAP_ALL_ACCESS, 0, 0, total_size));
        if (ptr == nullptr) {
            if (is_newly_created)
                CloseHandle(new_handle);

            return;
        }

        std::memcpy(ptr + block_offset, &val, size);
        UnmapViewOfFile(ptr);

        if (is_newly_created)
            set_shared_mem_handle(key1, block_id, new_handle);
    }

    template <typename T>
    bool read(uint32_t key1, uint32_t key2, T &val) const {
        std::lock_guard<std::mutex> lock(mutex);

        const size_t size = sizeof(T);
        const size_t total_size = size << block_bits;

        uint32_t block_id, block_offset;
        calc_block_pos(key2, block_id, block_offset);

        HANDLE handle = get_shared_mem_handle(key1, block_id);
        if (handle == nullptr)
            return false;

        T *ptr = static_cast<T *>(MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, total_size));
        if (ptr == nullptr) {
            return false;
        }

        std::memcpy(&val, ptr + block_offset, size);
        UnmapViewOfFile(ptr);

        return true;
    }

private:
    static std::mutex mutex;

    std::unordered_map<uint32_t, std::map<uint32_t, HANDLE>> handle_map;
    uint32_t block_bits;

    void calc_block_pos(uint32_t key, uint32_t &block_id, uint32_t &block_offset) const;
    HANDLE get_shared_mem_handle(uint32_t key1, uint32_t block_id) const;
    void set_shared_mem_handle(uint32_t key1, uint32_t block_id, HANDLE handle);
    void cleanup_all_handle_impl() noexcept;
};

inline void
SharedMemory::calc_block_pos(uint32_t key, uint32_t &block_id, uint32_t &block_offset) const {
    block_id = key >> block_bits;                    // key / (2 ^ block_bits)
    block_offset = key & ((1u << block_bits) - 1u);  // key % (2 ^ block_bits)
}
