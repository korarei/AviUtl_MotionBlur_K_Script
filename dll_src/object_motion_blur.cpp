#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#define NOMINMAX
#include <Windows.h>

#include "aul_utils.hpp"
#include "lua_func.hpp"
#include "shared_memory.hpp"
#include "structs.hpp"
#include "transform_utils.hpp"

static constexpr const char *WARNING_COL = "\033[38;5;208m";
static constexpr const char *RESET_COL = "\033[0m";

// Get dll path.
static const std::string
get_self_path() {
    HMODULE hModule = NULL;

    if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCSTR>(&get_self_path),
                              &hModule)) {
        return "";
    }

    char path[MAX_PATH];
    DWORD result = ::GetModuleFileNameA(hModule, path, MAX_PATH);
    if (result == 0) {
        return "";
    }

    return std::string(path);
}

// Shader path.
static const std::string dll_path = get_self_path();
static const std::string shader_path = dll_path.empty() ? "C:" : dll_path.substr(0, dll_path.find_last_of("\\/"));

// The object index is a uint16_t, but the maximum value is probably 15000, so 14 bits (max 16383) should be sufficient.
// I don't think there are more than 65,536 individual objects.
inline static constexpr int32_t
make_shared_mem_key(uint16_t scr_id, uint16_t obj_id, int32_t obj_index) {
    uint32_t id_t = (static_cast<uint32_t>(scr_id & 0x3) << 30);
    uint32_t id_m = (static_cast<uint32_t>(obj_index & 0xFFFF) << 14);
    uint32_t id_b = static_cast<uint32_t>(obj_id & 0x3FFF);
    uint32_t id = id_t | id_m | id_b;
    return static_cast<int32_t>(id);
}

// Apply geometry to the transform.
inline static void
apply_geometry(Transform &transform, int32_t frame, int32_t shared_mem_key, const Geometry &default_geo) {
    Geometry geo;
    if (get_shared_mem(shared_mem_key, frame, geo)) {
        transform.apply_geometry(geo);
    } else {
        transform.apply_geometry(default_geo);
    }
}

// Calculate the transform before frame 0.
inline static std::variant<Transform, std::array<Transform, 2>>
calc_neg_frame(const std::array<Transform, 3> &transforms, bool is_calc_2f_enabled = false) {
    Transform delta1 = transforms[1] - transforms[0];
    Transform delta2 = transforms[2] - transforms[1];
    Transform transform_neg1f = transforms[0] - delta1 * 2 + delta2;

    // Uniformly accelerated linear motion
    if (is_calc_2f_enabled) {
        return std::array<Transform, 2>{transform_neg1f, transform_neg1f - delta1 * 3 + delta2 * 2};
    } else {
        return transform_neg1f;
    }
}

// Calculate the amounticient that determines the length of the blur.
inline static constexpr float
calc_blur_amount(float shutter_angle, float offset = 0.0f) {
    constexpr float inv_360 = 1.0f / 360.0f;
    return std::clamp(shutter_angle * inv_360 - offset, 0.0f, 1.0f);
}

// Calculate the amounticient that determines the offset movement amount.
static std::array<float, 3>
calc_offset_amount(const SegmentData<Displacements> &disp_data, float shutter_angle, float shutter_phase) {
    constexpr float inv_360 = 1.0f / 360.0f;
    constexpr float inv_720 = 1.0f / 720.0f;

    if (!disp_data.seg1 || !disp_data.seg1->get_is_moved()) {
        return {0.0f, 0.0f, 0.0f};
    } else if (!disp_data.seg2) {
        float amount = (shutter_angle + shutter_phase) * inv_360;
        return {amount, amount, amount};
    } else if (disp_data.seg2->get_is_moved()) {
        auto [distance, zoom, rz_deg] = disp_data.seg2->calc_relative_displacements(disp_data.seg1.value());
        float factor = (shutter_angle + shutter_phase) * inv_720;
        auto calc = [&](float ratio) -> float {
            return (3.0f - ratio + (ratio - 1.0f) * shutter_angle * inv_360) * factor;
        };
        return {calc(distance), calc(zoom), calc(rz_deg)};
    } else {
        float amount = 1.0f + shutter_phase * inv_360;
        return {amount, amount, amount};
    }
}

