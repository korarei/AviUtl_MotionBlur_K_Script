#include "lua_func.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

// Parameters for object motion blur
ObjectMotionBlurParams::ObjectMotionBlurParams(lua_State *L, bool is_saving) :
    shutter_angle(lua_isnumber(L, 1) ? std::clamp(static_cast<float>(lua_tonumber(L, 1)), 0.0f, 720.0f) : 180.0f),
    shutter_phase(lua_isnumber(L, 2) ? std::clamp(static_cast<float>(lua_tonumber(L, 2)), -360.0f, 360.0f) : -90.0f),
    render_samp_lim(lua_isnumber(L, 3) ? std::max(static_cast<int>(lua_tointeger(L, 3)), 1) : 256),
    preview_samp_lim(lua_isnumber(L, 4) ? std::max(static_cast<int>(lua_tointeger(L, 4)), 0) : 0),
    mix_orig_img(lua_isboolean(L, 5) ? lua_toboolean(L, 5) : false),
    use_geo(lua_isboolean(L, 6) ? lua_toboolean(L, 6) : false),
    geo_cleanup_method(lua_isnumber(L, 7) ? lua_tointeger(L, 7) : 1),
    save_all_geo(lua_isboolean(L, 8) ? lua_toboolean(L, 8) : true),
    keep_size(lua_isboolean(L, 9) ? lua_toboolean(L, 9) : false),
    calc_neg_f(lua_isboolean(L, 10) ? lua_toboolean(L, 10) : true),
    reload_shader(lua_isboolean(L, 11) ? lua_toboolean(L, 11) : false),
    print_info(lua_isboolean(L, 12) ? lua_toboolean(L, 12) : false),
    shader_dir(lua_isstring(L, 13) ? lua_tostring(L, 13) : "\\shaders"),
    samp_lim((preview_samp_lim != 0 && !is_saving) ? preview_samp_lim : render_samp_lim) {}

// Enable the use of GLShaderKit in C++
GLShaderKit::GLShaderKit(lua_State *L) : L(L) {
    lua_getglobal(L, "require");
    lua_pushstring(L, "GLShaderKit");
    lua_call(L, 1, 1);
}

Image
GLShaderKit::get_image() const {
    lua_getglobal(L, "obj");
    lua_getfield(L, -1, "getpixeldata");
    lua_call(L, 0, 3);
    Image img;
    img.size.set_x(lua_tointeger(L, -2));
    img.size.set_y(lua_tointeger(L, -1));
    img.data = reinterpret_cast<ExEdit::PixelBGRA *>(lua_touserdata(L, -3));
    lua_pop(L, 3);
    lua_getfield(L, -1, "cx");
    img.center.set_x(static_cast<float>(lua_tonumber(L, -1)));
    lua_pop(L, 1);
    lua_getfield(L, -1, "cy");
    img.center.set_y(static_cast<float>(lua_tonumber(L, -1)));
    lua_pop(L, 2);
    return img;
}

void
GLShaderKit::putpixeldata(void *data) const {
    lua_getglobal(L, "obj");
    lua_getfield(L, -1, "putpixeldata");
    lua_pushlightuserdata(L, data);
    lua_call(L, 1, 0);
    lua_pop(L, 1);
}

bool
GLShaderKit::isInitialized() const {
    lua_getfield(L, -1, "isInitialized");
    lua_call(L, 0, 1);
    bool value = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return value;
}

bool
GLShaderKit::activate() const {
    lua_getfield(L, -1, "activate");
    lua_call(L, 0, 1);
    bool value = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return value;
}

void
GLShaderKit::deactivate() const {
    lua_getfield(L, -1, "deactivate");
    lua_call(L, 0, 0);
    lua_pop(L, 1);
}

void
GLShaderKit::setPlaneVertex(int n) const {
    lua_getfield(L, -1, "setPlaneVertex");
    lua_pushinteger(L, n);
    lua_call(L, 1, 0);
}

void
GLShaderKit::setShader(const std::string &shader_path, bool force_reload) const {
    lua_getfield(L, -1, "setShader");
    lua_pushstring(L, shader_path.c_str());
    lua_pushboolean(L, force_reload);
    lua_call(L, 2, 0);
}

