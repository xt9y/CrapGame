#ifndef CRAPGAME_RENDERER_GPU_RADIANCECACHEPOLICY_HPP
#define CRAPGAME_RENDERER_GPU_RADIANCECACHEPOLICY_HPP

#include "Renderer/Gpu/RevisionState.hpp"

#include <array>
#include <cmath>
#include <cstdint>

namespace Renderer
{
namespace Gpu
{

struct RadianceCacheRecordGpu
{
    std::int32_t key_generation[4] = {};
    std::uint32_t meta[4] = {};
    float radiance[4] = {};
    float normal[4] = {};
};
static_assert(sizeof(RadianceCacheRecordGpu)==64u,"radiance cache record layout changed");

struct RadianceCachePolicy
{
    static constexpr float CELL_SIZE=0.5f;
    static constexpr std::uint32_t INITIAL_CAPACITY=65536u;
    static constexpr std::uint32_t HIGH_CONFIDENCE_SAMPLES=48u;
    static constexpr std::uint32_t ACCEPT_CONFIDENCE=16u;
    static constexpr std::uint32_t MAX_LINEAR_PROBES=8u;
    static constexpr std::uint32_t BUFFER_BINDING=9u;

    static std::array<std::int32_t,3> cell(float x,float y,float z)
    {
        return {{
            static_cast<std::int32_t>(std::floor(x/CELL_SIZE)),
            static_cast<std::int32_t>(std::floor(y/CELL_SIZE)),
            static_cast<std::int32_t>(std::floor(z/CELL_SIZE))
        }};
    }

    static std::uint32_t hashCell(std::int32_t x,std::int32_t y,std::int32_t z)
    {
        std::uint32_t h=static_cast<std::uint32_t>(x)*0x8da6b343u
            +static_cast<std::uint32_t>(y)*0xd8163841u
            +static_cast<std::uint32_t>(z)*0xcb1ab31fu;
        h^=h>>13u;h*=0x85ebca6bu;h^=h>>16u;
        return h;
    }

    static bool generationChanges(const RevisionState& cached,const RevisionState& current)
    {
        return !worldRadianceValid(cached,current);
    }
};

} // namespace Gpu
} // namespace Renderer

#endif
