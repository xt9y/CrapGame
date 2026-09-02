#ifndef CRAPGAME_RENDERER_LUMEN_RADIANCECACHE_HPP
#define CRAPGAME_RENDERER_LUMEN_RADIANCECACHE_HPP

#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Lumen/SurfaceCache.hpp"
#include "Renderer/Lumen/Tracer.hpp"
#include "Renderer/Math/Math.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Renderer 
{
namespace Lumen 
{

struct RadianceProbe 
{
    Math::Vec3 position,
               radiance;

    std::uint64_t updated_frame = 0;
    bool valid = false;
};

class RadianceCache 
{

public:
    void update (
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Tracer& tracer,
                const SurfaceCache& surface_cache,
                const Math::Vec3& camera_position,
                std::uint64_t frame_index,
                std::size_t maximum_updates
        );

    Math::Vec3 sample (const Math::Vec3& position) const;
    const std::vector<RadianceProbe>& probes () const;

private:
    void resetGrid (const Math::Vec3& camera_position);

    std::vector<RadianceProbe> probes_;
    Math::Vec3 grid_center_ = {0.0f, 0.0f, 0.0f};

    std::size_t update_cursor_ = 0;
    bool initialized_ = false;
};

} // namespace Lumen
} // namespace Renderer

#endif
