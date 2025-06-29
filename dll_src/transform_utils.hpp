#pragma once

#include <utility>

#include "aul_utils.hpp"
#include "structs.hpp"
#include "utils.hpp"
#include "vector_2d.hpp"
#include "vector_3d.hpp"

#define ZOOM_MIN 1.0e-4f

enum class AngleUnit : int {
    Rad,
    Deg
};

enum class Coords : int {
    Global,
    Local
};

struct Transform {
    float x, y, zoom, rz_deg, rz_rad, cx, cy;

    // Constructors.
    Transform(float x = 0.0f, float y = 0.0f, float zoom = 1.0f, float rz_deg = 0.0f, float cx = 0.0f,
              float cy = 0.0f) noexcept;
    Transform(const ObjectUtils &obj_utls, int offset_frame = 0, OffsetType offset_type = OffsetType::Current) noexcept;

    [[nodiscard]] Transform operator+(const Transform &other) const noexcept;
    [[nodiscard]] Transform operator-(const Transform &other) const noexcept;
    [[nodiscard]] Transform operator*(float scalar) const noexcept;

    Vec2<float> get_location() const;
    float get_zoom() const;
    float get_rz(AngleUnit unit = AngleUnit::Rad) const;
    [[nodiscard]] constexpr Vec2<float> get_pos() const noexcept { return Vec2<float>(x, y); }
    [[nodiscard]] constexpr Vec2<float> get_center() const noexcept { return Vec2<float>(cx, cy); }

    void apply_geometry(const Geometry &geo) noexcept;
};

class Delta {
public:
    // HTM (Homogeneous Transformation Matrix) & Adjustment Matrix (Orientation Matrix).
    struct Mapping {
        Mat3<float> htm;
        Mat3<float> adj_mat;
    };

    // Constructors.
    Delta(const Transform &from, const Transform &to) noexcept;

    // Getters.
    [[nodiscard]] constexpr const float &get_rot() const noexcept { return rel_rot; }
    [[nodiscard]] constexpr const float &get_scale() const noexcept { return rel_scale; }
    [[nodiscard]] constexpr const Vec2<float> &get_pos() const noexcept { return rel_pos; }
    [[nodiscard]] constexpr const Vec2<float> &get_center() const noexcept { return center_to; }
    [[nodiscard]] constexpr const bool &get_is_moved() const noexcept { return is_moved; }

    // Calculate the required samples.
    [[nodiscard]] constexpr int calc_req_samp(float amt, const Vec2<float> &img_size, float adj = 1.0f) const noexcept;

    // Calculate the offset mapping data.
    [[nodiscard]] static Mapping calc_offset_mapping(const SegData<Delta> &delta_data,
                                                     const SegData<float> &offset_amt_data,
                                                     bool is_inv = false) noexcept;

    // Calculate the HTM (Homogeneous Transformation Matrix).
    [[nodiscard]] Mat3<float> calc_htm(float amt = 1.0f, int samp = 1, bool is_inv = false) const noexcept;

    // Calculate the bounding box size.
    [[nodiscard]] Vec2<float> calc_bbox(const Vec2<float> &img_size, float amt = 1.0f,
                                        float offset_rot = 0.0f) const noexcept;

private:
    float rel_rot, rel_scale;
    Vec2<float> rel_pos, center_to, center_from;
    float rel_dist;
    bool is_moved;

    [[nodiscard]] static constexpr Mat3<float> calc_htm_impl(float rot, float scale, const Vec2<float> &pos,
                                                             bool is_inv) noexcept;
};

inline constexpr int
Delta::calc_req_samp(float amt, const Vec2<float> &img_size, float adj) const noexcept {
    if (!is_moved)
        return 0;

    auto size = img_size + center_from.abs();
    float r = size.norm(2) * 0.5f;

    auto req_samps = Vec3<float>(rel_dist, (rel_scale - 1.0f) * r, rel_rot * r) * amt;
    return static_cast<int>(std::ceil(req_samps.norm(-1) * adj));
}

inline constexpr Mat3<float>
Delta::calc_htm_impl(float rot, float scale, const Vec2<float> &pos, bool is_inv) noexcept {
    if (is_inv) {
        auto inv_ori = Mat2<float>::rotation(-rot, 1.0f / scale);
        auto inv_step_pos = -(inv_ori * pos);
        auto trans = Vec3<float>(inv_step_pos, 1.0f);
        return Mat3<float>(inv_ori, trans);
    } else {
        auto ori = Mat2<float>::rotation(rot, scale);
        auto trans = Vec3<float>(pos, 1.0f);
        return Mat3<float>(ori, trans);
    }
}

