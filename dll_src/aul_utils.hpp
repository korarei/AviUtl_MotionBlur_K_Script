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
    // Constructors.
    AulMemory();
    AulMemory(const AulMemory &) = delete;
    AulMemory &operator=(const AulMemory &) = delete;

protected:
    using GetCurrProcFn = ExEdit::ObjectFilterIndex(__cdecl *)(ExEdit::FilterProcInfo *);

    ExEdit::Filter *efp;
    ExEdit::FilterProcInfo *efpip;
    ExEdit::Filter **loaded_filter_table;
    std::int32_t camera_mode;
    bool is_saving;
    GetCurrProcFn get_curr_proc;

private:
    struct Offsets {
        static constexpr std::uintptr_t Version = 0x4d726;
        static constexpr std::uintptr_t ScriptEfp = 0x1b2b10;
        static constexpr std::uintptr_t ScriptEfpip = 0x1b2b20;
        static constexpr std::uintptr_t LoadedFilterTable = 0x187c98;
        static constexpr std::uintptr_t GetCurrProc = 0x047ba0;
        static constexpr std::uintptr_t CameraMode = 0x013596c;
        static constexpr std::uintptr_t IsSaving = 0x1a52e4;
    };

    [[nodiscard]] std::uintptr_t get_exedit_base() const;
    [[nodiscard]] bool check_exedit_version(std::uintptr_t exedit_base) const noexcept;
    [[nodiscard]] ExEdit::Filter *get_exedit_filter_ptr(std::uintptr_t exedit_base) const noexcept;
    [[nodiscard]] ExEdit::FilterProcInfo *get_exedit_filter_proc_info_ptr(std::uintptr_t exedit_base) const noexcept;
    [[nodiscard]] ExEdit::Filter **get_loaded_filter_table(std::uintptr_t exedit_base) const noexcept;
    [[nodiscard]] GetCurrProcFn get_get_curr_proc(std::uintptr_t exedit_base) const noexcept;
    [[nodiscard]] std::int32_t get_camera_mode(std::uintptr_t exedit_base) const noexcept;
    [[nodiscard]] std::int32_t get_is_saving(std::uintptr_t exedit_base) const noexcept;
};

class ObjectUtils : public AulMemory {
public:
    using Geo = ExEdit::FilterProcInfo::Geometry;
    using ObjFilterIdx = ExEdit::ObjectFilterIndex;

    // Constructors.
    ObjectUtils();
    ObjectUtils(const ObjectUtils &) = delete;
    ObjectUtils &operator=(const ObjectUtils &) = delete;

    [[nodiscard]] constexpr const std::int32_t &get_frame_begin() const noexcept { return efpip->objectp->frame_begin; }
    [[nodiscard]] constexpr const std::int32_t &get_frame_end() const noexcept { return efpip->objectp->frame_end; }
    [[nodiscard]] constexpr const std::int32_t &get_frame_num() const noexcept { return efpip->frame_num; }
    [[nodiscard]] constexpr const std::int32_t &get_local_frame() const noexcept { return local_frame; }
    [[nodiscard]] constexpr const std::int32_t &get_obj_w() const noexcept { return efpip->obj_w; }
    [[nodiscard]] constexpr const std::int32_t &get_obj_h() const noexcept { return efpip->obj_h; }
    [[nodiscard]] constexpr const Geo &get_obj_data() const noexcept { return efpip->obj_data; }
    [[nodiscard]] constexpr const bool &get_is_saving() const noexcept { return is_saving; }
    [[nodiscard]] constexpr const std::uint16_t &get_curr_object_idx() const noexcept { return curr_object_idx; }
    [[nodiscard]] constexpr const std::int32_t &get_obj_index() const noexcept { return efpip->obj_index; }
    [[nodiscard]] constexpr const std::int32_t &get_obj_num() const noexcept { return efpip->obj_num; }
    [[nodiscard]] constexpr const std::int32_t &get_camera_mode() const noexcept { return camera_mode; }
    [[nodiscard]] constexpr const std::int32_t &get_max_w() const noexcept { return max_w; }
    [[nodiscard]] constexpr const std::int32_t &get_max_h() const noexcept { return max_h; }

    constexpr void set_obj_w(std::int32_t w) noexcept { efpip->obj_w = std::clamp(w, 0, max_w); }
    constexpr void set_obj_h(std::int32_t h) noexcept { efpip->obj_h = std::clamp(h, 0, max_h); }

    [[nodiscard]] static constexpr ObjFilterIdx create_ofi(std::uint16_t object_idx, std::uint16_t filter_idx) noexcept;
    [[nodiscard]] static constexpr float calc_ox(std::int32_t ox) noexcept;
    [[nodiscard]] static constexpr float calc_oy(std::int32_t oy) noexcept;
    [[nodiscard]] static constexpr float calc_zoom(std::int32_t zoom) noexcept;
    [[nodiscard]] static constexpr float calc_cx(std::int32_t cx, float base_cx = 0.0f) noexcept;
    [[nodiscard]] static constexpr float calc_cy(std::int32_t cy, float base_cy = 0.0f) noexcept;
    [[nodiscard]] static float calc_rz(std::int32_t rz, float base_angle = 0.0f) noexcept;

