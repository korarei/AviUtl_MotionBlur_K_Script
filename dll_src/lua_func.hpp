#pragma once

#include <array>
#include <cstdint>
#include <lua.hpp>
#include <string>
#include <vector>

#include "structs.hpp"
#include "vector_2d.hpp"

struct ObjectMotionBlurParams {
    const float shutter_angle;
    const float shutter_phase;
    const int render_sample_limit;
    const int preview_sample_limit;
    const bool is_orig_img_visible;
    const bool is_using_geometry_enabled;
    const int geometry_data_cleanup_method;
    const bool is_saving_all_geometry_enabled;
    const bool is_keeping_size_enabled;
    const bool is_calc_neg1f_and_neg2f_enabled;
    const bool is_reload_enabled;
    const bool is_printing_info_enabled;
    const std::string shader_folder;
    int sample_limit;

    ObjectMotionBlurParams(lua_State *L, bool is_saving);
};

class GLShaderKit {
public:
    explicit GLShaderKit(lua_State *L);

    Image get_image() const;
    void putpixeldata(void *data) const;

    bool isInitialized() const;
    bool activate() const;
    void deactivate() const;
    void setPlaneVertex(int n) const;
    void setShader(std::string shader_path, bool force_reload) const;
    void setTexture2D(int unit, const Image &img) const;
    void setFloat(std::string name, const std::vector<float> &vec) const;
    void setInt(std::string name, const std::vector<int> &vec) const;
    void setMatrix(std::string name, std::string type, bool transpose, float angle_rad) const;
    void draw(std::string mode, Image &img) const;

    void setParamsForOMBStep(const std::string &name, const Steps &steps) const; // OMBStep: Object Motion Blur Step

private:
    lua_State *L;
};

void
expand_image(const std::array<int, 4> &expansion, lua_State *L);