class Displacements {
public:
    Displacements(const Transform &from, const Transform &to);

    Vec2<float> get_location(Coords coords = Coords::Local) const;
    float get_distance(Coords coords = Coords::Local) const;
    float get_zoom(Coords coords = Coords::Local) const;
    float get_rz(AngleUnit unit = AngleUnit::Rad) const;
    bool get_is_moved() const;

    int calc_required_samples(float blur_amount, const Vec2<int> &image_size, float factor) const;
    std::tuple<float, float, float> calc_relative_displacements(const Displacements &other) const;
    Steps calc_steps(float amount, int samples, float offset_angle_rad) const;
    Steps calc_steps(const std::array<float, 3> &amount, int samples, float offset_angle_rad) const;
    Vec2<float> calc_relative_location(float amount = 1.0f, float offset_angle_rad = 0.0f) const;
    float calc_relative_scale(float amount = 1.0f) const;
    Vec2<float> calc_bounding_box_size(const Vec2<int> &image_size, float amount = 1.0f,
                                       float offset_angle_rad = 0.0f) const;

private:
    Vec2<float> global_location, local_location;
    float global_distance, local_distance;
    float global_zoom, local_zoom;
    float rz_deg, rz_rad;
    bool is_moved;
    Vec2<float> center_from;
    Vec2<float> center_to;

    Steps calc_steps_impl(float loc_amount, float s_amount, float rz_amount, int samples, float offset_angle_rad) const;
};

// Overload the +, -, and * operators for Transform class.
inline Transform
Transform::operator+(const Transform &other) const noexcept {
    return Transform(x + other.x, y + other.y, zoom + other.zoom, rz_deg + other.rz_deg, cx + other.cx, cy + other.cy);
}

inline Transform
Transform::operator-(const Transform &other) const noexcept {
    return Transform(x - other.x, y - other.y, zoom - other.zoom, rz_deg - other.rz_deg, cx - other.cx, cy - other.cy);
}

inline Transform
Transform::operator*(float scalar) const noexcept {
    return Transform(x * scalar, y * scalar, zoom * scalar, rz_deg * scalar, cx * scalar, cy * scalar);
}

// Transform Getters
inline Vec2<float>
Transform::get_location() const {
    return Vec2<float>(x, y);
}

inline float
Transform::get_zoom() const {
    return zoom;
}

inline float
Transform::get_rz(AngleUnit unit) const {
    switch (unit) {
        case AngleUnit::Rad:
            return rz_rad;
        case AngleUnit::Deg:
            return rz_deg;
        default:
            return 0.0f;  // Invalid unit, return 0.0f
    }
}

// Displacements Getters
inline Vec2<float>
Displacements::get_location(Coords coords) const {
    switch (coords) {
        case Coords::Global:
            return global_location;
        case Coords::Local:
            return local_location;
        default:
            return Vec2<float>(0.0f, 0.0f);  // Invalid coordinate system, return zero vector
    }
}

inline float
Displacements::get_distance(Coords coords) const {
    switch (coords) {
        case Coords::Global:
            return global_distance;
        case Coords::Local:
            return local_distance;
        default:
            return 0.0f;  // Invalid coordinate system, return 0.0f
    }
}

inline float
Displacements::get_zoom(Coords coords) const {
    switch (coords) {
        case Coords::Global:
            return global_zoom;
        case Coords::Local:
            return local_zoom;
        default:
            return 0.0f;  // Invalid coordinate system, return 0.0f
    }
}

inline float
Displacements::get_rz(AngleUnit unit) const {
    switch (unit) {
        case AngleUnit::Rad:
            return rz_rad;
        case AngleUnit::Deg:
            return rz_deg;
        default:
            return 0.0f;  // Invalid unit, return 0.0f
    }
}

inline bool
Displacements::get_is_moved() const {
    return is_moved;
}

// Displacements Methods
// Calculate the step values.
inline Steps
Displacements::calc_steps(float amount, int samples, float offset_angle_rad) const {
    return calc_steps_impl(amount, amount, amount, samples, offset_angle_rad);
}

inline Steps
Displacements::calc_steps(const std::array<float, 3> &amount, int samples, float offset_angle_rad) const {
    const auto &[loc_amount, scale_amount, rz_amount] = amount;
    return calc_steps_impl(loc_amount, scale_amount, rz_amount, samples, offset_angle_rad);
}

// Calculate the scale (to / from).
inline float
Displacements::calc_relative_scale(float amount) const {
    return are_equal(amount, 0.0f) ? 1.0f : std::max(1.0f + local_zoom * amount, ZOOM_MIN);
}