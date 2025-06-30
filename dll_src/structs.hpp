#pragma once

#include <optional>
#define NOMINMAX
#include <exedit.hpp>

#include "vector_2d.hpp"

// Image data.
struct Image {
    Vec2<int> size;
    Vec2<float> center;
    ExEdit::PixelBGRA *data;
};

// obj.ox etc.
struct Geometry {
    bool is_valid;
    int32_t ox, oy;
    int32_t cx, cy;  // For the future.
    int32_t zoom;
    int32_t rz;

    constexpr Geometry() : is_valid(true), ox(0), oy(0), cx(0), cy(0), zoom(0), rz(0) {}

    constexpr Geometry(int32_t ox_, int32_t oy_, int32_t cx_, int32_t cy_, int32_t zoom_, int32_t rz_) :
        is_valid(true), ox(ox_), oy(oy_), cx(cx_), cy(cy_), zoom(zoom_), rz(rz_) {}
};

// The structure to store data for each segment.
template <typename T>
struct SegData {
    T seg1;
    T seg2;
};

template <typename T>
struct OptSegData {
    std::optional<T> seg1;
    std::optional<T> seg2;

    OptSegData() : seg1(std::nullopt), seg2(std::nullopt) {}
    OptSegData(std::optional<T> s1, std::optional<T> s2) : seg1(s1), seg2(s2) {}
};

template <typename T>
struct MappingData {
    std::optional<T> offset;
    std::optional<T> seg1;
    std::optional<T> seg2;

    MappingData() : offset(std::nullopt), seg1(std::nullopt), seg2(std::nullopt) {}
};
