#include "transform_utils.hpp"

#include <algorithm>

// Transform class
// Constructor
Transform::Transform(float x, float y, float zoom, float rz_deg, float cx, float cy) :
    x(x), y(y), zoom(std::max(zoom, ZOOM_MIN)), rz_deg(rz_deg), rz_rad(to_rad(rz_deg)), cx(cx), cy(cy) {}

Transform::Transform(const ObjectUtils &obj_utls, int offset_frame, OffsetType offset_type) :
    x(obj_utls.calc_track_val(TrackName::X, offset_frame, offset_type)),
    y(obj_utls.calc_track_val(TrackName::Y, offset_frame, offset_type)),
    zoom(std::max(obj_utls.calc_track_val(TrackName::Zoom, offset_frame, offset_type), ZOOM_MIN)),
    rz_deg(obj_utls.calc_track_val(TrackName::RotationZ, offset_frame, offset_type)),
    rz_rad(to_rad(rz_deg)),
    cx(obj_utls.get_cx()),
    cy(obj_utls.get_cy()) {}

void
Transform::apply_geometry(const Geometry &geo) {
    x += ObjectUtils::calc_ox(geo.ox);
    y += ObjectUtils::calc_oy(geo.oy);
    zoom = std::max(zoom * ObjectUtils::calc_zoom(geo.zoom), ZOOM_MIN);
    rz_deg = ObjectUtils::calc_rz(geo.rz, rz_deg);
    rz_rad = to_rad(rz_deg);
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
Displacements::calc_required_samples(float blur_amount, const Vec2<int> &image_size, float scale) const {
    if (!is_moved)
        return 0;

    Vec2<float> abs_center(std::abs(center_from.get_x()), std::abs(center_from.get_y()));
    Vec2<float> size = static_cast<Vec2<float>>(image_size) * scale + abs_center;
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