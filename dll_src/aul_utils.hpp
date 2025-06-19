#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#define NOMINMAX
#include <exedit.hpp>

enum class TrackName : int {
    X,
    Y,
    Zoom,
    RotationZ,
    CenterX,
    CenterY
};

enum class OffsetType : int {
    Start,
    Current
};

class AulMemory {
public:
    AulMemory();

    AulMemory(const AulMemory &) = delete;
    AulMemory &operator=(const AulMemory &) = delete;

protected:
    ExEdit::Filter *efp;
    ExEdit::FilterProcInfo *efpip;
    ExEdit::Filter **loaded_filter_table;
    int32_t camera_mode;
    bool is_saving;

    using GetCurrentProcessing = ExEdit::ObjectFilterIndex(__cdecl *)(ExEdit::FilterProcInfo *);
    GetCurrentProcessing get_curr_proc;

private:
    static constexpr uintptr_t VERSION_OFFSET = 0x4d726;
    static constexpr uintptr_t EFPP_OFFSET = 0x1b2b10;
    static constexpr uintptr_t EFPIPP_OFFSET = 0x1b2b20;
    static constexpr uintptr_t LOADED_FILTER_TABLE_OFFSET = 0x187c98;
    static constexpr uintptr_t GET_CURR_PROC_OFFSET = 0x047ba0;
    static constexpr uintptr_t CAMERA_MODE_OFFSET = 0x013596c;
    static constexpr uintptr_t IS_SAVING_OFFSET = 0x1a52e4;

    [[nodiscard]] uintptr_t get_exedit_base() const;
    [[nodiscard]] bool check_exedit_version(uintptr_t exedit_base) const noexcept;
    [[nodiscard]] ExEdit::Filter *get_exedit_filter_ptr(uintptr_t exedit_base) const noexcept;
    [[nodiscard]] ExEdit::FilterProcInfo *get_exedit_filter_proc_info_ptr(uintptr_t exedit_base) const noexcept;
    [[nodiscard]] ExEdit::Filter **get_loaded_filter_table(uintptr_t exedit_base) const noexcept;
    [[nodiscard]] GetCurrentProcessing get_get_curr_proc(uintptr_t exedit_base) const noexcept;
    [[nodiscard]] int32_t get_camera_mode(uintptr_t exedit_base) const noexcept;
    [[nodiscard]] int32_t get_is_saving(uintptr_t exedit_base) const noexcept;
};

class ObjectUtils : public AulMemory {
public:
    ObjectUtils();

    ObjectUtils(const ObjectUtils &) = delete;
    ObjectUtils &operator=(const ObjectUtils &) = delete;

    [[nodiscard]] constexpr int32_t get_frame_begin() const noexcept;
    [[nodiscard]] constexpr int32_t get_frame_end() const noexcept;
    [[nodiscard]] constexpr int32_t get_frame_num() const noexcept;
    [[nodiscard]] constexpr int32_t get_local_frame() const noexcept;
    [[nodiscard]] constexpr int32_t get_obj_w() const noexcept;
    [[nodiscard]] constexpr int32_t get_obj_h() const noexcept;
    [[nodiscard]] constexpr const ExEdit::FilterProcInfo::Geometry &get_obj_data() const noexcept;
    [[nodiscard]] constexpr bool get_is_saving() const noexcept;
    [[nodiscard]] constexpr uint16_t get_curr_object_idx() const noexcept;
    [[nodiscard]] constexpr int32_t get_obj_index() const noexcept;
    [[nodiscard]] constexpr int32_t get_obj_num() const noexcept;
    [[nodiscard]] constexpr int32_t get_camera_mode() const noexcept;
    [[nodiscard]] constexpr int32_t get_max_w() const noexcept;
    [[nodiscard]] constexpr int32_t get_max_h() const noexcept;

    constexpr void set_obj_w(int32_t w) noexcept;
    constexpr void set_obj_h(int32_t h) noexcept;

