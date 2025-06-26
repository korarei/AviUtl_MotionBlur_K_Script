#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <lua.hpp>

#include "structs.hpp"
#include "vector_2d.hpp"
#include "vector_3d.hpp"

struct ObjectMotionBlurParams {
    const float shutter_angle;
    const float shutter_phase;
    const int render_samp_lim;
    const int preview_samp_lim;
    const bool mix_orig_img;
    const bool use_geo;
    const int geo_cleanup_method;
    const bool save_all_geo;
    const bool keep_size;
    const bool calc_neg_f;
    const bool reload_shader;
    const bool print_info;
    const std::filesystem::path shader_dir;
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
    void setShader(const std::string &shader_path, bool force_reload) const;
    void setTexture2D(int unit, const Image &img) const;
    void setFloat(std::string name, const std::vector<float> &vec) const;
    void setInt(std::string name, const std::vector<int> &vec) const;
    void setMatrix(std::string name, std::string type, bool transpose, float angle_rad) const;
    void setMat3(const char *name, bool transpose, const Mat3<float> &mat3) const;
    void draw(std::string mode, Image &img) const;

    void setParamsForOMBStep(const std::string &name, const Steps &steps) const;  // OMBStep: Object Motion Blur Step

private:
    lua_State *L;
};

void
expand_image(const std::array<int, 4> &expansion, lua_State *L);