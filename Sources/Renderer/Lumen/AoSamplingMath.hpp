#ifndef CRAPGAME_RENDERER_LUMEN_AO_SAMPLING_MATH_HPP
#define CRAPGAME_RENDERER_LUMEN_AO_SAMPLING_MATH_HPP

#include "Renderer/Lumen/Sampling.hpp"

#include <cmath>

namespace Renderer
{
namespace Lumen
{

inline Math::Vec3 aoNormalizeExact(const Math::Vec3& value)
{
    const float length_squared =
        value.x * value.x + value.y * value.y + value.z * value.z;
    const float value_length = std::sqrt(length_squared);

    if (value_length <= 0.000001f)
    {
        return {0.0f, 0.0f, 0.0f};
    }

    const float inverse = 1.0f / value_length;
    return {
        value.x * inverse,
        value.y * inverse,
        value.z * inverse,
    };
}

inline Math::Vec3 aoCrossExact(
            const Math::Vec3& a,
            const Math::Vec3& b
    )
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline HemisphereBasis aoHemisphereBasisExact(const Math::Vec3& normal)
{
    HemisphereBasis basis;
    basis.normal = aoNormalizeExact(normal);

    const Math::Vec3 reference =
        std::fabs(basis.normal.y) < 0.95f
        ? Math::Vec3{0.0f, 1.0f, 0.0f}
        : Math::Vec3{1.0f, 0.0f, 0.0f};

    basis.tangent = aoNormalizeExact(
            aoCrossExact(reference, basis.normal)
        );
    basis.bitangent = aoNormalizeExact(
            aoCrossExact(basis.normal, basis.tangent)
        );
    return basis;
}

inline Math::Vec3 aoSampleHemisphereExact(
            const HemisphereBasis& basis,
            const HemisphereSample& sample
    )
{
    const Math::Vec3 tangent = {
        basis.tangent.x * sample.x,
        basis.tangent.y * sample.x,
        basis.tangent.z * sample.x,
    };
    const Math::Vec3 normal = {
        basis.normal.x * sample.y,
        basis.normal.y * sample.y,
        basis.normal.z * sample.y,
    };
    const Math::Vec3 first_sum = {
        tangent.x + normal.x,
        tangent.y + normal.y,
        tangent.z + normal.z,
    };
    const Math::Vec3 bitangent = {
        basis.bitangent.x * sample.z,
        basis.bitangent.y * sample.z,
        basis.bitangent.z * sample.z,
    };

    return aoNormalizeExact({
        first_sum.x + bitangent.x,
        first_sum.y + bitangent.y,
        first_sum.z + bitangent.z,
    });
}

inline bool aoRadianceIsExactlyZero(const Math::Vec3& value)
{
    return value.x == 0.0f
        && value.y == 0.0f
        && value.z == 0.0f;
}

} // namespace Lumen
} // namespace Renderer

#endif
