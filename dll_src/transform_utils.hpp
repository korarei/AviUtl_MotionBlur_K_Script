#pragma once

#include <utility>

#include "aul_utils.hpp"
#include "structs.hpp"
#include "utils.hpp"
#include "vector_2d.hpp"
#include "vector_3d.hpp"

#define ZOOM_MIN 1.0e-4f

struct Transform {
    float x, y, zoom, rz_deg, rz_rad, cx, cy;

    // Constructors.
    Transform(float x = 0.0f, float y = 0.0f, float zoom = 1.0f, float rz_deg = 0.0f, float cx = 0.0f,
              float cy = 0.0f) noexcept;
    Transform(const ObjectUtils &obj_utls, int offset_frame = 0, OffsetType offset_type = OffsetType::Current) noexcept;

    [[nodiscard]] Transform operator+(const Transform &other) const noexcept;
    [[nodiscard]] Transform operator-(const Transform &other) const noexcept;
    [[nodiscard]] Transform operator*(float scalar) const noexcept;

    [[nodiscard]] constexpr Vec2<float> get_pos() const noexcept { return Vec2<float>(x, y); }
    [[nodiscard]] constexpr Vec2<float> get_center() const noexcept { return Vec2<float>(cx, cy); }

    void apply_geometry(const Geometry &geo) noexcept;
};

class Delta {
public:
    // HTM (Homogeneous Transformation Matrix) & Adjustment Matrix (Inverse Orientation Matrix).
    struct Mapping {
        Mat3<float> htm;
        Mat3<float> adj_mat;
    };

    struct Init {
        bool is_valid;
        Mat2<float> orientation;

        Init() : is_valid(false), orientation() {}
        Init(bool valid, const Mat2<float> &mat) : is_valid(valid), orientation(mat) {}
    };

    // Constructors.
    Delta(const Transform &from, const Transform &to) noexcept;

    // Getters.
    [[nodiscard]] constexpr const float &get_rot() const noexcept { return rel_rot; }
    [[nodiscard]] constexpr const float &get_scale() const noexcept { return rel_scale; }
    [[nodiscard]] constexpr const Vec2<float> &get_pos() const noexcept { return rel_pos; }
    [[nodiscard]] constexpr const Vec2<float> &get_center() const noexcept { return center_to; }
    [[nodiscard]] constexpr const bool &get_is_moved() const noexcept { return is_moved; }
    [[nodiscard]] constexpr const Init &get_init() const noexcept { return init_data; }

    // Setters.
    constexpr void set_init(const Init &new_init) noexcept { init_data = new_init; }

    // Calculate the required samples.
    [[nodiscard]] constexpr int calc_req_samp(float amt, const Vec2<float> &img_size, float adj = 1.0f) const noexcept;

    // Calculate the offset mapping data.
    [[nodiscard]] static Mat3<float> calc_offset_htm(OptSegData<Delta> &delta_data,
                                                     const SegData<float> &offset_amt_data,
                                                     bool is_inv = false) noexcept;

    // Calculate the HTM (Homogeneous Transformation Matrix).
    [[nodiscard]] Mat3<float> calc_htm(float amt = 1.0f, int samp = 1, bool is_inv = false) const noexcept;

private:
    float rel_rot, rel_scale;
    Vec2<float> rel_pos, center_to, center_from;
    float rel_dist;
    bool is_moved;
    Init init_data;

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
