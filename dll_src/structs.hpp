#pragma once

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
    int32_t ox, oy;
    int32_t cx, cy;  // For the future.
    int32_t zoom;
    int32_t rz;

    constexpr Geometry() : ox(0), oy(0), cx(0), cy(0), zoom(0), rz(0) {}

    constexpr Geometry(int32_t ox_, int32_t oy_, int32_t cx_, int32_t cy_, int32_t zoom_, int32_t rz_) :
        ox(ox_), oy(oy_), cx(cx_), cy(cy_), zoom(zoom_), rz(rz_) {}
};

// A structure to store data for each segment.
template <typename T>
struct SegmentData {
    std::optional<T> seg1;
    std::optional<T> seg2;
    std::optional<T> offset;

    SegmentData() : seg1(std::nullopt), seg2(std::nullopt), offset(std::nullopt) {}
};

// The corner coordinates of the object in each frame as seen from the current object.
struct Corner {
    Vec2<float> location;
    Vec2<int> upper_left;
    Vec2<int> lower_right;

    Corner() : location(0.0f, 0.0f), upper_left(0, 0), lower_right(0, 0) {}
};

// Blur step.
struct Steps {
    Vec2<float> location;
    float scale, rz_rad;
};