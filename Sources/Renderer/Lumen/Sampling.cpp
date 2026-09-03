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

HemisphereBasis hemisphereBasis(const Math::Vec3& normal)
{
    HemisphereBasis basis;
    basis.normal = Math::normalize(normal);

    const Math::Vec3 reference =
        std::fabs(basis.normal.y) < 0.95f
        ? Math::Vec3{0.0f, 1.0f, 0.0f}
        : Math::Vec3{1.0f, 0.0f, 0.0f};

    basis.tangent = Math::normalize(
            Math::cross(reference, basis.normal)
        );
    basis.bitangent = Math::normalize(
            Math::cross(basis.normal, basis.tangent)
        );
    return basis;
}

HemisphereSample hemisphereSequenceSample(
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
                angle = GOLDEN * static_cast<float>(sample_index) + phase;

    HemisphereSample sample;
    sample.x = std::cos(angle) * radius;
    sample.z = std::sin(angle) * radius;
    sample.y = std::sqrt(
            std::max(0.0f, 1.0f - sequence)
        );
    return sample;
}

std::vector<HemisphereSample> buildHemisphereSequence(
                int sample_count,
                std::uint64_t frame_index
        )
{
    const int count = std::max(1, sample_count);
    std::vector<HemisphereSample> sequence;
    sequence.reserve(static_cast<std::size_t>(count));

    for (int sample_index = 0; sample_index < count; ++sample_index)
    {
        sequence.push_back(
                hemisphereSequenceSample(
                        sample_index,
                        count,
                        frame_index
                    )
            );
    }

    return sequence;
}

void fillHemisphereSequence(
                int sample_count,
                std::uint64_t frame_index,
                std::vector<HemisphereSample> *sequence
        )
{
    if (!sequence)
    {
        return;
    }

    const int count = std::max(1, sample_count);
    sequence->resize(static_cast<std::size_t>(count));

    for (int sample_index = 0; sample_index < count; ++sample_index)
    {
        (*sequence)[static_cast<std::size_t>(sample_index)] =
            hemisphereSequenceSample(
                    sample_index,
                    count,
                    frame_index
                );
    }
}

Math::Vec3 sampleHemisphere(
                const HemisphereBasis& basis,
                const HemisphereSample& sample
        )
{
    return Math::normalize(
            Math::add(
                    Math::add(
                            Math::multiply(basis.tangent, sample.x),
                            Math::multiply(basis.normal, sample.y)
                        ),
                    Math::multiply(basis.bitangent, sample.z)
                )
        );
}

Math::Vec3 sampleHemisphere (
                const Math::Vec3& normal,
                int sample_index,
                int sample_count,
                std::uint64_t frame_index
        ) 
{
    return sampleHemisphere(
            hemisphereBasis(normal),
            hemisphereSequenceSample(
                    sample_index,
                    sample_count,
                    frame_index
                )
        );
}

} // namespace Lumen
} // namespace Renderer
