#ifndef CRAPGAME_RENDERER_GPU_VIRTUALSHADOWINVALIDATIONGPU_HPP
#define CRAPGAME_RENDERER_GPU_VIRTUALSHADOWINVALIDATIONGPU_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Math/Math.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Renderer
{
namespace Gpu
{

class VirtualShadowMapGpu;

class VirtualShadowInvalidationGpu
{
public:
    bool init (std::string *error = nullptr);

    bool update (
                const Ecs::World& world,
                VirtualShadowMapGpu& virtual_shadow_map,
                std::uint64_t scene_revision,
                std::string *error = nullptr
        );

    void shutdown ();

    bool ready () const
    {
        return program_ != 0 && region_buffer_ != 0;
    }

private:
    static constexpr std::size_t MAX_REGIONS = 256u;

    struct CasterSnapshot
    {
        Math::Vec3 minimum = {0.0f, 0.0f, 0.0f},
                   maximum = {0.0f, 0.0f, 0.0f};
        std::uint32_t mesh = Ecs::INVALID_ASSET_HANDLE,
                      material = Ecs::INVALID_ASSET_HANDLE;
        bool valid = false;
    };

    struct LightSnapshot
    {
        Math::Vec3 direction = {0.0f, -1.0f, 0.0f};
        Ecs::Entity entity = Ecs::INVALID_ENTITY;
        int active_index = -1;
        bool valid = false;
    };

    struct RegionGpu
    {
        float minimum[4] = {},
              maximum[4] = {};
    };

    CasterSnapshot casterSnapshot (
                const Ecs::World& world,
                Ecs::Entity entity
        ) const;
    LightSnapshot lightSnapshot (const Ecs::World& world) const;
    bool sameCaster (
                const CasterSnapshot& a,
                const CasterSnapshot& b
        ) const;
    void appendRegion (const CasterSnapshot& snapshot, bool *overflow);
    bool uploadRegions (std::string *error);

    GLuint program_ = 0,
           region_buffer_ = 0;

    GLint region_count_location_ = -1,
          light_index_location_ = -1,
          invalidate_light_location_ = -1,
          right_location_ = -1,
          up_location_ = -1;

    std::vector<CasterSnapshot> snapshots_,
                                current_snapshots_;
    std::vector<RegionGpu> regions_;
    LightSnapshot light_ = {};

    std::size_t region_capacity_ = 0u;
    std::uint64_t scene_revision_ = 0u,
                  mesh_revision_ = 0u,
                  material_revision_ = 0u;
    bool initialized_ = false;
};

} // namespace Gpu
} // namespace Renderer

#endif
