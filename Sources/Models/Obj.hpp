#ifndef CRAPGAME_MODELS_OBJ_HPP
#define CRAPGAME_MODELS_OBJ_HPP

#include "Models/Material.hpp"
#include "Renderer/Mesh/Mesh.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Models
{
namespace Obj
{

struct Submesh
{
    Renderer::Mesh::MeshData mesh;
    std::string object_name;
    std::string group_name;
    std::string material_name;
    std::uint32_t material_index = UINT32_MAX;
};

struct Document
{
    std::vector<Submesh> submeshes;
    std::vector<MaterialData> materials;
    std::vector<std::string> warnings;
};

bool load(
    const std::string& path,
    Document *document,
    std::string *error = nullptr
);

} // namespace Obj
} // namespace Models

#endif
