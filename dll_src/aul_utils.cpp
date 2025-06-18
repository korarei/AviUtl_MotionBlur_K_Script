#include <tchar.h>
#include <cstring>
#include <stdexcept>
#define NOMINMAX
#include <Windows.h>

#include "aul_utils.hpp"

AulMemory::AulMemory() : efp(nullptr), efpip(nullptr), loaded_filter_table(nullptr), camera_mode(-1), is_saving(false) {
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

    get_curr_proc = get_get_curr_proc(exedit_base);
    if (!get_curr_proc)
        throw std::runtime_error("Failed to retrieve get current processing.");

    camera_mode = get_camera_mode(exedit_base);
    if (camera_mode < 0)
        throw std::runtime_error("Failed to retrieve camera mode.");

    int32_t raw_saving_flag = get_is_saving(exedit_base);
    if ((raw_saving_flag & ~1) != 0)
        throw std::runtime_error("Failed to retrieve is saving status.");

    is_saving = (raw_saving_flag & 1) != 0;
}

uintptr_t
AulMemory::get_exedit_base() const {
    static HMODULE handle = []() -> HMODULE {
        HMODULE h = ::GetModuleHandle(_T("exedit.auf"));
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
    AulMemory(),
    curr_ofi(efpip ? get_curr_proc(efpip) : create_object_filter_index(0, 0)),
    curr_object_idx(ExEdit::object(curr_ofi)),
    curr_filter_idx(ExEdit::filter(curr_ofi)),
    local_frame(efpip ? efpip->frame_num - efpip->objectp->frame_begin : 0) {
    AviUtl::SysInfo sys_info;
    efp->aviutl_exfunc->get_sys_info(nullptr, &sys_info);
    max_w = sys_info.max_w;
    max_h = sys_info.max_h;

    if (sys_info.build != 11003) {
        throw std::runtime_error("AviUtl v1.10 is required.");
    }
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
