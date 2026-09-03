#ifndef CRAPGAME_RENDERER_LUMEN_SHORTRANGEAO_HPP
#define CRAPGAME_RENDERER_LUMEN_SHORTRANGEAO_HPP

#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Lumen/Sampling.hpp"
#include "Renderer/Lumen/Tracer.hpp"

#include <cstdint>
#include <vector>

namespace Renderer 
{
namespace Lumen 
{

float shortRangeWeight (
                float distance,
                float maximum_distance
        );

float shortRangeVisibility (
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Tracer& tracer,
                const GBuffer::Pixel& pixel,
                std::uint64_t frame_index,
                int ray_count,
                float maximum_distance,
                const std::vector<HemisphereSample> *sequence = nullptr
        );

/*
 * Cheap high-resolution contact term.  It samples nearby reconstructed world
 * positions instead of launching unified screen/SDF rays for every pixel.
 */
float shortRangeScreenVisibility (
                const GBuffer::Buffer& gbuffer,
                int x,
                int y,
                float maximum_distance
        );

} // namespace Lumen
} // namespace Renderer

#endif
