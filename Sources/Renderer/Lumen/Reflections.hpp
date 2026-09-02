#ifndef CRAPGAME_RENDERER_LUMEN_REFLECTIONS_HPP
#define CRAPGAME_RENDERER_LUMEN_REFLECTIONS_HPP

#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Lumen/RadianceCache.hpp"
#include "Renderer/Lumen/SurfaceCache.hpp"
#include "Renderer/Lumen/Tracer.hpp"

#include <cstdint>
#include <vector>

namespace Renderer 
{
namespace Lumen 
{

class ReflectionSystem 
{

public:
    void render (
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Tracer& tracer,
                const SurfaceCache& surface_cache,
                const RadianceCache& radiance_cache,
                const Math::Vec3& camera_position,
                std::uint64_t frame_index,
                std::vector<Math::Vec3> *output
        ) const;
};

} // namespace Lumen
} // namespace Renderer

#endif