// Calculate the actual number of samples to be used.
inline static constexpr int
calc_samples(int required, int sample_limit, int total_required_samples) {
    float ratio = static_cast<float>(sample_limit) * static_cast<float>(required)
                / static_cast<float>(total_required_samples);
    return std::clamp(required, 1, static_cast<int>(std::max(ratio, 1.0f)));
}

inline static Corner
calc_corners(const Vec2<float> &base, const Vec2<float> &disp, const Vec2<float> &bbox_size, float offset_scale,
             const Vec2<int> &img_size) {
    Corner corner;
    corner.location = base + disp * offset_scale;
    Vec2<float> diff_half_size = (bbox_size * offset_scale - static_cast<Vec2<float>>(img_size)) * 0.5f;
    corner.upper_left = static_cast<Vec2<int>>((diff_half_size - corner.location).ceil());
    corner.lower_right = static_cast<Vec2<int>>((diff_half_size + corner.location).ceil());
    return corner;
}

// Resize the image.
static void
resize_image(const Vec2<int> &img_size, const Vec2<float> &center, const SegmentData<Displacements> &disp,
             const SegmentData<float> &blur, const Steps &offset, float scale_factor_seg1, const Vec2<int> &max_size,
             lua_State *L) {
    if (!disp.seg1 || !blur.seg1) {
        return;
    }

    float offset_scale_inv = 1.0f / offset.scale;
    std::array<Corner, 3> corners;
    int count = 2;

    // Displacement.
    Vec2<float> d0 = center.rotation(-offset.rz_rad, -1.0f) - offset.location;
    Vec2<float> d1 = disp.seg1->calc_relative_location(*blur.seg1, offset.rz_rad);

    // Bounding box size.
    Vec2<float> curr_f_size = disp.seg1->calc_bounding_box_size(img_size, 0.0f, offset.rz_rad);
    Vec2<float> prev_1f_size = disp.seg1->calc_bounding_box_size(img_size, *blur.seg1, offset.rz_rad);

    corners[0] = calc_corners(center, d0, curr_f_size, offset_scale_inv, img_size);
    corners[1] = calc_corners(corners[0].location, d1, prev_1f_size, offset_scale_inv, img_size);

    // If the second segment is available, calculate the corners for it.
    if (disp.seg2 && blur.seg2) {
        Vec2<float> d2 = disp.seg2->calc_relative_location(*blur.seg2, offset.rz_rad)
                                 .rotation(disp.seg1->get_rz(), scale_factor_seg1);
        Vec2<float> prev_2f_size =
                disp.seg2->calc_bounding_box_size(img_size, *blur.seg2, offset.rz_rad - disp.seg1->get_rz())
                * scale_factor_seg1;

        corners[2] = calc_corners(corners[1].location, d2, prev_2f_size, offset_scale_inv, img_size);
        count++;
    }

    std::array<int, 4> expansion = {0, 0, 0, 0};  // top, bottom, left, right

    for (int i = 0; i < count; i++) {
        expansion[0] = std::max(expansion[0], corners[i].upper_left.get_y());
        expansion[1] = std::max(expansion[1], corners[i].lower_right.get_y());
        expansion[2] = std::max(expansion[2], corners[i].upper_left.get_x());
        expansion[3] = std::max(expansion[3], corners[i].lower_right.get_x());
    }

    Vec2<int> new_size = img_size + Vec2<int>(expansion[2] + expansion[3], expansion[0] + expansion[1]);
    if (new_size.get_x() > max_size.get_x() || new_size.get_y() > max_size.get_y()) {
        std::cout << WARNING_COL << "[ObjectMotionBlur][WARNING] Image size exceeds maximum size.\n"
                  << "New size: " << new_size.to_string() << "\nMax size:" << max_size.to_string() << RESET_COL
                  << std::endl;
    }

    expand_image(expansion, L);
}