void
GLShaderKit::setTexture2D(int unit, const Image &img) const {
    lua_getfield(L, -1, "setTexture2D");
    lua_pushinteger(L, unit);
    lua_pushlightuserdata(L, img.data);
    lua_pushinteger(L, img.size.get_x());
    lua_pushinteger(L, img.size.get_y());
    lua_call(L, 4, 0);
}

void
GLShaderKit::setFloat(std::string name, const std::vector<float> &vec) const {
    lua_getfield(L, -1, "setFloat");
    lua_pushstring(L, name.c_str());
    for (float value : vec) lua_pushnumber(L, value);

    lua_call(L, vec.size() + 1, 0);
}

void
GLShaderKit::setInt(std::string name, const std::vector<int> &vec) const {
    lua_getfield(L, -1, "setInt");
    lua_pushstring(L, name.c_str());
    for (int value : vec) lua_pushinteger(L, value);

    lua_call(L, vec.size() + 1, 0);
}

void
GLShaderKit::setMatrix(std::string name, std::string type, bool transpose, float angle_rad) const {
    lua_getfield(L, -1, "setMatrix");
    lua_pushstring(L, name.c_str());
    lua_pushstring(L, type.c_str());
    lua_pushboolean(L, transpose);

    float cos = std::cos(angle_rad);
    float sin = std::sin(angle_rad);

    lua_newtable(L);

    lua_pushnumber(L, cos);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, sin);
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, -sin);
    lua_rawseti(L, -2, 3);
    lua_pushnumber(L, cos);
    lua_rawseti(L, -2, 4);

    lua_call(L, 4, 0);
}

void GLShaderKit::setMat3(const char *name, bool transpose, const Mat3<float> &mat3) const {
    lua_getfield(L, -1, "setMatrix");
    lua_pushstring(L, name);
    lua_pushstring(L, "3x3");
    lua_pushboolean(L, transpose);

    lua_newtable(L);

    lua_pushnumber(L, mat3(0, 0));
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, mat3(0, 1));
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, mat3(0, 2));
    lua_rawseti(L, -2, 3);
    lua_pushnumber(L, mat3(1, 0));
    lua_rawseti(L, -2, 4);
    lua_pushnumber(L, mat3(1, 1));
    lua_rawseti(L, -2, 5);
    lua_pushnumber(L, mat3(1, 2));
    lua_rawseti(L, -2, 6);
    lua_pushnumber(L, mat3(2, 0));
    lua_rawseti(L, -2, 7);
    lua_pushnumber(L, mat3(2, 1));
    lua_rawseti(L, -2, 8);
    lua_pushnumber(L, mat3(2, 2));
    lua_rawseti(L, -2, 9);

    lua_call(L, 4, 0);
}

void
GLShaderKit::draw(std::string mode, Image &img) const {
    lua_getfield(L, -1, "draw");
    lua_pushstring(L, mode.c_str());
    lua_pushlightuserdata(L, img.data);
    lua_pushinteger(L, img.size.get_x());
    lua_pushinteger(L, img.size.get_y());
    lua_call(L, 4, 0);
}

void
GLShaderKit::setParamsForOMBStep(const std::string &name, const Steps &steps) const {
    std::string pos_param = "step_pos_" + name;
    std::string scale_param = "step_scale_" + name;
    std::string rot_param = "step_rot_mat_" + name;

    setFloat(pos_param.c_str(), {steps.location.get_x(), steps.location.get_y()});
    setFloat(scale_param.c_str(), {steps.scale});
    setMatrix(rot_param.c_str(), "2x2", false, steps.rz_rad);
}

void
expand_image(const std::array<int, 4> &expansion, lua_State *L) {
    lua_getglobal(L, "obj");
    lua_getfield(L, -1, "effect");
    lua_pushstring(L, "領域拡張");
    lua_pushstring(L, "上");
    lua_pushinteger(L, expansion[0]);
    lua_pushstring(L, "下");
    lua_pushinteger(L, expansion[1]);
    lua_pushstring(L, "左");
    lua_pushinteger(L, expansion[2]);
    lua_pushstring(L, "右");
    lua_pushinteger(L, expansion[3]);
    lua_call(L, 9, 0);
    lua_pop(L, 1);
}