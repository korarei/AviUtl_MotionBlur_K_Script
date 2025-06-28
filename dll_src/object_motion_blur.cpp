#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
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
#include "utils.hpp"

static constexpr const char *WARNING_COL = "\033[38;5;208m";
static constexpr const char *RESET_COL = "\033[0m";

// The object index (obj_id) is a std::uint16_t, but the maximum value is probably 15000, so 14 bits (max 16383) should
// be sufficient. I don't think there are more than 262,144 (2^18) individual objects.
inline static constexpr std::uint32_t
make_shared_mem_key(std::uint16_t obj_id, std::int32_t obj_index) {
    std::uint32_t id_t = (static_cast<std::uint32_t>(obj_index & 0x3FFFF) << 14);
    std::uint32_t id_b = static_cast<std::uint32_t>(obj_id & 0x3FFFu);
    std::uint32_t id = id_t | id_b;
    return id;
}

// Get shared memory class.
static std::unique_ptr<SharedMemory> &
get_shared_mem() {
    static std::unique_ptr<SharedMemory> shared_mem = std::make_unique<SharedMemory>(3u);  // 8 elems per block.
    return shared_mem;
}

// Apply geometry to the transform.
inline static void
apply_geo(Transform &tf, std::uint32_t shared_mem_key, std::uint32_t slot_id, const Geometry &default_geo) {
    auto &shared_mem = get_shared_mem();
    Geometry geo;

    if (shared_mem->read(shared_mem_key, slot_id, geo) && geo.is_valid)
        tf.apply_geometry(geo);
    else
        tf.apply_geometry(default_geo);
}

// Calculate the transform before frame 0.
inline static std::variant<Transform, std::array<Transform, 2>>
calc_neg_frame(const std::array<Transform, 3> &tf_array, bool will_calc_2f = false) {
    Transform d1 = tf_array[1] - tf_array[0];
    Transform d2 = tf_array[2] - tf_array[1];
    Transform tf_neg_1f = tf_array[0] - d1 * 2.0f + d2;

    // Uniformly accelerated linear motion
    if (will_calc_2f) {
        return std::array<Transform, 2>{tf_neg_1f, tf_neg_1f - d1 * 3.0f + d2 * 2.0f};
    } else {
        return tf_neg_1f;
    }
}

// Calculate the amounticient that determines the length of the blur.
inline static SegData<float>
calc_blur_amt(float shutter_angle) {
    constexpr float inv_360 = 1.0f / 360.0f;

    const float ratio = shutter_angle * inv_360;
    const float seg1 = std::min(ratio, 1.0f);
    const float seg2 = std::max(ratio - 1.0f, 0.0f);

    return SegData<float>(seg1, seg2);
}

// Calculate the amounticient that determines the offset movement amount.
static std::array<float, 3>
calc_offset_amt(const SegmentData<Displacements> &disp_data, float shutter_angle, float shutter_phase) {
    constexpr float inv_360 = 1.0f / 360.0f;
    constexpr float inv_720 = 1.0f / 720.0f;

    if (!disp_data.seg1 || !disp_data.seg1->get_is_moved()) {
        return {0.0f, 0.0f, 0.0f};
    } else if (!disp_data.seg2) {
        float amt = (shutter_angle + shutter_phase) * inv_360;
        return {amt, amt, amt};
    } else if (disp_data.seg2->get_is_moved()) {
        auto [distance, zoom, rz_deg] = disp_data.seg2->calc_relative_displacements(disp_data.seg1.value());
        float factor = (shutter_angle + shutter_phase) * inv_720;
        auto calc = [&](float ratio) -> float {
            return (3.0f - ratio + (ratio - 1.0f) * shutter_angle * inv_360) * factor;
        };
        return {calc(distance), calc(zoom), calc(rz_deg)};
    } else {
        float amt = 1.0f + shutter_phase * inv_360;
        return {amt, amt, amt};
    }
}