// Rendering.
static void
render_object_motion_blur(lua_State *L, const ObjectMotionBlurParams &params, const SegmentData<Steps> &steps_data,
                          const SegmentData<int> &samples_data) {
    GLShaderKit gl_shader_kit(L);

    if (!gl_shader_kit.isInitialized())
        throw std::runtime_error("GL Shader Kit is not available.");

    Image img = gl_shader_kit.get_image();

    gl_shader_kit.activate();
    gl_shader_kit.setPlaneVertex(1);
    gl_shader_kit.setShader((shader_path + params.shader_folder + "\\MotionBlur_K.frag").c_str(),
                            params.is_reload_enabled);

    gl_shader_kit.setTexture2D(0, img);
    Vec2<float> resolution = static_cast<Vec2<float>>(img.size);
    Vec2<float> pivot = img.center + resolution * 0.5f;
    gl_shader_kit.setFloat("resolution", {resolution.get_x(), resolution.get_y()});
    gl_shader_kit.setFloat("pivot", {pivot.get_x(), pivot.get_y()});
    gl_shader_kit.setInt("is_orig_img_visible", {params.is_orig_img_visible});
    gl_shader_kit.setInt("samples", {*samples_data.seg1, samples_data.seg2 ? *samples_data.seg2 : 0});

    gl_shader_kit.setParamsForOMBStep("offset", *steps_data.offset);
    gl_shader_kit.setParamsForOMBStep("seg1", *steps_data.seg1);
    if (steps_data.seg2)
        gl_shader_kit.setParamsForOMBStep("seg2", *steps_data.seg2);

    gl_shader_kit.draw("TRIANGLE_STRIP", img);
    gl_shader_kit.deactivate();

    gl_shader_kit.putpixeldata(img.data);
}

// Save Geometry data to shared memory. (-1, -2)
static void
save_minimal_geometry(int32_t shared_mem_key, const Geometry &default_geo) {
    Geometry geo_prev_1f;
    if (get_shared_mem(shared_mem_key, -1, geo_prev_1f))
        create_shared_mem(shared_mem_key, -2, geo_prev_1f);
    else
        create_shared_mem(shared_mem_key, -2, default_geo);

    create_shared_mem(shared_mem_key, -1, default_geo);
}

// Clear handle.
static void
cleanup_geometry_data(bool is_enabled, int method, int32_t base_key, const ObjectUtils &obj_utils) {
    if (is_enabled) {
        switch (method) {
            case 1:
                break;  // pass
            case 2:
                if (obj_utils.get_frame_num() == obj_utils.get_frame_end()
                    && obj_utils.get_obj_index() == obj_utils.get_obj_num() - 1)
                    cleanup_for_id(0, obj_utils.get_object_id());

                break;
            case 3:
                cleanup_all(0);
                break;
            case 4:
                cleanup_for_id(0, obj_utils.get_object_id());
                break;
            default:
                uint16_t id = static_cast<uint16_t>(std::clamp(std::abs(static_cast<int64_t>(method)), 0i64, 65535i64));
                cleanup_for_id(0, id);
                break;
        }
    } else if (handle_exists(base_key)) {
        cleanup_for_id(0, obj_utils.get_object_id());
    }
}

