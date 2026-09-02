#ifndef CRAPGAME_RENDERER_MATH_HPP
#define CRAPGAME_RENDERER_MATH_HPP

#include <cstddef>

namespace Renderer 
{
namespace Math 
{

struct Vec2 
{
    float x,
          y;
};

struct Vec3 
{
    float x,
          y,
          z;
};

struct Vec4 
{
    float x,
          y,
          z,
          w;
};

struct Mat4 
{
    float value[16];
};

float radians (float degrees);
float clamp (float value, float minimum, float maximum);
float saturate (float value);
float dot (const Vec3& a, const Vec3& b);
float length (const Vec3& value);
float lengthSquared (const Vec3& value);

Vec3 add (const Vec3& a, const Vec3& b);
Vec3 subtract (const Vec3& a, const Vec3& b);
Vec3 multiply (const Vec3& value, float scalar);
Vec3 multiply (const Vec3& a, const Vec3& b);
Vec3 divide (const Vec3& value, float scalar);
Vec3 cross (const Vec3& a, const Vec3& b);
Vec3 normalize (const Vec3& value);
Vec3 reflect (const Vec3& direction, const Vec3& normal);
Vec3 mix (const Vec3& a, const Vec3& b, float amount);

Mat4 identity ();
Mat4 multiply (const Mat4& a, const Mat4& b);
Mat4 translation (const Vec3& value);
Mat4 scaling (const Vec3& value);
Mat4 rotationX (float degrees);
Mat4 rotationY (float degrees);
Mat4 rotationZ (float degrees);
Mat4 rotationEuler (const Vec3& degrees);
Mat4 transform (
                const Vec3& position,
                const Vec3& rotation,
                const Vec3& scale
        );
Mat4 perspective (
                float fov_degrees,
                float aspect,
                float near_plane,
                float far_plane
        );
Mat4 lookAt (
                const Vec3& eye,
                const Vec3& center,
                const Vec3& up
        );

Vec4 transform (const Mat4& matrix, const Vec4& value);
Vec3 transformPoint (const Mat4& matrix, const Vec3& value);
Vec3 transformDirection (const Mat4& matrix, const Vec3& value);
Vec3 inverseTransformPoint (
                const Vec3& value,
                const Vec3& position,
                const Vec3& rotation,
                const Vec3& scale
        );
Vec3 inverseTransformDirection (
                const Vec3& value,
                const Vec3& rotation,
                const Vec3& scale
        );

} // namespace Math
} // namespace Renderer

#endif