    [[nodiscard]] static constexpr ExEdit::ObjectFilterIndex create_ofi(uint16_t object_idx,
                                                                        uint16_t filter_idx) noexcept;
    [[nodiscard]] static constexpr float calc_ox(int32_t ox) noexcept;
    [[nodiscard]] static constexpr float calc_oy(int32_t oy) noexcept;
    [[nodiscard]] static constexpr float calc_zoom(int32_t zoom) noexcept;
    [[nodiscard]] static constexpr float calc_cx(int32_t cx, float base_cx = 0.0f) noexcept;
    [[nodiscard]] static constexpr float calc_cy(int32_t cy, float base_cy = 0.0f) noexcept;
    [[nodiscard]] static float calc_rz(int32_t rz, float base_angle = 0.0f) noexcept;

    [[nodiscard]] float get_cx(std::optional<int32_t> cx = std::nullopt, int32_t offset_frame = 0,
                               OffsetType offset_type = OffsetType::Current) const noexcept;
    [[nodiscard]] float get_cy(std::optional<int32_t> cy = std::nullopt, int32_t offset_frame = 0,
                               OffsetType offset_type = OffsetType::Current) const noexcept;
    [[nodiscard]] float get_rz(std::optional<int32_t> rz = std::nullopt, int32_t offset_frame = 0,
                               OffsetType offset_type = OffsetType::Current) const noexcept;
    [[nodiscard]] float calc_track_val(TrackName track_name, int32_t offset_frame = 0,
                                       OffsetType offset_type = OffsetType::Current) const;

private:
    ExEdit::ObjectFilterIndex curr_ofi;
    uint16_t curr_object_idx;
    uint16_t curr_filter_idx;
    int32_t local_frame;
    int32_t max_w;
    int32_t max_h;
};

// Functions of AviUtl Memory class.
inline bool
AulMemory::check_exedit_version(uintptr_t exedit_base) const noexcept {
    int32_t version = *reinterpret_cast<int32_t *>(exedit_base + VERSION_OFFSET);
    return version == 9200;
}

inline ExEdit::Filter *
AulMemory::get_exedit_filter_ptr(uintptr_t exedit_base) const noexcept {
    ExEdit::Filter **efpp = reinterpret_cast<ExEdit::Filter **>(exedit_base + EFPP_OFFSET);
    if (efpp && *efpp)
        return *efpp;
    return nullptr;
}

inline ExEdit::FilterProcInfo *
AulMemory::get_exedit_filter_proc_info_ptr(uintptr_t exedit_base) const noexcept {
    ExEdit::FilterProcInfo **efpipp = reinterpret_cast<ExEdit::FilterProcInfo **>(exedit_base + EFPIPP_OFFSET);
    if (efpipp && *efpipp)
        return *efpipp;
    return nullptr;
}

inline ExEdit::Filter **
AulMemory::get_loaded_filter_table(uintptr_t exedit_base) const noexcept {
    return reinterpret_cast<ExEdit::Filter **>(exedit_base + LOADED_FILTER_TABLE_OFFSET);
}

inline AulMemory::GetCurrentProcessing
AulMemory::get_get_curr_proc(uintptr_t exedit_base) const noexcept {
    return reinterpret_cast<GetCurrentProcessing>(exedit_base + GET_CURR_PROC_OFFSET);
}

inline int32_t
AulMemory::get_camera_mode(uintptr_t exedit_base) const noexcept {
    return *reinterpret_cast<int32_t *>(exedit_base + CAMERA_MODE_OFFSET);
}

inline int32_t
AulMemory::get_is_saving(uintptr_t exedit_base) const noexcept {
    return *reinterpret_cast<int32_t *>(exedit_base + IS_SAVING_OFFSET);
}

// Functions of Object Utilities class.
inline constexpr int32_t
ObjectUtils::get_frame_begin() const noexcept {
    return efpip->objectp->frame_begin;
}

inline constexpr int32_t
ObjectUtils::get_frame_end() const noexcept {
    return efpip->objectp->frame_end;
}

inline constexpr int32_t
ObjectUtils::get_frame_num() const noexcept {
    return efpip->frame_num;
}

inline constexpr int32_t
ObjectUtils::get_local_frame() const noexcept {
    return local_frame;
}

inline constexpr int32_t
ObjectUtils::get_obj_w() const noexcept {
    return efpip->obj_w;
}

