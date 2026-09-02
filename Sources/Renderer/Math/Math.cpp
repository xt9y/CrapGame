#include "Math.hpp"

#include <cmath>

namespace Renderer 
{
namespace Math 
{
namespace 
{

constexpr float PI = 3.14159265358979323846f;
constexpr float EPSILON = 0.000001f;

Vec3 rotateX (const Vec3& value, float degrees) 
{
    const float angle = radians(degrees),
                sine  = std::sin(angle),
                cosine = std::cos(angle);

    return {
        value.x,
        value.y * cosine - value.z * sine,
        value.y * sine + value.z * cosine,
    };
}

Vec3 rotateY (const Vec3& value, float degrees) 
{
    const float angle = radians(degrees),
                sine  = std::sin(angle),
                cosine = std::cos(angle);

    return {
        value.x * cosine + value.z * sine,
        value.y,
        -value.x * sine + value.z * cosine,
    };
}

Vec3 rotateZ (const Vec3& value, float degrees) 
{
    const float angle = radians(degrees),
                sine  = std::sin(angle),
                cosine = std::cos(angle);

    return {
        value.x * cosine - value.y * sine,
        value.x * sine + value.y * cosine,
        value.z,
    };
}

} // namespace

float radians (float degrees) 
{
    return degrees * PI / 180.0f;
}

float clamp (float value, float minimum, float maximum) 
{
    if (value < minimum) 
    {
        return minimum;
    }

    if (value > maximum) 
    {
        return maximum;
    }

    return value;
}

float saturate (float value) 
{
    return clamp(value, 0.0f, 1.0f);
}

float dot (const Vec3& a, const Vec3& b) 
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float lengthSquared (const Vec3& value) 
{
    return dot(value, value);
}

float length (const Vec3& value) 
{
    return std::sqrt(lengthSquared(value));
}

Vec3 add (const Vec3& a, const Vec3& b) 
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 subtract (const Vec3& a, const Vec3& b) 
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 multiply (const Vec3& value, float scalar) 
{
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

Vec3 multiply (const Vec3& a, const Vec3& b) 
{
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}

Vec3 divide (const Vec3& value, float scalar) 
{
    if (std::fabs(scalar) <= EPSILON) 
    {
        return {0.0f, 0.0f, 0.0f};
    }

    const float inverse = 1.0f / scalar;
    return multiply(value, inverse);
}

Vec3 cross (const Vec3& a, const Vec3& b) 
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

Vec3 normalize (const Vec3& value) 
{
    const float value_length = length(value);

    if (value_length <= EPSILON) 
    {
        return {0.0f, 0.0f, 0.0f};
    }

    return divide(value, value_length);
}

Vec3 reflect (const Vec3& direction, const Vec3& normal) 
{
    return subtract(
            direction,
            multiply(normal, 2.0f * dot(direction, normal))
        );
}

Vec3 mix (const Vec3& a, const Vec3& b, float amount) 
{
    const float t = saturate(amount);
    return add(multiply(a, 1.0f - t), multiply(b, t));
}

Mat4 identity () 
{
    Mat4 result = {};
    result.value[0]  = 1.0f;
    result.value[5]  = 1.0f;
    result.value[10] = 1.0f;
    result.value[15] = 1.0f;
    return result;
}

Mat4 multiply (const Mat4& a, const Mat4& b) 
{
    Mat4 result = {};

    for (std::size_t column = 0; column < 4; ++column) 
    {
        for (std::size_t row = 0; row < 4; ++row) 
        {
            float value = 0.0f;

            for (std::size_t index = 0; index < 4; ++index) 
            {
                value += 
                    a.value[index * 4 + row] * 
                    b.value[column * 4 + index];
            }

            result.value[column * 4 + row] = value;
        }
    }

    return result;
}

Mat4 translation (const Vec3& value) 
{
    Mat4 result = identity();
    result.value[12] = value.x;
    result.value[13] = value.y;
    result.value[14] = value.z;
    return result;
}

Mat4 scaling (const Vec3& value) 
{
    Mat4 result = {};
    result.value[0]  = value.x;
    result.value[5]  = value.y;
    result.value[10] = value.z;
    result.value[15] = 1.0f;
    return result;
}

Mat4 rotationX (float degrees) 
{
    const float angle = radians(degrees),
                sine = std::sin(angle),
                cosine = std::cos(angle);

    Mat4 result = identity();
    result.value[5]  = cosine;
    result.value[6]  = sine;
    result.value[9]  = -sine;
    result.value[10] = cosine;
    return result;
}

Mat4 rotationY (float degrees) 
{
    const float angle = radians(degrees),
                sine = std::sin(angle),
                cosine = std::cos(angle);

    Mat4 result = identity();
    result.value[0]  = cosine;
    result.value[2]  = -sine;
    result.value[8]  = sine;
    result.value[10] = cosine;
    return result;
}

Mat4 rotationZ (float degrees) 
{
    const float angle = radians(degrees),
                sine = std::sin(angle),
                cosine = std::cos(angle);

    Mat4 result = identity();
    result.value[0] = cosine;
    result.value[1] = sine;
    result.value[4] = -sine;
    result.value[5] = cosine;
    return result;
}

Mat4 rotationEuler (const Vec3& degrees) 
{
    return multiply(
            rotationY(degrees.y),
            multiply(rotationX(degrees.x), rotationZ(degrees.z))
        );
}

Mat4 transform (
                const Vec3& position,
                const Vec3& rotation,
                const Vec3& scale
        ) 
{
    return multiply(
            translation(position),
            multiply(rotationEuler(rotation), scaling(scale))
        );
}

Mat4 perspective (
                float fov_degrees,
                float aspect,
                float near_plane,
                float far_plane
        ) 
{
    const float tangent = 
        std::tan(radians(fov_degrees) * 0.5f);

    Mat4 result = {};

    if (aspect <= EPSILON 
            || tangent <= EPSILON 
            || far_plane <= near_plane) 
    {
        return result;
    }

    result.value[0]  = 1.0f / (aspect * tangent);
    result.value[5]  = 1.0f / tangent;
    result.value[10] = -(far_plane + near_plane) / (far_plane - near_plane);
    result.value[11] = -1.0f;
    result.value[14] = -(2.0f * far_plane * near_plane) / (far_plane - near_plane);
    return result;
}

Mat4 lookAt (
                const Vec3& eye,
                const Vec3& center,
                const Vec3& up
        ) 
{
    const Vec3 forward = 
        normalize(subtract(center, eye));

    const Vec3 side = 
        normalize(cross(forward, up));

    const Vec3 corrected_up = 
        cross(side, forward);

    Mat4 result = identity();
    result.value[0]  = side.x;
    result.value[4]  = side.y;
    result.value[8]  = side.z;
    result.value[1]  = corrected_up.x;
    result.value[5]  = corrected_up.y;
    result.value[9]  = corrected_up.z;
    result.value[2]  = -forward.x;
    result.value[6]  = -forward.y;
    result.value[10] = -forward.z;
    result.value[12] = -dot(side, eye);
    result.value[13] = -dot(corrected_up, eye);
    result.value[14] = dot(forward, eye);
    return result;
}

Vec4 transform (const Mat4& matrix, const Vec4& value) 
{
    return {
        matrix.value[0] * value.x + matrix.value[4] * value.y + matrix.value[8]  * value.z + matrix.value[12] * value.w,
        matrix.value[1] * value.x + matrix.value[5] * value.y + matrix.value[9]  * value.z + matrix.value[13] * value.w,
        matrix.value[2] * value.x + matrix.value[6] * value.y + matrix.value[10] * value.z + matrix.value[14] * value.w,
        matrix.value[3] * value.x + matrix.value[7] * value.y + matrix.value[11] * value.z + matrix.value[15] * value.w,
    };
}

Vec3 transformPoint (const Mat4& matrix, const Vec3& value) 
{
    const Vec4 transformed = 
        transform(matrix, {value.x, value.y, value.z, 1.0f});

    if (std::fabs(transformed.w) <= EPSILON) 
    {
        return {transformed.x, transformed.y, transformed.z};
    }

    return {
        transformed.x / transformed.w,
        transformed.y / transformed.w,
        transformed.z / transformed.w,
    };
}

Vec3 transformDirection (const Mat4& matrix, const Vec3& value) 
{
    const Vec4 transformed = 
        transform(matrix, {value.x, value.y, value.z, 0.0f});

    return {transformed.x, transformed.y, transformed.z};
}

Vec3 inverseTransformPoint (
                const Vec3& value,
                const Vec3& position,
                const Vec3& rotation,
                const Vec3& scale
        ) 
{
    Vec3 result = subtract(value, position);
    result = rotateZ(result, -rotation.z);
    result = rotateX(result, -rotation.x);
    result = rotateY(result, -rotation.y);

    if (std::fabs(scale.x) > EPSILON) result.x /= scale.x;
    if (std::fabs(scale.y) > EPSILON) result.y /= scale.y;
    if (std::fabs(scale.z) > EPSILON) result.z /= scale.z;
    return result;
}

Vec3 inverseTransformDirection (
                const Vec3& value,
                const Vec3& rotation,
                const Vec3& scale
        ) 
{
    Vec3 result = value;
    result = rotateZ(result, -rotation.z);
    result = rotateX(result, -rotation.x);
    result = rotateY(result, -rotation.y);

    if (std::fabs(scale.x) > EPSILON) result.x /= scale.x;
    if (std::fabs(scale.y) > EPSILON) result.y /= scale.y;
    if (std::fabs(scale.z) > EPSILON) result.z /= scale.z;
    return result;
}

} // namespace Math
} // namespace Renderer
