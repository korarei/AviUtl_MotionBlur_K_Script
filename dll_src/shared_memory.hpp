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
    SharedMemory();
    ~SharedMemory();

    void cleanup_all_handle();
    void cleanup_for_id(uint16_t object_id);
    bool handle_exists(int32_t key1) const;

    template <typename T>
    void write(int32_t key1, int32_t key2, const T &val) {
        std::lock_guard<std::mutex> lock(mutex);

        size_t size = sizeof(T);
        if (size == 0)
            return;

        HANDLE old_handle = get_shared_mem_handle(key1, key2);
        HANDLE new_handle = nullptr;
        bool is_newly_created = false;

        if (old_handle == nullptr) {
            new_handle = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, nullptr);
            if (new_handle == nullptr)
                return;

            is_newly_created = true;
        } else {
            new_handle = old_handle;
        }

        T *ptr = static_cast<T *>(MapViewOfFile(new_handle, FILE_MAP_ALL_ACCESS, 0, 0, size));
        if (ptr == nullptr) {
            if (is_newly_created)
                CloseHandle(new_handle);

            return;
        }

        std::memcpy(ptr, &val, size);
        UnmapViewOfFile(ptr);

        if (is_newly_created)
            set_shared_mem_handle(key1, key2, new_handle);
    }

    template <typename T>
    bool read(int32_t key1, int32_t key2, T &val) const {
        std::lock_guard<std::mutex> lock(mutex);

        size_t size = sizeof(T);
        if (size == 0)
            return false;

        HANDLE handle = get_shared_mem_handle(key1, key2);
        if (handle == nullptr)
            return false;

        T *ptr = static_cast<T *>(MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, size));
        if (ptr == nullptr) {
            return false;
        }

        std::memcpy(&val, ptr, size);
        UnmapViewOfFile(ptr);

        return true;
    }

private:
    static std::mutex mutex;

    std::unordered_map<int32_t, std::map<int32_t, HANDLE>> handle_map;

    HANDLE get_shared_mem_handle(int32_t key1, int32_t key2) const;
    void set_shared_mem_handle(int32_t key1, int32_t key2, HANDLE handle);
    void cleanup_all_handle_impl();
};