// Calculate the actual number of samples to be used.
inline static SegData<int>
calc_samp(const SegData<int> &req_samp_data, int samp_lim, int total_req_samp) {
    if (!req_samp_data.seg2) {
        int samp = std::min(*req_samp_data.seg1, samp_lim);
        return SegData<int>(samp, 0);
    }

    int samp_seg1 = (samp_lim * *req_samp_data.seg1) / total_req_samp;
    int samp_seg2 = samp_lim - samp_seg1;
    return SegData<int>(samp_seg1, samp_seg2);
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
             const SegData<float> &blur, const Steps &offset, float scale_factor_seg1, const Vec2<int> &max_size,
             lua_State *L) {
    if (!disp.seg1 || !blur.seg1) {
        return;
    }

    float offset_scale_inv = 1.0f / offset.scale;
    std::array<Corner, 3> corners;
    int count = 2;

    // Displacement.
    Vec2<float> d0 = center.rotate(-offset.rz_rad, -1.0f) - offset.location;
    Vec2<float> d1 = disp.seg1->calc_relative_location(*blur.seg1, offset.rz_rad);

    // Bounding box size.
    Vec2<float> curr_f_size = disp.seg1->calc_bounding_box_size(img_size, 0.0f, offset.rz_rad);
    Vec2<float> prev_1f_size = disp.seg1->calc_bounding_box_size(img_size, *blur.seg1, offset.rz_rad);

    corners[0] = calc_corners(center, d0, curr_f_size, offset_scale_inv, img_size);
    corners[1] = calc_corners(corners[0].location, d1, prev_1f_size, offset_scale_inv, img_size);

    // If the second segment is available, calculate the corners for it.
    if (disp.seg2 && blur.seg2) {
        Vec2<float> d2 = disp.seg2->calc_relative_location(*blur.seg2, offset.rz_rad)
                                 .rotate(disp.seg1->get_rz(), scale_factor_seg1);
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
                  << "New size: " << new_size << "\nMax size:" << max_size << RESET_COL << std::endl;
    }

    expand_image(expansion, L);
}

// Rendering.
static void
render_object_motion_blur(lua_State *L, const ObjectMotionBlurParams &params, const SegmentData<Steps> &steps_data,
                          const SegData<int> &samp_data, const MappingData<Mat3<float>> &htm_data) {
    namespace fs = std::filesystem;

    fs::path shader_path = get_self_dir() / params.shader_dir.relative_path() / "MotionBlur_K.frag";
    if (!fs::exists(shader_path) || !fs::is_regular_file(shader_path))
        throw std::runtime_error("Shader file not found: " + shader_path.string());

    GLShaderKit gl_shader_kit(L);
    if (!gl_shader_kit.isInitialized())
        throw std::runtime_error("GL Shader Kit is not available.");

    Image img = gl_shader_kit.get_image();

    gl_shader_kit.activate();
    gl_shader_kit.setPlaneVertex(1);
    gl_shader_kit.setShader(shader_path.string(), params.reload_shader);

    gl_shader_kit.setTexture2D(0, img);
    Vec2<float> res = static_cast<Vec2<float>>(img.size);
    Vec2<float> pivot = img.center + res * 0.5f;
    gl_shader_kit.setFloat("res", {res.get_x(), res.get_y()});
    gl_shader_kit.setFloat("pivot", {pivot.get_x(), pivot.get_y()});
    gl_shader_kit.setInt("mix_orig_img", {params.mix_orig_img});
    gl_shader_kit.setInt("samp", {*samp_data.seg1, *samp_data.seg2});

    gl_shader_kit.setParamsForOMBStep("offset", *steps_data.offset);
    gl_shader_kit.setMat3("init_htm_seg1", false, *htm_data.seg1);
    if (steps_data.seg2) {
        gl_shader_kit.setMat3("init_htm_seg2", false, *htm_data.seg2);
    }

    gl_shader_kit.draw("TRIANGLE_STRIP", img);
    gl_shader_kit.deactivate();

    gl_shader_kit.putpixeldata(img.data);
}

