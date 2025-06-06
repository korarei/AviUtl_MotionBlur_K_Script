#include <cstring>
#include <ranges>
#include <stdexcept>
#define NOMINMAX
#include <Windows.h>
#include <bitset>
#include <iostream>

#include "aul_utils.hpp"

AulPtrs::AulPtrs() : efp(nullptr), efpip(nullptr), loaded_filter_table(nullptr), camera_mode(-1) {
    static uintptr_t exedit_base = get_exedit_base();

    efp = get_exedit_filter_ptr(exedit_base);
    if (!efp)
        throw std::runtime_error("Failed to retrieve ExEdit filter pointer.");

    efpip = get_exedit_filter_proc_info_ptr(exedit_base);
    if (!efpip)
        throw std::runtime_error("Failed to retrieve ExEdit filter Proc info pointer.");

    loaded_filter_table = get_loaded_filter_table(exedit_base);
    if (!loaded_filter_table)
        throw std::runtime_error("Failed to retrieve loaded filter table.");

    camera_mode = get_camera_mode(exedit_base);
    if (camera_mode < 0)
        throw std::runtime_error("Invalid camera mode.");

    get_curr_proc = get_get_curr_proc(exedit_base);
    if (!get_curr_proc)
        throw std::runtime_error("Failed to retrieve get current processing.");
}

uintptr_t
AulPtrs::get_exedit_base() const {
    static HMODULE handle = []() -> HMODULE {
        HMODULE h = ::GetModuleHandleA("exedit.auf");
        if (!h)
            throw std::runtime_error("Failed to get ExEdit module handle.");
        return h;
    }();

    uintptr_t base = reinterpret_cast<uintptr_t>(handle);

    static uintptr_t exedit_base = [base, this]() -> uintptr_t {
        if (!check_exedit_version(base))
            throw std::runtime_error("ExEdit (exedit.auf) v0.92 is required.");
        return base;
    }();

    return exedit_base;
}

ObjectUtils::ObjectUtils() :
    AulPtrs(),
    local_frame(efpip ? efpip->frame_num - efpip->objectp->frame_begin : 0),
    is_saving(efp && efpip ? efp->aviutl_exfunc->is_saving(static_cast<AviUtl::EditHandle *>(efpip->editp)) : false),
    curr_ofi(efpip ? get_curr_proc(efpip) : create_object_filter_index(0, 0)),
    curr_object_idx(ExEdit::object(curr_ofi)),
    curr_filter_idx(ExEdit::filter(curr_ofi)) {
    AviUtl::SysInfo sys_info;
    efp->aviutl_exfunc->get_sys_info(nullptr, &sys_info);
    max_w = sys_info.max_w;
    max_h = sys_info.max_h;

    if (sys_info.build != 11003) {
        throw std::runtime_error("AviUtl v1.10 is required.");
    }
}

bool
ObjectUtils::create_shared_mem_impl(int32_t key1, int32_t key2, AviUtl::SharedMemoryInfo **handle_ptr, int32_t size,
                                    const void *val) const {
    if (!efp)
        throw std::runtime_error("ExEdit filter pointer is null.");

    void *ptr = efp->aviutl_exfunc->create_shared_mem(key1, key2, size, handle_ptr);
    if (ptr != nullptr && val != nullptr) {
        std::memcpy(ptr, val, size);

        return true;
    }

    return false;
}

bool
ObjectUtils::get_shared_mem_impl(int32_t key1, int32_t key2, AviUtl::SharedMemoryInfo *handle, int32_t size,
                                 void *val) const {
    if (!efp)
        throw std::runtime_error("ExEdit filter pointer is null.");

    void *ptr = efp->aviutl_exfunc->get_shared_mem(key1, key2, handle);
    if (ptr == nullptr || val == nullptr)
        return false;

    std::memcpy(val, ptr, size);

    return true;
}

void
ObjectUtils::delete_shared_mem(int32_t key1, AviUtl::SharedMemoryInfo *handle) const {
    if (!efp)
        throw std::runtime_error("ExEdit filter pointer is null.");

    efp->aviutl_exfunc->delete_shared_mem(key1, handle);
}

