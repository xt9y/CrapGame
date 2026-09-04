#include "Renderer/Gpu/TriangleScene.hpp"
#include "Renderer/Material/Material.hpp"
#include "Renderer/Mesh/Mesh.hpp"

#include <cassert>
#include <string>

int main()
{
    using namespace Renderer;
    Mesh::clearLoadedMeshes();
    Material::clear();

    Mesh::MeshData mesh;
    mesh.vertices = {
        {{0.0f,0.0f,0.0f},{0.0f,0.0f,1.0f},{0.0f,0.0f},{1.0f,0.0f,0.0f,1.0f}},
        {{1.0f,0.0f,0.0f},{0.0f,0.0f,1.0f},{1.0f,0.0f},{1.0f,0.0f,0.0f,1.0f}},
        {{0.0f,1.0f,0.0f},{0.0f,0.0f,1.0f},{0.0f,1.0f},{1.0f,0.0f,0.0f,1.0f}},
    };
    mesh.indices = {0u,1u,2u};
    mesh.bounds = {{0.0f,0.0f,0.0f},{1.0f,1.0f,0.0f}};
    const std::uint32_t mesh_handle = Mesh::registerLoadedMesh(mesh);

    Material::Resource material;
    material.render_class = Material::RenderClass::Opaque;
    const Material::MaterialHandle material_handle =
        Material::registerMaterial(material);

    Ecs::World world;
    for (int i=0;i<2;++i)
    {
        const Ecs::Entity e=world.createEntity();
        world.addTransform(e,{{static_cast<float>(i)*2.0f,0.0f,0.0f},{0.0f,0.0f,0.0f},{1.0f,1.0f,1.0f}});
        world.addMesh(e,{Ecs::MeshType::Cube,mesh_handle});
        world.addRenderable(e,{true});
        Ecs::MaterialComponent ecs_material = {{1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},0.0f,1.0f,1.0f};
        ecs_material.renderer_material=material_handle;
        world.addMaterial(e,ecs_material);
    }

    Gpu::TriangleScene scene;
    std::string error;
    assert(scene.buildCpu(world,&error));
    assert(error.empty());
    assert(scene.triangleCount()==1u);
    assert(scene.meshCount()==1u);
    assert(scene.blasNodeCount()>0u);
    assert(scene.instanceCount()==2u);
    assert(scene.tlasNodeCount()>0u);

    Ecs::TransformComponent *moved=world.getTransform(1u);
    assert(moved);
    moved->position.x=7.0f;
    const std::size_t triangle_count=scene.triangleCount();
    const std::size_t blas_count=scene.blasNodeCount();
    assert(scene.buildCpu(world,&error));
    assert(scene.triangleCount()==triangle_count);
    assert(scene.blasNodeCount()==blas_count);
    assert(scene.instanceCount()==2u);
    return 0;
}
