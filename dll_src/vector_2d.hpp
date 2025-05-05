#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#define INF std::numeric_limits<int>::max()

template <typename T>
inline static bool
is_zero(T value) {
    if constexpr (std::is_floating_point<T>::value) {
        constexpr T epsilon = 1e-6f;
        return std::fabs(value) < epsilon;
    } else {
        return value == 0;
    }
}

template <typename T>
class Vec2 {
public:
    // Constructor
    Vec2(T x = T(0), T y = T(0)) : x(x), y(y) {};

    // Overload operators
    Vec2<T> operator+(const Vec2<T> &other) const { return Vec2(x + other.x, y + other.y); }

    Vec2<T> operator-(const Vec2<T> &other) const { return Vec2(x - other.x, y - other.y); }

    Vec2<T> operator*(T scalar) const { return Vec2(x * scalar, y * scalar); }

    Vec2<T> operator/(T scalar) const {
        if (scalar == static_cast<T>(0)) {
            throw std::runtime_error("Division by zero in Vec2.");
        }
        return Vec2(x / scalar, y / scalar);
    }

    template <typename U>
    explicit operator Vec2<U>() const {
        return Vec2<U>(static_cast<U>(x), static_cast<U>(y));
    }

    // Getters
    T get_x() const { return x; }

    T get_y() const { return y; }

    // Setters
    void set_x(T new_x) { x = new_x; }

    void set_y(T new_y) { y = new_y; }

    // Norm calculation
    // ord = 1: Manhattan norm (L1 norm)
    // ord = 2: Euclidean norm (L2 norm)
    // ord = int max: Maximum norm (L∞ norm)
    // Throws std::invalid_argument for unsupported orders
    T norm(int ord = 2) const {
        switch (ord) {
            case 1:
                return std::abs(x) + std::abs(y);
            case 2:
                return std::sqrt(x * x + y * y);
            case INF:
                return std::max(std::abs(x), std::abs(y));
            default:
                throw std::invalid_argument("Unsupported norm order.");
        }
    }

    // Rotation function
    // angle_rad: angle in radians
    // scale: scaling factor
    // Returns a new Vec2 object with the rotated coordinates
    // The rotation is performed around the origin (0, 0) using the standard 2D rotation matrix
    Vec2<T> rotation(T angle_rad = T(0), T scale = T(1)) const {
        if (is_zero(angle_rad)) {
            return Vec2<T>(scale * x, scale * y);
        }
        T cos_angle = std::cos(angle_rad);
        T sin_angle = std::sin(angle_rad);
        return Vec2<T>(scale * (x * cos_angle - y * sin_angle), scale * (x * sin_angle + y * cos_angle));
    }

    // To string.
    std::string to_string() const { return "Vec2(" + std::to_string(x) + ", " + std::to_string(y) + ")"; }

    // cieling function
    Vec2<T> ceil() const { return Vec2<T>(std::ceil(x), std::ceil(y)); }

private:
    T x, y;
};