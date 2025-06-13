#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <type_traits>

template <typename T>
class Vec2 {
public:
    // Constructor.
    Vec2() : x(0), y(0) {}
    Vec2(T x, T y) : x(x), y(y) {}
    Vec2(const Vec2 &other) : x(other.x), y(other.y) {}

    // Overload operators.
    Vec2 &operator=(const Vec2 &other) {
        if (this != &other) {
            x = other.x;
            y = other.y;
        }
        return *this;
    }

    Vec2 operator+(const Vec2 &other) const { return Vec2(x + other.x, y + other.y); }

    Vec2 operator-(const Vec2 &other) const { return Vec2(x - other.x, y - other.y); }

    Vec2 operator*(T scalar) const { return Vec2(x * scalar, y * scalar); }

    Vec2 operator/(T scalar) const {
        if (is_zero(scalar))
            throw std::invalid_argument("Division by zero in Vec2.");

        return Vec2(x / scalar, y / scalar);
    }

    Vec2 &operator+=(const Vec2 &other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    Vec2 &operator-=(const Vec2 &other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    Vec2 &operator*=(T scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    Vec2 &operator/=(T scalar) {
        if (is_zero(scalar))
            throw std::invalid_argument("Division by zero in Vec2.");

        x /= scalar;
        y /= scalar;
        return *this;
    }

    bool operator==(const Vec2 &other) const {
        if constexpr (std::is_floating_point_v<T>) {
            return std::abs(x - other.x) <= epsilon() && std::abs(y - other.y) <= epsilon();
        } else {
            return x == other.x && y == other.y;
        }
    }

    bool operator!=(const Vec2 &other) const { return !(*this == other); }

    template <typename U>
    explicit operator Vec2<U>() const {
        return Vec2<U>(static_cast<U>(x), static_cast<U>(y));
    }

    // Getters.
    T get_x() const { return x; }

    T get_y() const { return y; }

    // Setters.
    void set_x(T new_x) { x = new_x; }

    void set_y(T new_y) { y = new_y; }

    // Norm calculation.
    T norm(int ord = 2) const {
        switch (ord) {
            case 1:
                return std::abs(x) + std::abs(y);  // Manhattan norm (L1 norm)
            case 2:
                return std::sqrt(x * x + y * y);  // Euclidean norm (L2 norm)
            case -1:
                return std::max(std::abs(x), std::abs(y));  // Maximum norm (L inf norm)
            default:
                throw std::invalid_argument("Unsupported norm order.");
        }
    }

    // Rotation function.
    // The rotation is performed around the origin (0, 0) using the standard 2D rotation matrix.
    Vec2 rotate(T theta = T(0), T factor = T(1)) const {
        if (is_zero(theta))
            return Vec2(factor * x, factor * y);

        T cos_t = std::cos(theta);
        T sin_t = std::sin(theta);
        return Vec2(factor * (x * cos_t - y * sin_t), factor * (x * sin_t + y * cos_t));
    }

    // Ceiling function.
    Vec2 ceil() const { return Vec2(std::ceil(x), std::ceil(y)); }

private:
    T x, y;

    // epsilon value for floating-point comparison.
    static constexpr T epsilon() {
        if constexpr (std::is_floating_point_v<T>) {
            return std::numeric_limits<T>::epsilon() * 100;  // Adjusted for better precision in comparisons.
        } else {
            return T(0);
        }
    }

    // Check if the value is zero.
    static bool is_zero(T value) {
        if constexpr (std::is_floating_point_v<T>) {
            return std::abs(value) <= epsilon();
        } else {
            return value == T(0);
        }
    }
};

// Overload the multiplication operator for scalar multiplication.
template<typename T>
Vec2<T> operator*(T scalar, const Vec2<T>& vec) {
    return vec * scalar;
}

// Overload the output stream operator for Vec2.
template <typename T>
std::ostream &
operator<<(std::ostream &os, const Vec2<T> &vec) {
    os << "Vec2(" << vec.get_x() << ", " << vec.get_y() << ")";
    return os;
}