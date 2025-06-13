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
    const int render_samp_lim;
    const int preview_samp_lim;
    const bool is_orig_img_mixed;
    const bool is_geo_used;
    const int geo_cleanup_method;
    const bool is_all_geo_saved;
    const bool is_img_size_keeped;
    const bool is_neg_frame_simulated;
    const bool is_shader_reloaded;
    const bool is_info_printed;
    const std::string shader_dir;
    const int samp_lim;

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