#ifndef CRAPGAME_RENDERER_LUMEN_SAMPLING_HPP
#define CRAPGAME_RENDERER_LUMEN_SAMPLING_HPP

#include "Renderer/Math/Math.hpp"

#include <cstdint>
#include <vector>

namespace Renderer 
{
namespace Lumen 
{

struct HemisphereBasis
{
    Math::Vec3 normal,
               tangent,
               bitangent;
};

struct HemisphereSample
{
    float x = 0.0f,
          y = 0.0f,
          z = 0.0f;
};

HemisphereBasis hemisphereBasis(const Math::Vec3& normal);

HemisphereSample hemisphereSequenceSample(
                int sample_index,
                int sample_count,
                std::uint64_t frame_index
        );

std::vector<HemisphereSample> buildHemisphereSequence(
                int sample_count,
                std::uint64_t frame_index
        );

void fillHemisphereSequence(
                int sample_count,
                std::uint64_t frame_index,
                std::vector<HemisphereSample> *sequence
        );

Math::Vec3 sampleHemisphere(
                const HemisphereBasis& basis,
                const HemisphereSample& sample
        );

Math::Vec3 sampleHemisphere (
                const Math::Vec3& normal,
                int sample_index,
                int sample_count,
                std::uint64_t frame_index
        );

} // namespace Lumen
} // namespace Renderer

#endif
