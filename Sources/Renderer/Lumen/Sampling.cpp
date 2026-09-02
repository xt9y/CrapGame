#include "Sampling.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer 
{
namespace Lumen 
{
namespace 
{

constexpr float GOLDEN = 2.39996322972865332f;

} // namespace

Math::Vec3 sampleHemisphere (
                const Math::Vec3& normal,
                int sample_index,
                int sample_count,
                std::uint64_t frame_index
        ) 
{
    const int count = std::max(1, sample_count);

    const float sequence =
                (static_cast<float>(sample_index) + 0.5f) /
                static_cast<float>(count),
                radius = std::sqrt(sequence),
                phase = static_cast<float>(frame_index % 64u) * 0.61803398875f,
                angle = GOLDEN * static_cast<float>(sample_index) + phase,
                local_x = std::cos(angle) * radius,
                local_z = std::sin(angle) * radius,
                local_y = std::sqrt(
                        std::max(0.0f, 1.0f - sequence)
                    );

    const Math::Vec3 n = Math::normalize(normal);

    const Math::Vec3 reference =
        std::fabs(n.y) < 0.95f
        ? Math::Vec3{0.0f, 1.0f, 0.0f}
        : Math::Vec3{1.0f, 0.0f, 0.0f};

    const Math::Vec3 tangent =
        Math::normalize(Math::cross(reference, n));

    const Math::Vec3 bitangent =
        Math::normalize(Math::cross(n, tangent));

    return Math::normalize(
            Math::add(
                    Math::add(
                            Math::multiply(tangent, local_x),
                            Math::multiply(n, local_y)
                        ),
                    Math::multiply(bitangent, local_z)
                )
        );
}

} // namespace Lumen
} // namespace Renderer