// Save Geometry data to shared memory. (4, 3)
static void
save_minimal_geo(std::int32_t shared_mem_key, const Geometry &default_geo) {
    Geometry geo_prev_1f;
    auto &shared_mem = get_shared_mem();

    if (shared_mem->read(shared_mem_key, 4u, geo_prev_1f))
        shared_mem->write(shared_mem_key, 3u, geo_prev_1f);
    else
        shared_mem->write(shared_mem_key, 3u, default_geo);

    shared_mem->write(shared_mem_key, 4u, default_geo);
}

// Clear handle.
static void
cleanup_geo(bool is_geo_used, int method, bool is_last_frame, std::uint16_t obj_id) {
    auto &shared_mem = get_shared_mem();
    constexpr std::uint32_t key1_mask = 0x3FFFu;  // 14 bits for object index.
    std::uint32_t match_bits = static_cast<std::uint32_t>(obj_id) & key1_mask;

    if (is_geo_used) {
        switch (method) {
            case 1:
                break;  // pass
            case 2:
                if (is_last_frame)
                    shared_mem->cleanup_for_key1_mask(match_bits, key1_mask);

                break;
            case 3:
                shared_mem->cleanup_all_handle();
                break;
            case 4:
                shared_mem->cleanup_for_key1_mask(match_bits, key1_mask);
                break;
            default:
                std::uint32_t id =
                        static_cast<std::uint32_t>(std::clamp(std::abs(static_cast<int64_t>(method)), 0i64, 15000i64));
                shared_mem->cleanup_for_key1_mask(id, key1_mask);
                break;
        }
    } else if (shared_mem->has_key1(match_bits)) {
        shared_mem->cleanup_for_key1_mask(match_bits, key1_mask);
    }
}

