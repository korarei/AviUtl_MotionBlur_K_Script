#pragma once

#include "vector_2d.hpp"

template <vec_base::arithmetic T>
class Vec3 : public vec_base::VecBase<Vec3<T>, 3, T> {
public:
    // Constructors.
    constexpr Vec3() noexcept : super() {}
    constexpr Vec3(T x, T y, T z) noexcept : super(x, y, z) {}
    explicit constexpr Vec3(const Vec2<T> &vec, T z = T{0}) noexcept : super(vec[0], vec[1], z) {}
    constexpr Vec3(const Vec3 &other) noexcept = default;
    constexpr Vec3(Vec3 &&other) noexcept = default;

    // Assignment operators.
    constexpr Vec3 &operator=(const Vec3 &other) noexcept = default;
    constexpr Vec3 &operator=(Vec3 &&other) noexcept = default;

    // Type conversion operator.
    template <vec_base::arithmetic U>
    [[nodiscard]] explicit constexpr operator Vec3<U>() const noexcept {
        return Vec3<U>(static_cast<U>(get_x()), static_cast<U>(get_y()), static_cast<U>(get_z()));
    }

    // Getters.
    [[nodiscard]] constexpr T get_x() const noexcept { return this->data[0]; }
    [[nodiscard]] constexpr T get_y() const noexcept { return this->data[1]; }
    [[nodiscard]] constexpr T get_z() const noexcept { return this->data[2]; }

    // Setters.
    constexpr void set_x(T new_x) noexcept { this->data[0] = new_x; }
    constexpr void set_y(T new_y) noexcept { this->data[1] = new_y; }
    constexpr void set_z(T new_z) noexcept { this->data[2] = new_z; }

private:
    using super = vec_base::VecBase<Vec3<T>, 3, T>;
};

// Overload the multiplication operator for scalar multiplication.
template <vec_base::arithmetic T>
[[nodiscard]] constexpr Vec3<T>
operator*(T scalar, const Vec3<T> &vec) noexcept {
    return vec * scalar;
}

template <vec_base::arithmetic T>
class Mat3 : public vec_base::MatBase<Mat3<T>, Vec3<T>, 3, T> {
public:
    // Constructors.
    constexpr Mat3() noexcept : super() {}
    constexpr Mat3(const Vec3<T> &c0, const Vec3<T> &c1, const Vec3<T> &c2) noexcept : super(c0, c1, c2) {}
    constexpr explicit Mat3(const Mat2<T> &m2) noexcept :
        super(Vec3<T>(m2[0]), Vec3<T>(m2[1]), Vec3<T>(T{0}, T{0}, T{1})) {}
    constexpr Mat3(const Mat2<T> &m2, const Vec3<T> &c2) noexcept : super(Vec3<T>(m2[0]), Vec3<T>(m2[1]), c2) {}
    constexpr Mat3(T a11, T a12, T a13, T a21, T a22, T a23, T a31, T a32, T a33) noexcept :
        super(Vec3<T>(a11, a21, a31), Vec3<T>(a12, a22, a32), Vec3<T>(a13, a23, a33)) {}
    constexpr Mat3(const Mat3 &other) noexcept = default;
    constexpr Mat3(Mat3 &&other) noexcept = default;

    // Assignment operators.
    constexpr Mat3 &operator=(const Mat3 &other) noexcept = default;
    constexpr Mat3 &operator=(Mat3 &&other) noexcept = default;

    // Create identity matrix.
    [[nodiscard]] static constexpr Mat3 identity() {
        return Mat3(T{1}, T{0}, T{0}, T{0}, T{1}, T{0}, T{0}, T{0}, T{1});
    }

    // Create rotation matrix.
    [[nodiscard]] static constexpr Mat3 rotation(T theta, int axis = 2, T scale = T{1})
        requires std::floating_point<T>
    {
        T c = std::cos(theta) * scale;
        T s = std::sin(theta) * scale;

        switch (axis) {
            case 0:
                return Mat3(T{1}, T{0}, T{0}, T{0}, c, -s, T{0}, s, c);
            case 1:
                return Mat3(c, T{0}, s, T{0}, T{1}, T{0}, -s, T{0}, c);
            case 2:
                return Mat3(c, -s, T{0}, s, c, T{0}, T{0}, T{0}, T{1});
            default:
                throw std::invalid_argument("Unsupported axis.");
        }
    }

private:
    using super = vec_base::MatBase<Mat3<T>, Vec3<T>, 3, T>;
};

// Overload the multiplication operator for scalar multiplication of Mat3.
template <vec_base::arithmetic T>
[[nodiscard]] constexpr Mat3<T>
operator*(T scalar, const Mat3<T> &m) noexcept {
    return m * scalar;
}
