#include <tchar.h>
#include <mutex>
#include <vector>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "utils.hpp"

namespace {
constexpr DWORD INITIAL_BUFFER_SIZE = MAX_PATH;
constexpr DWORD MAX_BUFFER_SIZE = 32768;
constexpr DWORD BUFFER_GROWTH_FACTOR = 2;

const std::filesystem::path DEFAULT_DIR = _T("C:\\aviutl\\script");
}  // namespace

const std::filesystem::path &
get_self_dir() {
    static std::filesystem::path cached_dir;
    static std::once_flag initialized;

    std::call_once(initialized, []() {
        HMODULE handle = nullptr;

        // Get the address of this function.
        if (!::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                 reinterpret_cast<LPCTSTR>(get_self_dir), &handle)) {
            cached_dir = DEFAULT_DIR;
            return;
        }

        // Get the full path of the module.
        std::filesystem::path full_path;
        for (DWORD buffer_size = INITIAL_BUFFER_SIZE; buffer_size <= MAX_BUFFER_SIZE;
             buffer_size *= BUFFER_GROWTH_FACTOR) {
            std::vector<TCHAR> buffer(buffer_size);
            const DWORD result = ::GetModuleFileName(handle, buffer.data(), buffer_size);

            if (result == 0)
                break;

            if (result < buffer_size) {
                full_path = std::filesystem::path(buffer.data(), buffer.data() + result);
                break;
            }
        }

        if (full_path.empty()) {
            cached_dir = DEFAULT_DIR;
            return;
        }

        // Extract the directory from the full path.
        try {
            cached_dir = full_path.parent_path();
        } catch (const std::exception &) {
            cached_dir = DEFAULT_DIR;
        }
    });

    return cached_dir;
}