float
ObjectUtils::calc_track_val(TrackName track_name, int32_t offset_frame, OffsetType offset_type) const {
    if (!ExEdit::is_valid(curr_ofi))
        return 0.0f;

    auto curr_proc_efp = loaded_filter_table[efpip->objectp->filter_param[curr_filter_idx].id];
    if (!curr_proc_efp->track_gui)
        return 0.0f;

    int32_t val;
    int32_t frame = offset_type == OffsetType::Current
                          ? std::clamp(efpip->frame_num + offset_frame, efpip->objectp->frame_begin,
                                       efpip->objectp->frame_end)
                          : std::clamp(efpip->objectp->frame_begin + offset_frame, efpip->objectp->frame_begin,
                                       efpip->objectp->frame_end);
    int track_idx = -1;
    float track_denom = 1e-1f;

    switch (track_name) {
        case TrackName::X:
            track_idx = curr_proc_efp->track_gui->bx;
            track_denom = 1e-1f;
            break;
        case TrackName::Y:
            track_idx = curr_proc_efp->track_gui->by;
            track_denom = 1e-1f;
            break;
        case TrackName::Zoom:
            track_idx = curr_proc_efp->track_gui->zoom;
            track_denom = 1e-2f;
            break;
        case TrackName::RotationZ:
            track_idx = curr_proc_efp->track_gui->rz;
            track_denom = 1e-2f;
            break;
        case TrackName::CenterX:
            track_idx = curr_proc_efp->track_gui->cx;
            track_denom = 1e-1f;
            break;
        case TrackName::CenterY:
            track_idx = curr_proc_efp->track_gui->cy;
            track_denom = 1e-1f;
            break;
        default:
            throw std::invalid_argument("Invalid track name.");
    }

    if (track_idx < 0)  // efp->track_gui->invalid == -1
        return 0.0f;

    if (efp->exfunc->calc_trackbar(curr_ofi, frame, 0, &val, reinterpret_cast<char *>(1 + track_idx))) {
        return static_cast<float>(val) * track_denom;
    } else {
        return 0.0f;
    }
}

void
ObjectUtils::expand_image(const std::array<int32_t, 4> &expansion) {
    constexpr int32_t fill_flag = 2;              // has_alpha
    constexpr int32_t buf_cpy_flag = 0x13000003;  // Base (0x13000000) | dst_has_alpha (2) | src_has_alpha (1)

    constexpr auto has_neg = [](const std::array<int32_t, 4> &vals) -> bool {
        return std::ranges::any_of(vals, [](int32_t x) { return x < 0; });
    };

    if (has_neg(expansion))
        return;

    auto [top, bottom, left, right] = expansion;
    int32_t w = efpip->obj_w;
    int32_t h = efpip->obj_h;
    set_obj_w(efpip->obj_w + right + left);
    set_obj_h(efpip->obj_h + top + bottom);

    if (left == 0 && right == 0) {
        if (top > 0)
            efp->exfunc->fill(efpip->obj_temp, 0, 0, w, top, 0, 0, 0, 0, fill_flag);

        if (bottom > 0)
            efp->exfunc->fill(efpip->obj_temp, 0, efpip->obj_h - bottom, w, bottom, 0, 0, 0, 0, fill_flag);
    } else if (top == 0 && bottom == 0) {
        if (left > 0)
            efp->exfunc->fill(efpip->obj_temp, 0, 0, left, h, 0, 0, 0, 0, fill_flag);

        if (right > 0)
            efp->exfunc->fill(efpip->obj_temp, efpip->obj_w - right, 0, right, h, 0, 0, 0, 0, fill_flag);
    } else {
        efp->exfunc->fill(efpip->obj_temp, 0, 0, efpip->obj_w, efpip->obj_h, 0, 0, 0, 0, fill_flag);
    }

    efp->exfunc->bufcpy(efpip->obj_temp, left, top, efpip->obj_edit, 0, 0, w, h, 0, buf_cpy_flag);

    std::swap(efpip->obj_edit, efpip->obj_temp);

    // Investigating the cause of malfunction.
    //efpip->obj_data.cx += (left - right) << 11;
    //efpip->obj_data.cy += (top - bottom) << 11;
}