inline constexpr int32_t
ObjectUtils::get_obj_h() const noexcept {
    return efpip->obj_h;
}

inline constexpr const ExEdit::FilterProcInfo::Geometry &
ObjectUtils::get_obj_data() const noexcept {
    return efpip->obj_data;
}

inline constexpr bool
ObjectUtils::get_is_saving() const noexcept {
    return is_saving;
}

inline constexpr uint16_t
ObjectUtils::get_curr_object_idx() const noexcept {
    return curr_object_idx;
}

inline constexpr int32_t
ObjectUtils::get_obj_index() const noexcept {
    return efpip->obj_index;
}

inline constexpr int32_t
ObjectUtils::get_obj_num() const noexcept {
    return efpip->obj_num;
}

inline constexpr int32_t
ObjectUtils::get_camera_mode() const noexcept {
    return camera_mode;
}

inline constexpr int32_t
ObjectUtils::get_max_w() const noexcept {
    return max_w;
}

inline constexpr int32_t
ObjectUtils::get_max_h() const noexcept {
    return max_h;
}

// Object Utilities Setters.
inline constexpr void
ObjectUtils::set_obj_w(int32_t w) noexcept {
    efpip->obj_w = std::clamp(w, 0, max_w);
}

inline constexpr void
ObjectUtils::set_obj_h(int32_t h) noexcept {
    efpip->obj_h = std::clamp(h, 0, max_h);
}

// calculate the geometry.
// This fixed-point precision provides one more decimal digit than the trackbar,
// ensuring accurate internal computation before mapping to UI resolution.
inline constexpr float
ObjectUtils::calc_ox(int32_t ox) noexcept {
    return static_cast<float>((static_cast<int64_t>(ox) * 100) >> 12) * 1e-2f;
}

inline constexpr float
ObjectUtils::calc_oy(int32_t oy) noexcept {
    return static_cast<float>((static_cast<int64_t>(oy) * 100) >> 12) * 1e-2f;
}

inline constexpr float
ObjectUtils::calc_zoom(int32_t zoom) noexcept {
    return static_cast<float>((static_cast<int64_t>(zoom) * 1000) >> 16) * 1e-3f;
}

inline constexpr float
ObjectUtils::calc_cx(int32_t cx, float base_cx) noexcept {
    return base_cx + calc_oy(cx);
}

inline constexpr float
ObjectUtils::calc_cy(int32_t cy, float base_cy) noexcept {
    return base_cy + calc_oy(cy);
}

inline float
ObjectUtils::calc_rz(int32_t rz, float base_angle) noexcept {
    return std::fmod(base_angle, 360.0f) + static_cast<float>((static_cast<int64_t>(rz) * 360 * 1000) >> 16) * 1e-3f;
}

inline float
ObjectUtils::get_cx(std::optional<int32_t> cx, int32_t offset_frame, OffsetType offset_type) const noexcept {
    float base_cx = calc_track_val(TrackName::CenterX, offset_frame, offset_type);
    return calc_cx(cx.value_or(efpip->obj_data.cx), base_cx);
}

inline float
ObjectUtils::get_cy(std::optional<int32_t> cy, int32_t offset_frame, OffsetType offset_type) const noexcept {
    float base_cy = calc_track_val(TrackName::CenterY, offset_frame, offset_type);
    return calc_cy(cy.value_or(efpip->obj_data.cy), base_cy);
}

inline float
ObjectUtils::get_rz(std::optional<int32_t> rz, int32_t offset_frame, OffsetType offset_type) const noexcept {
    float base_angle = calc_track_val(TrackName::RotationZ, offset_frame, offset_type);
    return calc_rz(rz.value_or(efpip->obj_data.rz), base_angle);
}

// Create an object filter index from object index and filter index.
inline constexpr ExEdit::ObjectFilterIndex
ObjectUtils::create_ofi(uint16_t object_idx, uint16_t filter_idx) noexcept {
    const auto filter_part = static_cast<std::uint32_t>(filter_idx) << 16;
    const auto object_part = static_cast<std::uint32_t>(object_idx);
    return static_cast<ExEdit::ObjectFilterIndex>(filter_part | object_part);
}