    [[nodiscard]] float get_cx(std::optional<std::int32_t> cx = std::nullopt, std::int32_t offset_frame = 0,
                               OffsetType offset_type = OffsetType::Current) const noexcept;
    [[nodiscard]] float get_cy(std::optional<std::int32_t> cy = std::nullopt, std::int32_t offset_frame = 0,
                               OffsetType offset_type = OffsetType::Current) const noexcept;
    [[nodiscard]] float get_rz(std::optional<std::int32_t> rz = std::nullopt, std::int32_t offset_frame = 0,
                               OffsetType offset_type = OffsetType::Current) const noexcept;
    [[nodiscard]] float calc_track_val(TrackName track_name, std::int32_t offset_frame = 0,
                                       OffsetType offset_type = OffsetType::Current) const;

private:
    ObjFilterIdx curr_ofi;
    std::uint16_t curr_object_idx;
    std::uint16_t curr_filter_idx;
    std::int32_t local_frame;
    std::int32_t max_w;
    std::int32_t max_h;
};

// Functions of AviUtl Memory class.
inline bool
AulMemory::check_exedit_version(std::uintptr_t exedit_base) const noexcept {
    std::int32_t version = *reinterpret_cast<std::int32_t *>(exedit_base + Offsets::Version);
    return version == 9200;
}

inline ExEdit::Filter *
AulMemory::get_exedit_filter_ptr(std::uintptr_t exedit_base) const noexcept {
    ExEdit::Filter **efpp = reinterpret_cast<ExEdit::Filter **>(exedit_base + Offsets::ScriptEfp);
    if (efpp && *efpp)
        return *efpp;
    return nullptr;
}

inline ExEdit::FilterProcInfo *
AulMemory::get_exedit_filter_proc_info_ptr(std::uintptr_t exedit_base) const noexcept {
    ExEdit::FilterProcInfo **efpipp = reinterpret_cast<ExEdit::FilterProcInfo **>(exedit_base + Offsets::ScriptEfpip);
    if (efpipp && *efpipp)
        return *efpipp;
    return nullptr;
}

inline ExEdit::Filter **
AulMemory::get_loaded_filter_table(std::uintptr_t exedit_base) const noexcept {
    return reinterpret_cast<ExEdit::Filter **>(exedit_base + Offsets::LoadedFilterTable);
}

inline AulMemory::GetCurrProcFn
AulMemory::get_get_curr_proc(std::uintptr_t exedit_base) const noexcept {
    return reinterpret_cast<GetCurrProcFn>(exedit_base + Offsets::GetCurrProc);
}

inline std::int32_t
AulMemory::get_camera_mode(std::uintptr_t exedit_base) const noexcept {
    return *reinterpret_cast<std::int32_t *>(exedit_base + Offsets::CameraMode);
}

inline std::int32_t
AulMemory::get_is_saving(std::uintptr_t exedit_base) const noexcept {
    return *reinterpret_cast<std::int32_t *>(exedit_base + Offsets::IsSaving);
}

// Functions of Object Utilities class.
// Create an object filter index from object index and filter index.
inline constexpr ObjectUtils::ObjFilterIdx
ObjectUtils::create_ofi(std::uint16_t object_idx, std::uint16_t filter_idx) noexcept {
    const auto filter_part = static_cast<std::uint32_t>(filter_idx) << 16;
    const auto object_part = static_cast<std::uint32_t>(object_idx);
    return static_cast<ObjFilterIdx>(filter_part | object_part);
}

// calculate the geometry.
// This fixed-point precision provides one more decimal digit than the trackbar,
// ensuring accurate internal computation before mapping to UI resolution.
inline constexpr float
ObjectUtils::calc_ox(std::int32_t ox) noexcept {
    return static_cast<float>((static_cast<int64_t>(ox) * 100) >> 12) * 1e-2f;
}

inline constexpr float
ObjectUtils::calc_oy(std::int32_t oy) noexcept {
    return static_cast<float>((static_cast<int64_t>(oy) * 100) >> 12) * 1e-2f;
}

inline constexpr float
ObjectUtils::calc_zoom(std::int32_t zoom) noexcept {
    return static_cast<float>((static_cast<int64_t>(zoom) * 1000) >> 16) * 1e-3f;
}

inline constexpr float
ObjectUtils::calc_cx(std::int32_t cx, float base_cx) noexcept {
    return base_cx + calc_oy(cx);
}

inline constexpr float
ObjectUtils::calc_cy(std::int32_t cy, float base_cy) noexcept {
    return base_cy + calc_oy(cy);
}

inline float
ObjectUtils::calc_rz(std::int32_t rz, float base_angle) noexcept {
    return std::fmodf(base_angle, 360.0f) + static_cast<float>((static_cast<int64_t>(rz) * 360 * 1000) >> 16) * 1e-3f;
}

inline float
ObjectUtils::get_cx(std::optional<std::int32_t> cx, std::int32_t offset_frame, OffsetType offset_type) const noexcept {
    float base_cx = calc_track_val(TrackName::CenterX, offset_frame, offset_type);
    return calc_cx(cx.value_or(efpip->obj_data.cx), base_cx);
}

inline float
ObjectUtils::get_cy(std::optional<std::int32_t> cy, std::int32_t offset_frame, OffsetType offset_type) const noexcept {
    float base_cy = calc_track_val(TrackName::CenterY, offset_frame, offset_type);
    return calc_cy(cy.value_or(efpip->obj_data.cy), base_cy);
}

inline float
ObjectUtils::get_rz(std::optional<std::int32_t> rz, std::int32_t offset_frame, OffsetType offset_type) const noexcept {
    float base_angle = calc_track_val(TrackName::RotationZ, offset_frame, offset_type);
    return calc_rz(rz.value_or(efpip->obj_data.rz), base_angle);
}
