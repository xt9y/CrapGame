#ifndef CRAPGAME_RENDERER_LUMEN_DISTANCEFIELD_HPP
#define CRAPGAME_RENDERER_LUMEN_DISTANCEFIELD_HPP

#include "Renderer/Mesh/Mesh.hpp"

#include <cstddef>
#include <vector>

namespace Renderer 
{
namespace Lumen 
{

struct MeshDistanceField 
{
    Mesh::Bounds bounds;
    Math::Vec3 extent;
    std::vector<float> distance;

    int resolution = 0;
    int resolution_minus_one = 0;
    std::size_t resolution_squared = 0u;
    bool signed_distance = false;
};

MeshDistanceField buildDistanceField (
                const Mesh::MeshData& mesh,
                int resolution
        );

float sampleDistanceField (
                const MeshDistanceField& field,
                const Math::Vec3& position
        );

} // namespace Lumen
} // namespace Renderer

#endif
