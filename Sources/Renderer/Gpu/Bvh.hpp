#ifndef CRAPGAME_RENDERER_GPU_BVH_HPP
#define CRAPGAME_RENDERER_GPU_BVH_HPP

#include "Ecs/Ecs.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>
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

static_assert(
        sizeof(BvhNodeGpu) == 48u,
        "BvhNodeGpu must match the GLSL std430 BvhNode layout"
    );

struct BvhBuild
{
    std::vector<BvhNodeGpu> nodes;
};

BvhBoundsInput primitiveBounds (
            const Ecs::TransformComponent& transform,
            Ecs::MeshType mesh,
            std::uint32_t primitive_index
    );

BvhBuild buildBvhSah (
            const std::vector<BvhBoundsInput>& bounds,
            std::size_t leaf_size = 3u,
            std::size_t bin_count = 16u
    );

BvhBuild buildBvh (
            const std::vector<BvhBoundsInput>& bounds,
            std::size_t leaf_size = 3u
    );

/* Preserve the existing topology/leaf primitive IDs and only update world
 * bounds. Internal nodes are processed bottom-up, so this is linear O(nodes)
 * with no sorting/allocation and is suitable for moving-object frames. */
bool refitBvh (
            std::vector<BvhNodeGpu> *nodes,
            const std::vector<BvhBoundsInput>& bounds
    );

} // namespace Gpu
} // namespace Renderer

#endif
