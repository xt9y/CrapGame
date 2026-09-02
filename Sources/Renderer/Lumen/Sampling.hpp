#ifndef CRAPGAME_RENDERER_LUMEN_SAMPLING_HPP
#define CRAPGAME_RENDERER_LUMEN_SAMPLING_HPP

#include "Renderer/Math/Math.hpp"

#include <cstdint>

namespace Renderer 
{
namespace Lumen 
{

Math::Vec3 sampleHemisphere (
                const Math::Vec3& normal,
                int sample_index,
                int sample_count,
                std::uint64_t frame_index
        );

} // namespace Lumen
} // namespace Renderer

#endif
