#include "transform_utils.hpp"

#include <algorithm>
#include <cmath>

// Transform class
// Constructor
Transform::Transform(float x, float y, float zoom, float rz_deg, float cx, float cy) noexcept :
    x(x), y(y), zoom(std::max(zoom, ZOOM_MIN)), rz_deg(rz_deg), rz_rad(to_rad(rz_deg)), cx(cx), cy(cy) {}

Transform::Transform(const ObjectUtils &obj_utls, int offset_frame, OffsetType offset_type) noexcept :
    x(obj_utls.calc_track_val(TrackName::X, offset_frame, offset_type)),
    y(obj_utls.calc_track_val(TrackName::Y, offset_frame, offset_type)),
    zoom(std::max(obj_utls.calc_track_val(TrackName::Zoom, offset_frame, offset_type), ZOOM_MIN)),
    rz_deg(obj_utls.calc_track_val(TrackName::RotationZ, offset_frame, offset_type)),
    rz_rad(to_rad(rz_deg)),
    cx(obj_utls.get_cx()),
    cy(obj_utls.get_cy()) {}

void
Transform::apply_geometry(const Geometry &geo) noexcept {
    x += ObjectUtils::calc_ox(geo.ox);
    y += ObjectUtils::calc_oy(geo.oy);
    zoom = std::max(zoom * ObjectUtils::calc_zoom(geo.zoom), ZOOM_MIN);
    rz_deg = ObjectUtils::calc_rz(geo.rz, rz_deg);
    rz_rad = to_rad(rz_deg);
}

Delta::Delta(const Transform &from, const Transform &to) noexcept :
    rel_rot(to.rz_rad - from.rz_rad),
    rel_scale(std::max(to.zoom / from.zoom, ZOOM_MIN)),
    rel_pos((to.get_pos() - from.get_pos()).rotate(-from.rz_rad, 100.0f / from.zoom)),
    center_to(-to.get_center()),
    center_from(-from.get_center()),
    rel_dist(rel_pos.norm(2)),
    is_moved(!is_zero(rel_dist) || !are_equal(rel_scale, 1.0f) || !is_zero(rel_rot)) {}

int
Delta::calc_req_samp(float amt, const Vec2<float> &img_size, float img_scale) const noexcept {
    if (!is_moved)
        return 0;

    auto size = (img_size + center_from.abs()) * img_scale;
    float r = size.norm(2) * 0.5f;

    Vec3<float> req_samps(rel_dist, (rel_scale - 1.0f) * r, rel_rot * r);
    req_samps *= amt;
    return static_cast<int>(std::ceil(req_samps.norm(-1)));
}

Delta::Mapping
Delta::calc_mapping(bool is_inv, float amt, int samp) const noexcept {
    float step_amt = amt / static_cast<float>(samp);
    float step_rot = rel_rot * step_amt;
    float step_scale = 1.0f + (rel_scale - 1.0f) * amt;
    auto step_pos = rel_pos * step_amt;
    auto adj = Mat3<float>::identity();

    if (samp > 1)
        step_scale = std::pow(step_scale, 1.0f / static_cast<float>(samp));

    if (is_inv) {
        auto inv_ori = Mat2<float>::rotation(-step_rot, 1.0f / step_scale);
        auto inv_step_pos = -(inv_ori * step_pos);
        auto trans = Vec3<float>(inv_step_pos, 1.0f);
        auto htm = Mat3<float>(inv_ori, trans);

        if (samp > 1)
            adj = Mat3<float>::rotation(step_rot, 2, step_scale);

        return {htm, adj};
    } else {
        auto ori = Mat2<float>::rotation(step_rot, step_scale);
        auto trans = Vec3<float>(step_pos, 1.0f);
        auto htm = Mat3<float>(ori, trans);

        if (samp > 1)
            adj = Mat3<float>::rotation(-step_rot, 2, 1.0f / step_scale);

        return {htm, adj};
    }
}

Vec2<float>
Delta::calc_bbox(const Vec2<float> &img_size, float amt, float offset_rot) const noexcept {
    if (is_zero(amt) && is_zero(offset_rot)) {
        return img_size;
    } else {
        float theta = rel_rot * amt - offset_rot;
        auto rot_mat = Mat2<float>::rotation(theta);
        Vec2<float> size = img_size * (1.0f + (rel_scale - 1.0f) * amt);
        return rot_mat.abs() * size;
    }
}