// The main function of Object Motion Blur.
int
process_object_motion_blur(lua_State *L) {
    try {
        // Create instances.
        auto &shared_mem = get_shared_mem();
        ObjectUtils obj_utils;
        ObjectMotionBlurParams params(L, obj_utils.get_is_saving());

        if (params.use_geo && obj_utils.get_obj_num() > 262144)  // 2^18
            std::cout << WARNING_COL << "[ObjectMotionBlur][WARNING] There are too many individual objects."
                      << RESET_COL << std::endl;

        // Required components for saving geometry data.
        bool is_last_frame = obj_utils.get_frame_num() == obj_utils.get_frame_end();
        bool is_last_obj_index = obj_utils.get_obj_index() == (obj_utils.get_obj_num() - 1);
        std::uint16_t obj_id = obj_utils.get_curr_object_idx();
        std::int32_t local_frame = obj_utils.get_local_frame();

        std::uint32_t shared_mem_key = make_shared_mem_key(obj_id, obj_utils.get_obj_index());
        std::uint32_t base_slot_id = params.save_all_geo ? std::max(local_frame - 1, 0) : 4u;
        const auto &data = obj_utils.get_obj_data();
        Geometry geo_curr_f = {data.ox, data.oy, data.cx, data.cy, data.zoom, data.rz};

        auto update_geo = [&]() {
            // Save geometry data.
            // This section is executed only when "Save All Geo" is disabled.
            if (params.use_geo && !params.save_all_geo && obj_utils.get_camera_mode() != 3)
                save_minimal_geo(shared_mem_key, geo_curr_f);

            // Cleanup.
            if (is_last_obj_index)
                cleanup_geo(params.use_geo, params.geo_cleanup_method, is_last_frame, obj_id);
        };

        if (params.use_geo && (params.save_all_geo || local_frame <= 2))
            shared_mem->write(shared_mem_key, local_frame, geo_curr_f);

        // Invalid value.
        if (is_zero(params.shutter_angle)) {
            update_geo();
            return 0;
        }

        if (is_zero(obj_utils.calc_track_val(TrackName::Zoom))) {
            update_geo();
            return 0;
        }

        // Insufficient samples.
        if (params.samp_lim == 1 || (params.shutter_angle > 360.0f && params.samp_lim == 2))
            throw std::runtime_error("The samples are insufficient.");

        // Prepare the items needed for calculation.
        bool should_calc_prev_2f = params.shutter_angle > 360.0f && (params.calc_neg_f || local_frame >= 2);

        SegmentData<Displacements> disp_data;
        SegmentData<Steps> steps_data;
        SegData<Delta> delta_data;
        SegData<int> req_samp_data;
        MappingData<Mat3<float>> htm_data;

        Vec2<float> img_size = static_cast<Vec2<float>>(Vec2<int>(obj_utils.get_obj_w(), obj_utils.get_obj_h()));
        Vec2<float> center(obj_utils.get_cx(), obj_utils.get_cy());
        Vec2<int> max_size(obj_utils.get_max_w(), obj_utils.get_max_h());

        // calculate the displacements.
        if (params.calc_neg_f && local_frame <= 1) {
            std::array<Transform, 3> tf_array = {Transform(obj_utils, 0, OffsetType::Start),
                                                 Transform(obj_utils, 1, OffsetType::Start),
                                                 Transform(obj_utils, 2, OffsetType::Start)};

            if (params.use_geo) {
                for (auto &tf : tf_array) {
                    apply_geo(tf, shared_mem_key, &tf - tf_array.data(), geo_curr_f);
                }
            }

            // Calculate displacements from transforms.
            // Calculate frames that do not actually exist.
            if (local_frame == 0) {
                auto tf_neg_f = calc_neg_frame(tf_array, should_calc_prev_2f);
                std::visit(
                        [&](auto &&tf) {
                            if constexpr (std::is_same_v<std::decay_t<decltype(tf)>, Transform>) {
                                disp_data.seg1 = Displacements(tf_array[0], tf);
                                delta_data.seg1 = Delta(tf_array[0], tf);
                            } else {
                                auto &[neg_1f, neg_2f] = tf;
                                disp_data.seg1 = Displacements(tf_array[0], neg_1f);
                                disp_data.seg2 = Displacements(neg_1f, neg_2f);
                                delta_data.seg1 = Delta(tf_array[0], neg_1f);
                                delta_data.seg2 = Delta(neg_1f, neg_2f);
                            }
                        },
                        tf_neg_f);
            } else {
                if (!should_calc_prev_2f) {
                    disp_data.seg1 = Displacements(tf_array[1], tf_array[0]);
                    delta_data.seg1 = Delta(tf_array[1], tf_array[0]);
                } else {
                    auto tf_neg_f = calc_neg_frame(tf_array);
                    std::visit(
                            [&](auto &&tf) {
                                if constexpr (std::is_same_v<std::decay_t<decltype(tf)>, Transform>) {
                                    disp_data.seg1 = Displacements(tf_array[1], tf_array[0]);
                                    disp_data.seg2 = Displacements(tf_array[0], tf);
                                    delta_data.seg1 = Delta(tf_array[1], tf_array[0]);
                                    delta_data.seg2 = Delta(tf_array[0], tf);
                                } else {
                                    throw std::runtime_error("Unexpected variant type in tf_neg_f.");
                                }
                            },
                            tf_neg_f);
                }
            }
        } else if (local_frame != 0) {  // Default case.
            Transform tf_curr_f = Transform(obj_utils);
            Transform tf_prev_1f = Transform(obj_utils, -1);

            if (params.use_geo) {
                tf_curr_f.apply_geometry(geo_curr_f);
                apply_geo(tf_prev_1f, shared_mem_key, base_slot_id, geo_curr_f);
            }

            disp_data.seg1 = Displacements(tf_curr_f, tf_prev_1f);
            delta_data.seg1 = Delta(tf_curr_f, tf_prev_1f);

            if (should_calc_prev_2f) {
                Transform tf_prev_2f = Transform(obj_utils, -2);

                if (params.use_geo) {
                    apply_geo(tf_prev_2f, shared_mem_key, base_slot_id - 1u, geo_curr_f);
                }

                disp_data.seg2 = Displacements(tf_prev_1f, tf_prev_2f);
                delta_data.seg2 = Delta(tf_prev_1f, tf_prev_2f);
            }
        }

        // Reinitialize geometry.
        update_geo();

        // Invalid value.
        if (!delta_data.seg1)
            return 0;

        bool can_render_prev_2f = delta_data.seg2 && delta_data.seg2->get_is_moved();  // Render to 2 frames ago.

        // Calculate the blur amounts.
        SegData<float> blur_amt_data = calc_blur_amt(params.shutter_angle);

        // Calculate the samples.
        req_samp_data.seg1 = delta_data.seg1->calc_req_samp(*blur_amt_data.seg1, img_size);
        int total_req_samp = *req_samp_data.seg1;

        if (can_render_prev_2f) {
            float adj = delta_data.seg1->get_scale();
            req_samp_data.seg2 = delta_data.seg2->calc_req_samp(*blur_amt_data.seg2, img_size, adj);
            total_req_samp = *req_samp_data.seg1 + *req_samp_data.seg2;
        }

        if (total_req_samp == 0)
            return 0;

        SegData<int> samp_data = calc_samp(req_samp_data, params.samp_lim - 1, total_req_samp);

        // Calculate the step data.
        auto offset_amt = calc_offset_amt(disp_data, params.shutter_angle, params.shutter_phase);
        steps_data.offset = disp_data.seg1->calc_steps(offset_amt, 1, 0.0f);

        htm_data.seg1 = delta_data.seg1->calc_htm(*blur_amt_data.seg1, true, *samp_data.seg1);

        steps_data.seg1 = disp_data.seg1->calc_steps(*blur_amt_data.seg1, *samp_data.seg1, steps_data.offset->rz_rad);

        if (can_render_prev_2f) {
            htm_data.seg2 = delta_data.seg2->calc_htm(*blur_amt_data.seg2, true, *samp_data.seg2);

            steps_data.seg2 =
                    disp_data.seg2->calc_steps(*blur_amt_data.seg2, *samp_data.seg2, steps_data.offset->rz_rad);
        }

        // Resize.
        if (!params.keep_size)
            resize_image(static_cast<Vec2<int>>(img_size), center, disp_data, blur_amt_data, *steps_data.offset,
                         delta_data.seg1->get_scale(), max_size, L);

        // Rendering.
        render_object_motion_blur(L, params, steps_data, samp_data, htm_data);

        // Print information.params.is_printing_info_enabled
        if (params.print_info) {
            std::string cleanup_method_str;
            switch (params.geo_cleanup_method) {
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

            if (obj_utils.get_obj_index() == 0) {
                std::cout << "[ObjectMotionBlur][INFO]\nDll Version: " << get_version() << "\nObject ID: " << obj_id
                          << "\nGeo Clear Method: " << cleanup_method_str << std::endl;
            }
            std::cout << "Index: " << obj_utils.get_obj_index() << ", Required Samples: " << total_req_samp + 1
                      << std::endl;
        }

        return 0;
    } catch (const std::runtime_error &e) {
        lua_pushfstring(L, "[ObjectMotionBlur] Runtime Error: %s", e.what());
        lua_error(L);
        return 0;
    } catch (const std::invalid_argument &e) {
        lua_pushfstring(L, "[ObjectMotionBlur] Invalid Argument: %s", e.what());
        lua_error(L);
        return 0;
    } catch (const std::out_of_range &e) {
        lua_pushfstring(L, "[ObjectMotionBlur] Out of Range: %s", e.what());
        lua_error(L);
        return 0;
    } catch (const std::bad_alloc &e) {
        lua_pushfstring(L, "[ObjectMotionBlur] Memory Allocation Failed: %s", e.what());
        lua_error(L);
        return 0;
    } catch (const std::logic_error &e) {
        lua_pushfstring(L, "[ObjectMotionBlur] Logic Error: %s", e.what());
        lua_error(L);
        return 0;
    } catch (const std::exception &e) {
        lua_pushfstring(L, "[ObjectMotionBlur] Standard Exception: %s", e.what());
        lua_error(L);
        return 0;
    } catch (...) {
        lua_pushstring(L, "[ObjectMotionBlur] Unknown/Non-standard Exception: Unhandled error type");
        lua_error(L);
        return 0;
    }
}