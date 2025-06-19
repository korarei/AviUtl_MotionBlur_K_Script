#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <type_traits>

template <typename T>
concept arithmetic = std::integral<T> || std::floating_point<T>;

// Define a 2D vector class template.
template <arithmetic T>
class Vec2 {
public:
    // Constructors.
    constexpr Vec2() noexcept : x(T(0)), y(T(0)) {}
    constexpr Vec2(T x, T y) noexcept : x(x), y(y) {}
    constexpr Vec2(const Vec2 &other) noexcept = default;
    constexpr Vec2(Vec2 &&other) noexcept = default;

    // Assignment operators.
    constexpr Vec2 &operator=(const Vec2 &other) noexcept = default;
    constexpr Vec2 &operator=(Vec2 &&other) noexcept = default;

    // Operators.
    [[nodiscard]] constexpr Vec2 operator+(const Vec2 &other) const noexcept { return Vec2(x + other.x, y + other.y); }

    [[nodiscard]] constexpr Vec2 operator-(const Vec2 &other) const noexcept { return Vec2(x - other.x, y - other.y); }

    [[nodiscard]] constexpr Vec2 operator*(T scalar) const noexcept { return Vec2(x * scalar, y * scalar); }

    [[nodiscard]] constexpr Vec2 operator/(T scalar) const {
        if (is_zero(scalar)) {
            throw std::invalid_argument("Division by zero in Vec2.");
        }
        return Vec2(x / scalar, y / scalar);
    }

