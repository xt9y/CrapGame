#ifndef CRAPGAME_RENDERER_LUMEN_GLOBALDISTANCEFIELD_HPP
#define CRAPGAME_RENDERER_LUMEN_GLOBALDISTANCEFIELD_HPP

#include "Renderer/Lumen/SphereTrace.hpp"
#include "Renderer/Math/Math.hpp"

#include <cstddef>
#include <vector>

namespace Renderer 
{
namespace Lumen 
{

class GlobalDistanceField 
{

public:
    void build (
                const DistanceFieldScene& scene,
                const Math::Vec3& camera_position
        );

    float sample (const Math::Vec3& position) const;

private:
    struct Clipmap 
    {
        Math::Vec3 center,
                   minimum;
        float half_extent = 0.0f;
        float extent = 0.0f;
        int resolution = 0;
        int resolution_minus_one = 0;
        std::size_t resolution_squared = 0u;
        std::vector<float> distance;
    };

    void buildClipmap (
                Clipmap *clipmap,
                const DistanceFieldScene& scene,
                const Math::Vec3& camera_position,
                float half_extent,
                int resolution
        );

    float sampleClipmap (
                const Clipmap& clipmap,
                const Math::Vec3& position
        ) const;

    std::vector<Clipmap> clipmaps_;
};

} // namespace Lumen
} // namespace Renderer

#endif
