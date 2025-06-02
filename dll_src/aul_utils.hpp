#pragma once

#include <cmath>
#include <cstdint>
#include <optional>
#include <algorithm>

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

class AulPtrs {
public:
    AulPtrs();

    AulPtrs(const AulPtrs &) = delete;
    AulPtrs &operator=(const AulPtrs &) = delete;

protected:
    ExEdit::Filter *efp;
    ExEdit::FilterProcInfo *efpip;
    ExEdit::Filter **loaded_filter_table;
    int32_t camera_mode;

    using GetCurrentProcessing = ExEdit::ObjectFilterIndex(__cdecl *)(ExEdit::FilterProcInfo *);
    GetCurrentProcessing get_curr_proc;

private:
    static constexpr uintptr_t VERSION_OFFSET = 0x4d726;
    static constexpr uintptr_t EFPP_OFFSET = 0x1b2b10;
    static constexpr uintptr_t EFPIPP_OFFSET = 0x1b2b20;
    static constexpr uintptr_t GET_CURR_PROC_OFFSET = 0x047ba0;
    static constexpr uintptr_t LOADED_FILTER_TABLE_OFFSET = 0x187c98;
    static constexpr uintptr_t CAMERA_MODE_OFFSET = 0x013596c;

    uintptr_t get_exedit_base() const;
    bool check_exedit_version(uintptr_t exedit_base) const;
    ExEdit::Filter *get_exedit_filter_ptr(uintptr_t exedit_base) const;
    ExEdit::FilterProcInfo *get_exedit_filter_proc_info_ptr(uintptr_t exedit_base) const;
    ExEdit::Filter **get_loaded_filter_table(uintptr_t exedit_base) const;
    int32_t get_camera_mode(uintptr_t exedit_base) const;
    GetCurrentProcessing get_get_curr_proc(uintptr_t exedit_base) const;
};

class ObjectUtils : public AulPtrs {
public:
    ObjectUtils();

    ObjectUtils(const ObjectUtils &) = delete;
    ObjectUtils &operator=(const ObjectUtils &) = delete;

    int32_t get_frame_begin() const;
    int32_t get_frame_end() const;
    int32_t get_frame_num() const;
    int32_t get_local_frame() const;
    int32_t get_obj_w() const;
    int32_t get_obj_h() const;
    const ExEdit::FilterProcInfo::Geometry &get_obj_data() const;
    bool get_is_saving() const;
    uint16_t get_object_id() const;
    int32_t get_obj_index() const;
    int32_t get_obj_num() const;
    int32_t get_camera_mode() const;
    int32_t get_max_w() const;
    int32_t get_max_h() const;

    void set_obj_w(int32_t w);
    void set_obj_h(int32_t h);
    ExEdit::FilterProcInfo::Geometry &set_obj_data();

    static constexpr ExEdit::ObjectFilterIndex create_object_filter_index(uint16_t object_index, uint16_t filter_index);
    static constexpr float calc_ox(int32_t ox);
    static constexpr float calc_oy(int32_t oy);
    static constexpr float calc_zoom(int32_t zoom);
    static constexpr float calc_cx(int32_t cx, float base_cx = 0.0f);
    static constexpr float calc_cy(int32_t cy, float base_cy = 0.0f);
    static float calc_rz(int32_t rz, float base_angle = 0.0f);

    float get_cx(std::optional<int32_t> cx = std::nullopt, int32_t offset_frame = 0,
                 OffsetType offset_type = OffsetType::Current) const;
    float get_cy(std::optional<int32_t> cy = std::nullopt, int32_t offset_frame = 0,
                 OffsetType offset_type = OffsetType::Current) const;
    float get_rz(std::optional<int32_t> rz = std::nullopt, int32_t offset_frame = 0,
                 OffsetType offset_type = OffsetType::Current) const;
    float calc_trackbar_value_for_drawing_filter(TrackName track_name, int32_t offset_frame = 0,
                                                 OffsetType offset_type = OffsetType::Current) const;

    template <typename T>
    bool create_shared_mem(int32_t key1, int32_t key2, AviUtl::SharedMemoryInfo **handle_ptr, const T &val) const {
        return create_shared_mem_impl(key1, key2, handle_ptr, sizeof(T), &val);
    }

    template <typename T>
    bool get_shared_mem(int32_t key1, int32_t key2, AviUtl::SharedMemoryInfo *handle, T &val) const {
        return get_shared_mem_impl(key1, key2, handle, sizeof(T), &val);
    }

    void delete_shared_mem(int32_t key1, AviUtl::SharedMemoryInfo *handle) const;

private:
    int32_t local_frame;
    bool is_saving;
    ExEdit::ObjectFilterIndex curr_proc_ofi;
    uint16_t object_id;
    uint16_t curr_proc_filter_idx;
    int32_t max_w;
    int32_t max_h;

    bool create_shared_mem_impl(int32_t key1, int32_t key2, AviUtl::SharedMemoryInfo **handle_ptr, int32_t size,
                                const void *val) const;
    bool get_shared_mem_impl(int32_t key1, int32_t key2, AviUtl::SharedMemoryInfo *handle, int32_t size,
                             void *val) const;
};

// Functions of AviUtl Pointers class.
inline bool
AulPtrs::check_exedit_version(uintptr_t exedit_base) const {
    int32_t version = *reinterpret_cast<int32_t *>(exedit_base + VERSION_OFFSET);
    return version == 9200;
}

inline ExEdit::Filter *
AulPtrs::get_exedit_filter_ptr(uintptr_t exedit_base) const {
    ExEdit::Filter **efpp = reinterpret_cast<ExEdit::Filter **>(exedit_base + EFPP_OFFSET);
    if (efpp && *efpp)
        return *efpp;
    return nullptr;
}

