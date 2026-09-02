#ifndef CRAPGAME_RENDERER_MESH_HPP
#define CRAPGAME_RENDERER_MESH_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Math/Math.hpp"

#include <cstdint>
#include <vector>

namespace Renderer 
{
namespace Mesh 
{

struct Vertex 
{
    Math::Vec3 position,
               normal;

    Math::Vec2 uv;
};

struct Bounds 
{
    Math::Vec3 minimum,
               maximum;
};

struct MeshData 
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    Bounds bounds;
};

MeshData createCube ();
MeshData createPlane ();
const MeshData& meshForType (Ecs::MeshType mesh_type);

} // namespace Mesh
} // namespace Renderer

#endif