// Displacements class
// Constructor
// Constructor with parameters
Displacements::Displacements(const Transform &from, const Transform &to) :
    global_location((to.get_location() - from.get_location())),
    global_distance(global_location.norm(2)),
    local_location(global_location.rotate(-from.get_rz(), 100.0f / from.get_zoom())),
    local_distance(local_location.norm(2)),
    global_zoom((to.get_zoom() - from.get_zoom())),
    local_zoom(global_zoom / from.get_zoom()),
    rz_deg((to.get_rz(AngleUnit::Deg) - from.get_rz(AngleUnit::Deg))),
    rz_rad(to.get_rz() - from.get_rz()),
    is_moved(!are_equal(global_distance, 0.0f) || !are_equal(global_zoom, 0.0f) || !are_equal(rz_deg, 0.0f)),
    center_from(from.get_center()),
    center_to(to.get_center()) {}

// Calculate the required_samples.
int
Displacements::calc_required_samples(float blur_amount, const Vec2<int> &image_size, float factor) const {
    if (!is_moved)
        return 0;

    Vec2<float> abs_center(std::abs(center_from.get_x()), std::abs(center_from.get_y()));
    Vec2<float> size = static_cast<Vec2<float>>(image_size) * factor + abs_center;
    float radius = size.norm(2) * 0.5f;

    return std::max({static_cast<int>(std::ceil(std::abs(local_distance) * blur_amount)),
                     static_cast<int>(std::ceil(std::abs(local_zoom) * radius * blur_amount)),
                     static_cast<int>(std::ceil(std::abs(rz_rad) * radius * blur_amount)), 0});
}

// Calculate the relative_displacements (ratio).
std::tuple<float, float, float>
Displacements::calc_relative_displacements(const Displacements &other) const {
    if (!is_moved || !other.is_moved) {
        return {0.0f, 0.0f, 0.0f};
    }

    return {!are_equal(other.global_distance, 0.0f) ? global_distance / other.global_distance : 0.0f,
            !are_equal(other.global_zoom, 0.0f) ? global_zoom / other.global_zoom : 0.0f,
            !are_equal(other.rz_deg, 0.0f) ? rz_deg / other.rz_deg : 0.0f};
}

// Calculate the step values.
Steps
Displacements::calc_steps_impl(float loc_amount, float scale_amount, float rz_amount, int samples,
                               float offset_angle_rad) const {
    if (!is_moved)
        return Steps{Vec2<float>(0.0f, 0.0f), 1.0f, 0.0f};

    float inv_samples = 1.0f / static_cast<float>(std::max(1, samples));
    Vec2<float> step_location = local_location.rotate(offset_angle_rad, loc_amount * inv_samples);
    float step_scale = std::pow(std::max(1.0f + local_zoom * scale_amount, ZOOM_MIN), inv_samples);
    float step_rz_rad = rz_rad * rz_amount * inv_samples;

    return Steps{step_location, step_scale, step_rz_rad};
}

Vec2<float>
Displacements::calc_relative_location(float amount, float offset_angle_rad) const {
    if (are_equal(amount, 0.0f)) {
        return center_from.rotate(-offset_angle_rad) - center_to.rotate(-offset_angle_rad);
    } else if (are_equal(amount, 1.0f)) {
        Vec2<float> rotated_from = center_from.rotate(-offset_angle_rad);
        Vec2<float> rotated_to = center_to.rotate(rz_rad - offset_angle_rad, calc_relative_scale());

        return rotated_from + local_location - rotated_to;
    } else {
        Vec2<float> rotated_from = center_from.rotate(-offset_angle_rad);
        Vec2<float> scaled_local = local_location * amount;
        Vec2<float> rotated_to = center_to.rotate(rz_rad * amount - offset_angle_rad, calc_relative_scale(amount));

        return rotated_from + scaled_local - rotated_to;
    }
}

Vec2<float>
Displacements::calc_bounding_box_size(const Vec2<int> &image_size, float amount, float offset_angle_rad) const {
    if (are_equal(amount, 0.0f) && are_equal(offset_angle_rad, 0.0f)) {
        return static_cast<Vec2<float>>(image_size);
    } else {
        float angle = rz_rad * amount - offset_angle_rad;
        float cos = std::cos(angle);
        float sin = std::sin(angle);
        Vec2<float> image = static_cast<Vec2<float>>(image_size) * calc_relative_scale(amount);

        float w = std::abs(image.get_x() * cos) + std::abs(image.get_y() * sin);
        float h = std::abs(image.get_x() * sin) + std::abs(image.get_y() * cos);

        return Vec2<float>(w, h);
    }
}