#include <vector>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "utils.hpp"

const std::string &
get_self_dir() {
    static std::string cached_dir;

    if (!cached_dir.empty())
        return cached_dir;

    HMODULE handle = NULL;
    const auto func_addr = reinterpret_cast<LPCSTR>(&get_self_dir);
    std::string default_dir = "C:\\aviutl\\script";

    if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, func_addr, &handle)) {
        cached_dir = default_dir;
        return cached_dir;
    }

    DWORD buffer_size = MAX_PATH;
    std::string full_path;

    while (buffer_size <= 32768) {
        std::vector<char> buffer(buffer_size);
        DWORD result = ::GetModuleFileNameA(handle, buffer.data(), buffer_size);

        if (result == 0)
            break;

        if (result < buffer_size) {
            full_path = std::string(buffer.data(), result);
            break;
        }

        buffer_size *= 2;
    }

    if (full_path.empty()) {
        cached_dir = default_dir;
        return cached_dir;
    }

    size_t pos = full_path.find_last_of("\\/");
    if (pos != std::string::npos) {
        cached_dir = full_path.substr(0, pos);
    } else {
        cached_dir = full_path;
    }

    return cached_dir;
}

bool
file_exists(const std::string &file_path) {
    DWORD flags = ::GetFileAttributesA(file_path.c_str());
    return (flags != INVALID_FILE_ATTRIBUTES && !(flags & FILE_ATTRIBUTE_DIRECTORY));
}
