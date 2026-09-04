#include "Mesh.hpp"
#include "Renderer/Mesh/Tangent.hpp"

#include <algorithm>
#include <deque>
#include <limits>
#include <utility>

namespace Renderer 
{
namespace Mesh 
{
namespace 
{

Bounds calculateBounds (const std::vector<Vertex>& vertices) 
{
    const float maximum_value = std::numeric_limits<float>::max();
    Bounds bounds = {
        { maximum_value,  maximum_value,  maximum_value},
        {-maximum_value, -maximum_value, -maximum_value},
    };

    for (const Vertex& vertex : vertices) 
    {
        bounds.minimum.x = std::min(bounds.minimum.x, vertex.position.x);
        bounds.minimum.y = std::min(bounds.minimum.y, vertex.position.y);
        bounds.minimum.z = std::min(bounds.minimum.z, vertex.position.z);
        bounds.maximum.x = std::max(bounds.maximum.x, vertex.position.x);
        bounds.maximum.y = std::max(bounds.maximum.y, vertex.position.y);
        bounds.maximum.z = std::max(bounds.maximum.z, vertex.position.z);
    }

    return bounds;
}

void addFace (
                MeshData *mesh,
                const Math::Vec3& a,
                const Math::Vec3& b,
                const Math::Vec3& c,
                const Math::Vec3& d,
                const Math::Vec3& normal
        ) 
{
    const std::uint32_t base = static_cast<std::uint32_t>(mesh->vertices.size());

    mesh->vertices.push_back({a, normal, {0.0f, 0.0f}});
    mesh->vertices.push_back({b, normal, {1.0f, 0.0f}});
    mesh->vertices.push_back({c, normal, {1.0f, 1.0f}});
    mesh->vertices.push_back({d, normal, {0.0f, 1.0f}});

    mesh->indices.push_back(base + 0u);
    mesh->indices.push_back(base + 1u);
    mesh->indices.push_back(base + 2u);
    mesh->indices.push_back(base + 0u);
    mesh->indices.push_back(base + 2u);
    mesh->indices.push_back(base + 3u);
}

std::deque<MeshData>& loadedMeshes ()
{
    static std::deque<MeshData> meshes;
    return meshes;
}

} // namespace

MeshData createCube () 
{
    constexpr float h = 0.75f;
    MeshData mesh;

    addFace(&mesh, {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h}, { 0.0f,  0.0f,  1.0f});
    addFace(&mesh, { h, -h, -h}, {-h, -h, -h}, {-h,  h, -h}, { h,  h, -h}, { 0.0f,  0.0f, -1.0f});
    addFace(&mesh, {-h, -h, -h}, {-h, -h,  h}, {-h,  h,  h}, {-h,  h, -h}, {-1.0f,  0.0f,  0.0f});
    addFace(&mesh, { h, -h,  h}, { h, -h, -h}, { h,  h, -h}, { h,  h,  h}, { 1.0f,  0.0f,  0.0f});
    addFace(&mesh, {-h,  h,  h}, { h,  h,  h}, { h,  h, -h}, {-h,  h, -h}, { 0.0f,  1.0f,  0.0f});
    addFace(&mesh, {-h, -h, -h}, { h, -h, -h}, { h, -h,  h}, {-h, -h,  h}, { 0.0f, -1.0f,  0.0f});

    mesh.bounds = calculateBounds(mesh.vertices);
    generateTangents(&mesh);
    return mesh;
}

MeshData createPlane () 
{
    MeshData mesh;
    addFace(
            &mesh,
            {-0.5f, 0.0f, -0.5f},
            {-0.5f, 0.0f,  0.5f},
            { 0.5f, 0.0f,  0.5f},
            { 0.5f, 0.0f, -0.5f},
            {0.0f, 1.0f, 0.0f}
        );
    mesh.bounds = calculateBounds(mesh.vertices);
    generateTangents(&mesh);
    return mesh;
}

const MeshData& meshForType (Ecs::MeshType mesh_type) 
{
    static const MeshData cube = createCube(),
                          plane = createPlane();

    switch (mesh_type) 
    {
        case Ecs::MeshType::Cube:  return cube;
        case Ecs::MeshType::Plane: return plane;
    }

    return cube;
}

std::uint32_t registerLoadedMesh (MeshData mesh)
{
    std::deque<MeshData>& meshes = loadedMeshes();
    const std::uint32_t handle = static_cast<std::uint32_t>(meshes.size());
    meshes.push_back(std::move(mesh));
    return handle;
}

const MeshData *loadedMesh (std::uint32_t handle)
{
    std::deque<MeshData>& meshes = loadedMeshes();
    return handle < meshes.size() ? &meshes[handle] : nullptr;
}

const MeshData& meshForComponent (const Ecs::MeshComponent& component)
{
    if (component.loaded_mesh != Ecs::INVALID_ASSET_HANDLE)
    {
        if (const MeshData *mesh = loadedMesh(component.loaded_mesh))
        {
            return *mesh;
        }
    }

    return meshForType(component.mesh);
}

void clearLoadedMeshes ()
{
    loadedMeshes().clear();
}

} // namespace Mesh
} // namespace Renderer