// The main function of Object Motion Blur.
int
process_object_motion_blur(lua_State *L) {
    try {
        // Create instances.
        ObjectUtils obj_utils;
        ObjectMotionBlurParams params(L, obj_utils.get_is_saving());
        // Required components for saving geometry data.
        if (params.is_using_geometry_enabled && obj_utils.get_obj_num() > 65536)
            std::cout << WARNING_COL << "[ObjectMotionBlur][WARNING] There are too many individual objects."
                      << RESET_COL << std::endl;

        int32_t shared_mem_key = make_shared_mem_key(0, obj_utils.get_object_id(), obj_utils.get_obj_index());
        int32_t shared_mem_base_key = make_shared_mem_key(0, obj_utils.get_object_id(), 0);
        Geometry geo_curr_f;
        int shared_mem_curr_f = params.is_saving_all_geometry_enabled ? obj_utils.get_local_frame() : 0;

        if (params.is_using_geometry_enabled) {
            const auto &data = obj_utils.get_obj_data();
            geo_curr_f = {data.ox, data.oy, data.cx, data.cy, data.zoom, data.rz};

            int local_frame = obj_utils.get_local_frame();
            if (params.is_saving_all_geometry_enabled || local_frame == 0 || local_frame == 1 || local_frame == 2) {
                create_shared_mem(shared_mem_key, local_frame, geo_curr_f);
            }
        }

        // Invalid value.
        if (are_equal(params.shutter_angle, 0.0f))
            return 0;

        if (are_equal(obj_utils.calc_trackbar_value_for_drawing_filter(TrackName::Zoom), 0.0f)) {
            // Save geometry data.
            // This section is executed only when "saving all geometry" is disabled.
            if (params.is_using_geometry_enabled && !params.is_saving_all_geometry_enabled
                && obj_utils.get_camera_mode() != 3)
                save_minimal_geometry(shared_mem_key, geo_curr_f);

            // Cleanup.
            cleanup_geometry_data(params.is_using_geometry_enabled, params.geometry_data_cleanup_method,
                                  shared_mem_base_key, obj_utils);
            return 0;
        }

        // Insufficient samples.
        if (params.sample_limit == 1 || (params.shutter_angle > 360.0f && params.sample_limit == 2))
            throw std::runtime_error("The samples are insufficient.");

        // Prepare the items needed for calculation.
        bool is_prev_2f_needed = params.shutter_angle > 360.0f
                              && (params.is_calc_neg1f_and_neg2f_enabled || obj_utils.get_local_frame() >= 2);

        SegmentData<Displacements> disp_data;
        SegmentData<float> blur_amount_data;
        SegmentData<int> required_samples_data, samples_data;
        SegmentData<Steps> steps_data;
        Vec2<int> image_size(obj_utils.get_obj_w(), obj_utils.get_obj_h());
        Vec2<float> center(obj_utils.calc_cx(), obj_utils.calc_cy());

        if (params.is_calc_neg1f_and_neg2f_enabled
            && (obj_utils.get_local_frame() == 0 || obj_utils.get_local_frame() == 1)) {
            std::array<Transform, 3> transforms = {Transform(obj_utils, 0, OffsetType::Start),
                                                   Transform(obj_utils, 1, OffsetType::Start),
                                                   Transform(obj_utils, 2, OffsetType::Start)};

            if (params.is_using_geometry_enabled) {
                for (auto &transform : transforms) {
                    apply_geometry(transform, &transform - transforms.data(), shared_mem_key, geo_curr_f);
                }
            }

            // Calculate displacements from transforms.
            // Calculate frames that do not actually exist.
            if (obj_utils.get_local_frame() == 0) {
                auto transform_neg_frame = calc_neg_frame(transforms, is_prev_2f_needed);
                std::visit(
                        [&](auto &&transform) {
                            if constexpr (std::is_same_v<std::decay_t<decltype(transform)>, Transform>) {
                                disp_data.seg1 = Displacements(transforms[0], transform);
                            } else {
                                auto &[neg1f, neg2f] = transform;
                                disp_data.seg1 = Displacements(transforms[0], neg1f);
                                disp_data.seg2 = Displacements(neg1f, neg2f);
                            }
                        },
                        transform_neg_frame);
            } else {
                if (!is_prev_2f_needed) {
                    disp_data.seg1 = Displacements(transforms[1], transforms[0]);
                } else {
                    auto transform_neg_frame = calc_neg_frame(transforms);
                    std::visit(
                            [&](auto &&transform) {
                                if constexpr (std::is_same_v<std::decay_t<decltype(transform)>, Transform>) {
                                    disp_data.seg1 = Displacements(transforms[1], transforms[0]);
                                    disp_data.seg2 = Displacements(transforms[0], transform);
                                } else {
                                    throw std::runtime_error("Unexpected variant type in transform_neg_frame.");
                                }
                            },
                            transform_neg_frame);
                }
            }
            // Default.
        } else if ((params.is_calc_neg1f_and_neg2f_enabled && obj_utils.get_local_frame() >= 2)
                   || (!params.is_calc_neg1f_and_neg2f_enabled && obj_utils.get_local_frame() != 0)) {
            Transform transform_curr_f = Transform(obj_utils);
            Transform transform_prev_1f = Transform(obj_utils, -1);

            if (params.is_using_geometry_enabled) {
                transform_curr_f.apply_geometry(geo_curr_f);
                apply_geometry(transform_prev_1f, shared_mem_curr_f - 1, shared_mem_key, geo_curr_f);
            }

            disp_data.seg1 = Displacements(transform_curr_f, transform_prev_1f);

            if (is_prev_2f_needed) {
                Transform transform_prev_2f = Transform(obj_utils, -2);

                if (params.is_using_geometry_enabled) {
                    apply_geometry(transform_prev_2f, shared_mem_curr_f - 2, shared_mem_key, geo_curr_f);
                }

                disp_data.seg2 = Displacements(transform_prev_1f, transform_prev_2f);
            }
        }

        // Save geometry data.
        // This section is executed only when "saving all geometry" is disabled.
        if (params.is_using_geometry_enabled && !params.is_saving_all_geometry_enabled
            && obj_utils.get_camera_mode() != 3)
            save_minimal_geometry(shared_mem_key, geo_curr_f);

        // Cleanup.
        cleanup_geometry_data(params.is_using_geometry_enabled, params.geometry_data_cleanup_method,
                              shared_mem_base_key, obj_utils);

        // Invalid value.
        if (!disp_data.seg1)
            return 0;

        bool is_prev_2f_rendering = disp_data.seg2 && disp_data.seg2->get_is_moved();  // Render to 2 frames ago.
        float scale_factor_seg1 = disp_data.seg1->calc_relative_scale();

        // Calculate the required samples.
        blur_amount_data.seg1 = calc_blur_amount(params.shutter_angle);
        required_samples_data.seg1 = disp_data.seg1->calc_required_samples(*blur_amount_data.seg1, image_size, 1.0f);
        int total_required_samples = *required_samples_data.seg1;

        if (is_prev_2f_rendering) {
            blur_amount_data.seg2 = calc_blur_amount(params.shutter_angle, 1.0f);
            required_samples_data.seg2 =
                    disp_data.seg2->calc_required_samples(*blur_amount_data.seg2, image_size, scale_factor_seg1);
            total_required_samples = *required_samples_data.seg1 + *required_samples_data.seg2;
        }

        // Invalid value.
        if (total_required_samples == 0)
            return 0;

        // Calculate the step data.
        auto offset_amount = calc_offset_amount(disp_data, params.shutter_angle, params.shutter_phase);
        steps_data.offset = disp_data.seg1->calc_steps(offset_amount, 1, 0.0f);

        samples_data.seg1 = calc_samples(*required_samples_data.seg1, params.sample_limit - 1, total_required_samples);
        steps_data.seg1 =
                disp_data.seg1->calc_steps(*blur_amount_data.seg1, *samples_data.seg1, steps_data.offset->rz_rad);

        if (is_prev_2f_rendering) {
            samples_data.seg2 =
                    calc_samples(*required_samples_data.seg2, params.sample_limit - 1, total_required_samples);
            steps_data.seg2 =
                    disp_data.seg2->calc_steps(*blur_amount_data.seg2, *samples_data.seg2, steps_data.offset->rz_rad);
        }

        // Resize.
        if (!params.is_keeping_size_enabled)
            resize_image(image_size, center, disp_data, blur_amount_data, *steps_data.offset, scale_factor_seg1,
                         Vec2<int>(obj_utils.get_max_w(), obj_utils.get_max_h()), L);

        // Rendering.
        render_object_motion_blur(L, params, steps_data, samples_data);

        // Print information.params.is_printing_info_enabled
        if (params.is_printing_info_enabled) {
            std::string cleanup_method_str;
            switch (params.geometry_data_cleanup_method) {
                case 1:
                    cleanup_method_str = "None";
                    break;
                case 2:
                    cleanup_method_str = "Auto (End of Frame)";
                    break;
                case 3:
                    cleanup_method_str = "All Objects";
                    break;
                case 4:
                    cleanup_method_str = "Current Object";
                    break;
                default:
                    cleanup_method_str = "Custom ID";
                    break;
            }
            std::cout << "[ObjectMotionBlur][INFO]\nObject ID: " << obj_utils.get_object_id()
                      << "\nIndex: " << obj_utils.get_obj_index()
                      << "\nRequired Samples: " << total_required_samples + 1
                      << "\nGeo Clear Method: " << cleanup_method_str << std::endl;
        }

        return 0;
    } catch (const std::runtime_error &e) {
        lua_pushfstring(L, "[ObjectMotionBlur] %s", e.what());
        lua_error(L);
        return 0;
    } catch (const std::invalid_argument &e) {
        lua_pushfstring(L, "[ObjectMotionBlur] %s", e.what());
        lua_error(L);
        return 0;
    } catch (const std::exception &e) {
        lua_pushfstring(L, "[ObjectMotionBlur] %s", e.what());
        lua_error(L);
        return 0;
    } catch (...) {
        lua_pushstring(L, "[ObjectMotionBlur] Unknown error occurred.");
        lua_error(L);
        return 0;
    }
}