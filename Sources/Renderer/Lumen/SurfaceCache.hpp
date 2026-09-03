#ifndef CRAPGAME_RENDERER_LUMEN_SURFACECACHE_HPP
#define CRAPGAME_RENDERER_LUMEN_SURFACECACHE_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Lumen/Cards.hpp"
#include "Renderer/Math/Math.hpp"

#include <vector>

namespace Renderer 
{
namespace Lumen 
{

struct SurfaceSample 
{
    Card card;

    Math::Vec3 albedo,
               emissive,
               direct_lighting,
               indirect_lighting;

    float metallic,
          roughness;
};

class SurfaceCache 
{

public:
    void build (
                const Ecs::World& world,
                const CardScene& cards
        );

    const SurfaceSample *sample (
                Ecs::Entity entity,
                const Math::Vec3& position,
                const Math::Vec3& normal
        ) const;

    Math::Vec3 radiance (
                Ecs::Entity entity,
                const Math::Vec3& position,
                const Math::Vec3& normal
        ) const;

    std::vector<SurfaceSample>& samples ();
    const std::vector<SurfaceSample>& samples () const;

private:
    std::vector<SurfaceSample> samples_,
                               previous_samples_;
};

} // namespace Lumen
} // namespace Renderer

#endif
