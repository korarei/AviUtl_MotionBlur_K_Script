#include "transform_utils.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

// Transform class.
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

// Delta class.
Delta::Delta(const Transform &from, const Transform &to) noexcept :
    rel_rot(to.rz_rad - from.rz_rad),
    rel_scale(std::max(to.zoom / from.zoom, ZOOM_MIN)),
    rel_pos((to.get_pos() - from.get_pos()).rotate(-from.rz_rad, 100.0f / from.zoom)),
    center_to(-to.get_center()),
    center_from(-from.get_center()),
    rel_dist(rel_pos.norm(2)),
    is_moved(!is_zero(rel_dist) || !are_equal(rel_scale, 1.0f) || !is_zero(rel_rot)),
    init_data() {}

Mat3<float>
Delta::calc_offset_htm(OptSegData<Delta> &delta_data, const SegData<float> &offset_amt_data, bool is_inv) noexcept {
    float offset_rot = delta_data.seg1->get_rot() * offset_amt_data.seg1;
    float offset_scale = std::pow(delta_data.seg1->get_scale(), offset_amt_data.seg1);
    auto offset_pos = delta_data.seg1->get_pos() * offset_amt_data.seg1;

    if (delta_data.seg2) {
        offset_rot += delta_data.seg2->get_rot() * offset_amt_data.seg2;
        offset_scale *= std::pow(delta_data.seg2->get_scale(), offset_amt_data.seg2);
        auto pos_seg2 = delta_data.seg2->get_pos();
        offset_pos += pos_seg2.rotate(delta_data.seg1->get_rot(), delta_data.seg1->get_scale()) * offset_amt_data.seg2;
    }

    std::optional<Delta::Init> new_init;

    if (!delta_data.seg1->get_init().is_valid) {
        if (!new_init)
            new_init.emplace(true, Mat2<float>::rotation(-offset_rot, 1.0f / offset_scale));

        delta_data.seg1->set_init(*new_init);
    }

    if (delta_data.seg2 && !delta_data.seg2->get_init().is_valid) {
        if (!new_init)
            new_init.emplace(true, Mat2<float>::rotation(-offset_rot, 1.0f / offset_scale));

        delta_data.seg2->set_init(*new_init);
    }

    return calc_htm_impl(offset_rot, offset_scale, offset_pos, is_inv);
}

Mat3<float>
Delta::calc_htm(float amt, int samp, bool is_inv) const noexcept {
    float step_amt = samp > 1 ? amt / static_cast<float>(samp) : amt;
    float step_rot = rel_rot * step_amt;
    float step_scale = std::pow(rel_scale, step_amt);
    auto step_pos = init_data.orientation * (rel_pos * step_amt);

    return calc_htm_impl(step_rot, step_scale, step_pos, is_inv);
}