    // Compound assignment operators.
    constexpr Vec2 &operator+=(const Vec2 &other) noexcept {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr Vec2 &operator-=(const Vec2 &other) noexcept {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    constexpr Vec2 &operator*=(T scalar) noexcept {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    constexpr Vec2 &operator/=(T scalar) {
        if (is_zero(scalar)) {
            throw std::invalid_argument("Division by zero in Vec2.");
        }
        x /= scalar;
        y /= scalar;
        return *this;
    }

    [[nodiscard]] constexpr bool operator==(const Vec2 &other) const noexcept {
        if constexpr (std::is_floating_point_v<T>) {
            return std::abs(x - other.x) <= epsilon() && std::abs(y - other.y) <= epsilon();
        } else {
            return x == other.x && y == other.y;
        }
    }

    [[nodiscard]] constexpr bool operator!=(const Vec2 &other) const noexcept { return !(*this == other); }

    template <arithmetic U>
    [[nodiscard]] explicit constexpr operator Vec2<U>() const noexcept {
        return Vec2<U>(static_cast<U>(x), static_cast<U>(y));
    }

    // Getters.
    [[nodiscard]] constexpr T get_x() const noexcept { return x; }
    [[nodiscard]] constexpr T get_y() const noexcept { return y; }

    // Setters.
    constexpr void set_x(T new_x) noexcept { x = new_x; }
    constexpr void set_y(T new_y) noexcept { y = new_y; }

    // Norm calculation.
    [[nodiscard]] T norm(int ord = 2) const {
        switch (ord) {
            case 1:
                return std::abs(x) + std::abs(y);  // Manhattan norm (L1 norm)
            case 2:
                if constexpr (std::is_floating_point_v<T>)
                    return std::sqrt(x * x + y * y);  // Euclidean norm (L2)
                else
                    return static_cast<T>(std::sqrt(static_cast<double>(x * x + y * y)));
            case -1:
                return std::max(std::abs(x), std::abs(y));  // Maximum norm (L inf norm)
            default:
                throw std::invalid_argument("Unsupported norm order.");
        }
    }

    // Rotation function.
    // The rotation is performed around the origin (0, 0) using the standard 2D rotation matrix.
    [[nodiscard]] Vec2 rotate(T theta = T(0), T factor = T(1)) const {
        if (is_zero(theta))
            return Vec2(factor * x, factor * y);

        if constexpr (std::is_floating_point_v<T>) {
            T cos_t = std::cos(theta);
            T sin_t = std::sin(theta);
            return Vec2(factor * (x * cos_t - y * sin_t), factor * (x * sin_t + y * cos_t));
        } else {
            double cos_t = std::cos(static_cast<double>(theta));
            double sin_t = std::sin(static_cast<double>(theta));
            double fx = static_cast<double>(factor * x);
            double fy = static_cast<double>(factor * y);
            return Vec2(static_cast<T>(fx * cos_t - fy * sin_t), static_cast<T>(fx * sin_t + fy * cos_t));
        }
    }

    // Ceiling function.
    [[nodiscard]] Vec2 ceil() const
        requires std::is_floating_point_v<T>
    {
        return Vec2(std::ceil(x), std::ceil(y));
    }

    // Floor function.
    [[nodiscard]] Vec2 floor() const
        requires std::is_floating_point_v<T>
    {
        return Vec2(std::floor(x), std::floor(y));
    }

    // epsilon value for floating-point comparison.
    [[nodiscard]] static constexpr T epsilon() noexcept {
        if constexpr (std::is_floating_point_v<T>) {
            return std::numeric_limits<T>::epsilon() * 1000;
        } else {
            return T(0);
        }
    }

    // Check if the value is zero.
    [[nodiscard]] static constexpr bool is_zero(T val) noexcept {
        if constexpr (std::is_floating_point_v<T>) {
            return std::abs(val) <= epsilon();
        } else {
            return val == T(0);
        }
    }

private:
    T x, y;
};

// Overload the multiplication operator for scalar multiplication.
template <arithmetic T>
[[nodiscard]] constexpr Vec2<T>
operator*(T scalar, const Vec2<T> &vec) noexcept {
    return vec * scalar;
}

// Overload the output stream operator for Vec2.
template <arithmetic T>
std::ostream &
operator<<(std::ostream &os, const Vec2<T> &vec) {
    os << "Vec2(" << vec.get_x() << ", " << vec.get_y() << ")";
    return os;
}

// Define a 2x2 matrix class using Vec2.
template <arithmetic T>
class Mat2 {
public:
    // Constructors.
    constexpr Mat2() noexcept : cols{Vec2<T>(T(1), T(0)), Vec2<T>(T(0), T(1))} {}
    constexpr Mat2(const Vec2<T> &c0, const Vec2<T> &c1) noexcept : cols{c0, c1} {}
    constexpr Mat2(T a11, T a12, T a21, T a22) noexcept : cols{Vec2<T>(a11, a21), Vec2<T>(a12, a22)} {}
    constexpr Mat2(const Mat2<T> &other) noexcept = default;
    constexpr Mat2(Mat2<T> &&other) noexcept = default;

    // Assignment operators.
    constexpr Mat2 &operator=(const Mat2<T> &other) noexcept = default;
    constexpr Mat2 &operator=(Mat2<T> &&other) noexcept = default;

    // Matrix-vector multiplication.
    [[nodiscard]] constexpr Vec2<T> operator*(const Vec2<T> &v) const noexcept {
        return Vec2<T>(cols[0].get_x() * v.get_x() + cols[1].get_x() * v.get_y(),
                       cols[0].get_y() * v.get_x() + cols[1].get_y() * v.get_y());
    }

    // Matrix-matrix multiplication.
    [[nodiscard]] constexpr Mat2 operator*(const Mat2 &m) const noexcept {
        return Mat2<T>((*this) * m.get_col(0), (*this) * m.get_col(1));
    }

    // Scalar multiplication.
    [[nodiscard]] constexpr Mat2 operator*(T scalar) const noexcept {
        return Mat2<T>(cols[0] * scalar, cols[1] * scalar);
    }

    // Getters.
    [[nodiscard]] constexpr Vec2<T> get_col(int idx) const {
        if (idx < 0 || idx >= 2) {
            throw std::out_of_range("Invalid column index");
        }
        return cols[idx];
    }

    [[nodiscard]] constexpr Vec2<T> get_row(int idx) const {
        switch (idx) {
            case 0:
                return Vec2<T>(cols[0].get_x(), cols[1].get_x());
            case 1:
                return Vec2<T>(cols[0].get_y(), cols[1].get_y());
            default:
                throw std::out_of_range("Invalid row index");
        }
    }

    [[nodiscard]] constexpr T get_elem(int row, int col) const {
        if (col < 0 || col >= 2 || row < 0 || row >= 2) {
            throw std::out_of_range("Invalid matrix indices");
        }
        return row == 0 ? cols[col].get_x() : cols[col].get_y();
    }

    // Setters.
    constexpr void set_col(int idx, const Vec2<T> &col) {
        if (idx < 0 || idx >= 2) {
            throw std::out_of_range("Invalid column index");
        }
        cols[idx] = col;
    }

    constexpr void set_row(int idx, const Vec2<T> &row) {
        switch (idx) {
            case 0:
                cols[0].set_x(row.get_x());
                cols[1].set_x(row.get_y());
                break;
            case 1:
                cols[0].set_y(row.get_x());
                cols[1].set_y(row.get_y());
                break;
            default:
                throw std::out_of_range("Invalid row index");
        }
    }

    constexpr void set_elem(int row, int col, T val) {
        if (col < 0 || col >= 2 || row < 0 || row >= 2) {
            throw std::out_of_range("Invalid matrix indices");
        }
        if (row == 0) {
            cols[col].set_x(val);
        } else {
            cols[col].set_y(val);
        }
    }

    // Transpose the matrix.
    [[nodiscard]] constexpr Mat2 transpose() const noexcept {
        return Mat2(Vec2<T>(cols[0].get_x(), cols[1].get_x()), 
                   Vec2<T>(cols[0].get_y(), cols[1].get_y()));
    }

    // Calculate the determinant.
    // The determinant of a 2x2 matrix is calculated as:
    // det(A) = a11 * a22 - a12 * a21
    [[nodiscard]] constexpr T determinant() const noexcept {
        return cols[0].get_x() * cols[1].get_y() - cols[0].get_y() * cols[1].get_x();
    }

    // Calculate the inverse of the matrix.
    // The inverse of a 2x2 matrix is calculated as:
    // A^-1 = (1/det(A)) * [[a22, -a12], [-a21, a11]]
    [[nodiscard]] Mat2 inverse() const {
        T det = determinant();
        if (Vec2<T>::is_zero(det)) {
            throw std::runtime_error("Matrix is singular and cannot be inverted.");
        }

        T inv_det = T{1} / det;
        return Mat2(Vec2<T>(cols[1].get_y() * inv_det, -cols[0].get_y() * inv_det),
                   Vec2<T>(-cols[1].get_x() * inv_det, cols[0].get_x() * inv_det));
    }

    // Create rotation matrix.
    [[nodiscard]] static Mat2 rotation(T theta) {
        if constexpr (std::is_floating_point_v<T>) {
            T cos_t = std::cos(theta);
            T sin_t = std::sin(theta);
            return Mat2(Vec2<T>(cos_t, sin_t), Vec2<T>(-sin_t, cos_t));
        } else {
            double cos_t = std::cos(static_cast<double>(theta));
            double sin_t = std::sin(static_cast<double>(theta));
            return Mat2(Vec2<T>(static_cast<T>(cos_t), static_cast<T>(sin_t)),
                       Vec2<T>(static_cast<T>(-sin_t), static_cast<T>(cos_t)));
        }
    }

private:
    std::array<Vec2<T>, 2> cols;
};

// Overload the multiplication operator for scalar multiplication of Mat2.
template <arithmetic T>
[[nodiscard]] constexpr Mat2<T> operator*(T scalar, const Mat2<T> &m) noexcept {
    return m * scalar;
}
