#ifndef CRAPGAME_RENDERER_MESH_TANGENT_HPP
#define CRAPGAME_RENDERER_MESH_TANGENT_HPP

#include "Renderer/Material/Material.hpp"
#include "Renderer/Mesh/Mesh.hpp"

#include <string>

namespace Renderer
{
namespace Mesh
{

void generateTangents(MeshData *mesh);
bool applyDisplacement(
    MeshData *mesh,
    const Material::TextureBinding& binding,
    float strength,
    std::string *error = nullptr
);

} // namespace Mesh
} // namespace Renderer

#endif
