#ifndef CRAPGAME_RENDERER_GPU_BVH_HPP
#define CRAPGAME_RENDERER_GPU_BVH_HPP

#include "Ecs/Ecs.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Renderer
{
namespace Gpu
{

struct BvhBoundsInput
{
    float minimum[3] = {};
    float maximum[3] = {};
    std::uint32_t primitive_index = 0;
};

/* std430-compatible: vec4 + vec4 + ivec4 = 48 bytes. */
struct BvhNodeGpu
{
    float bounds_minimum[4] = {};
    float bounds_maximum[4] = {};
    std::int32_t meta[4] = {};
};

struct BvhBuild
{
    std::vector<BvhNodeGpu> nodes;
    std::vector<std::uint32_t> primitive_indices;
};

BvhBoundsInput primitiveBounds (
            const Ecs::TransformComponent& transform,
            Ecs::MeshType mesh,
            std::uint32_t primitive_index
    );

BvhBuild buildBvh (
            const std::vector<BvhBoundsInput>& bounds,
            std::size_t leaf_size = 4u
    );

} // namespace Gpu
} // namespace Renderer

#endif