inline ExEdit::FilterProcInfo *
AulPtrs::get_exedit_filter_proc_info_ptr(uintptr_t exedit_base) const {
    ExEdit::FilterProcInfo **efpipp = reinterpret_cast<ExEdit::FilterProcInfo **>(exedit_base + EFPIPP_OFFSET);
    if (efpipp && *efpipp)
        return *efpipp;
    return nullptr;
}

inline ExEdit::Filter **
AulPtrs::get_loaded_filter_table(uintptr_t exedit_base) const {
    return reinterpret_cast<ExEdit::Filter **>(exedit_base + LOADED_FILTER_TABLE_OFFSET);
}

inline int32_t
AulPtrs::get_camera_mode(uintptr_t exedit_base) const {
    return *reinterpret_cast<int32_t *>(exedit_base + CAMERA_MODE_OFFSET);
}

inline AulPtrs::GetCurrentProcessing
AulPtrs::get_get_curr_proc(uintptr_t exedit_base) const {
    return reinterpret_cast<GetCurrentProcessing>(exedit_base + GET_CURR_PROC_OFFSET);
}

// Object Utilities Gettrers.
inline int32_t
ObjectUtils::get_frame_begin() const {
    return efpip->objectp->frame_begin;
}

inline int32_t
ObjectUtils::get_frame_end() const {
    return efpip->objectp->frame_end;
}

inline int32_t
ObjectUtils::get_frame_num() const {
    return efpip->frame_num;
}

inline int32_t
ObjectUtils::get_local_frame() const {
    return local_frame;
}

inline int32_t
ObjectUtils::get_obj_w() const {
    return efpip->obj_w;
}

inline int32_t
ObjectUtils::get_obj_h() const {
    return efpip->obj_h;
}

inline const ExEdit::FilterProcInfo::Geometry &
ObjectUtils::get_obj_data() const {
    return efpip->obj_data;
}

inline bool
ObjectUtils::get_is_saving() const {
    return is_saving;
}

inline uint16_t
ObjectUtils::get_object_id() const {
    return object_id;
}

inline int32_t
ObjectUtils::get_obj_index() const {
    return efpip->obj_index;
}

inline int32_t
ObjectUtils::get_obj_num() const {
    return efpip->obj_num;
}

inline int32_t
ObjectUtils::get_camera_mode() const {
    return camera_mode;
}

inline int32_t
ObjectUtils::get_max_w() const {
    return max_w;
}

inline int32_t
ObjectUtils::get_max_h() const {
    return max_h;
}

// Object Utilities Setters.
inline void
ObjectUtils::set_obj_w(int32_t w) {
    efpip->obj_w = std::clamp(w, 0, max_w);
}

inline void
ObjectUtils::set_obj_h(int32_t h) {
    efpip->obj_h = std::clamp(h, 0, max_h);
}

inline ExEdit::FilterProcInfo::Geometry &
ObjectUtils::set_obj_data() {
    return efpip->obj_data;
}

// calculate the geometry.
// This fixed-point precision provides one more decimal digit than the trackbar,
// ensuring accurate internal computation before mapping to UI resolution.
inline constexpr float
ObjectUtils::calc_ox(int32_t ox) {
    return static_cast<float>((static_cast<int64_t>(ox) * 100) >> 12) * 1e-2f;
}

inline constexpr float
ObjectUtils::calc_oy(int32_t oy) {
    return static_cast<float>((static_cast<int64_t>(oy) * 100) >> 12) * 1e-2f;
}

inline constexpr float
ObjectUtils::calc_zoom(int32_t zoom) {
    return static_cast<float>((static_cast<int64_t>(zoom) * 1000) >> 16) * 1e-3f;
}

inline constexpr float
ObjectUtils::calc_cx(int32_t cx, float base_cx) {
    return base_cx + calc_oy(cx);
}

inline constexpr float
ObjectUtils::calc_cy(int32_t cy, float base_cy) {
    return base_cy + calc_oy(cy);
}

inline float
ObjectUtils::calc_rz(int32_t rz, float base_angle) {
    return std::fmod(base_angle, 360.0f) + static_cast<float>((static_cast<int64_t>(rz) * 360 * 1000) >> 16) * 1e-3f;
}

inline float
ObjectUtils::get_cx(std::optional<int32_t> cx, int32_t offset_frame, OffsetType offset_type) const {
    float base_cx = calc_trackbar_value_for_drawing_filter(TrackName::CenterX, offset_frame, offset_type);
    return calc_cx(cx.value_or(efpip->obj_data.cx), base_cx);
}

inline float
ObjectUtils::get_cy(std::optional<int32_t> cy, int32_t offset_frame, OffsetType offset_type) const {
    float base_cy = calc_trackbar_value_for_drawing_filter(TrackName::CenterY, offset_frame, offset_type);
    return calc_cy(cy.value_or(efpip->obj_data.cy), base_cy);
}

inline float
ObjectUtils::get_rz(std::optional<int32_t> rz, int32_t offset_frame, OffsetType offset_type) const {
    float base_angle = calc_trackbar_value_for_drawing_filter(TrackName::RotationZ, offset_frame, offset_type);
    return calc_rz(rz.value_or(efpip->obj_data.rz), base_angle);
}

// Create an object filter index from object index and filter index.
inline constexpr ExEdit::ObjectFilterIndex
ObjectUtils::create_object_filter_index(uint16_t object_index, uint16_t filter_index) {
    return static_cast<ExEdit::ObjectFilterIndex>((static_cast<uint32_t>(filter_index) << 16)
                                                  | static_cast<uint32_t>(object_index));
}