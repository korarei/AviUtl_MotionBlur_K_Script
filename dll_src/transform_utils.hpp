#pragma once

#define _USE_MATH_DEFINES
#include <cmath>

#include "aul_utils.hpp"
#include "structs.hpp"
#include "vector_2d.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ZOOM_MIN 1e-2f

enum class AngleUnit : int {
    Rad,
    Deg
};

enum class Coords : int {
    Global,
    Local
};

inline bool
are_equal(float a, float b) {
    constexpr float epsilon = 1e-4f;
    return std::fabsf(a - b) < epsilon;
}

inline constexpr float
to_rad(float deg) {
    constexpr float inv_180 = 1.0f / 180.0f;
    return deg * static_cast<float>(M_PI) * inv_180;
}

class Transform {
public:
    Transform(float x = 0.0f, float y = 0.0f, float zoom = 1.0f, float rz_deg = 0.0f, float cx = 0.0f, float cy = 0.0f);
    Transform(const ObjectUtils &obj_utls, int offset_frame = 0, OffsetType offset_type = OffsetType::Current);

    Transform operator+(const Transform &other) const;
    Transform operator-(const Transform &other) const;
    Transform operator*(const float &scalar) const;

    Vec2<float> get_location() const;
    float get_zoom() const;
    float get_rz(AngleUnit unit = AngleUnit::Rad) const;
    Vec2<float> get_center() const;

    void apply_geometry(const Geometry &geo);

private:
    float x, y, zoom, rz_deg, rz_rad, cx, cy;
};

class Displacements {
public:
    Displacements(const Transform &from, const Transform &to);

    Vec2<float> get_location(Coords coords = Coords::Local) const;
    float get_distance(Coords coords = Coords::Local) const;
    float get_zoom(Coords coords = Coords::Local) const;
    float get_rz(AngleUnit unit = AngleUnit::Rad) const;
    bool get_is_moved() const;

    int calc_required_samples(float blur_amount, const Vec2<int> &image_size, float scale) const;
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
Transform::operator+(const Transform &other) const {
    return Transform(x + other.x, y + other.y, zoom + other.zoom, rz_deg + other.rz_deg, cx + other.cx, cy + other.cy);
}

inline Transform
Transform::operator-(const Transform &other) const {
    return Transform(x - other.x, y - other.y, zoom - other.zoom, rz_deg - other.rz_deg, cx - other.cx, cy - other.cy);
}

inline Transform
Transform::operator*(const float &scalar) const {
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

inline Vec2<float>
Transform::get_center() const {
    return Vec2<float>(cx, cy);
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