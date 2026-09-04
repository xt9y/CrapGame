#include "Models/Models.hpp"
#include "Models/Obj.hpp"
#include "Renderer/Mesh/Mesh.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

static bool nearValue (float a, float b)
{
    return std::fabs(a - b) < 0.001f;
}

int main ()
{
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "crapgame-models-contract";
    fs::remove_all(dir);
    fs::create_directories(dir / "tex");

    {
        std::ofstream mtl(dir / "test.mtl");
        mtl << "newmtl stone\nKd 0.2 0.4 0.6\nKs 0.8 0.7 0.6\nNs 18\nNi 1.5\n"
               "Pr 0.33\nPm 0.7\nTr 0.25\nPt 0.4\nKr 0.2\nPc 0.6\nPcr 0.1\nPs 0.3\naniso 0.4\n"
               "map_Kd -o 1 2 3 -s 2 2 2 tex/albedo.tga\nmap_Pr tex/rough.tga\n"
               "map_Pm tex/metal.tga\nrefl tex/refl.tga\n"
               "newmtl glass\nKd 1 1 1\nd 0.5\nmap_d tex/mask.tga\n";
    }

    {
        std::ofstream obj(dir / "test.obj");
        obj << "mtllib test.mtl\no Hall\ng Quad\n"
               "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
               "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
               "usemtl stone\ns 1\nf 1/1 2/2 3/3 4/4\n"
               "usemtl glass\ns off\nf -4/1 -2/3 -1/4\n";
    }

    Models::Obj::Document document;
    std::string error;
    assert(Models::Obj::load((dir / "test.obj").string(), &document, &error));
    assert(error.empty());
    assert(document.submeshes.size() == 2u);
    assert(document.materials.size() == 2u);
    assert(document.submeshes[0].mesh.indices.size() == 6u);
    assert(document.submeshes[1].mesh.indices.size() == 3u);

    const Models::MaterialData& material = document.materials[0];
    assert(nearValue(material.base_color.x, 0.2f));
    assert(nearValue(material.roughness, 0.33f));
    assert(nearValue(material.metallic, 0.7f));
    assert(nearValue(material.opacity, 0.75f));
    assert(nearValue(material.transmission, 0.4f));
    assert(nearValue(material.reflectivity, 0.2f));
    assert(nearValue(material.clearcoat, 0.6f));
    assert(material.base_color_texture.path.find("tex/albedo.tga") != std::string::npos);
    assert(nearValue(material.base_color_texture.offset.x, 1.0f));
    assert(nearValue(material.base_color_texture.scale.x, 2.0f));

    Models::clearCache();
    Ecs::World world;
    const Models::ModelHandle first = Models::load((dir / "test.obj").string(), &error);
    const Models::ModelHandle second = Models::load((dir / "test.obj").string(), &error);
    assert(first == second && first != Models::INVALID_MODEL);

    const std::vector<Ecs::Entity> entities = Models::spawn(world, first, {}, &error);
    assert(entities.size() == 2u);
    const Ecs::MeshComponent *mesh = world.getMesh(entities[0]);
    const Ecs::MaterialComponent *ecs_material = world.getMaterial(entities[0]);
    assert(mesh && mesh->loaded_mesh != Ecs::INVALID_ASSET_HANDLE);
    assert(Renderer::Mesh::loadedMesh(mesh->loaded_mesh));
    assert(ecs_material && nearValue(ecs_material->roughness, 0.33f));
    assert(nearValue(ecs_material->transparency, 0.25f));

    {
        std::ofstream bad(dir / "bad.obj");
        bad << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 99\n";
    }
    Models::Obj::Document invalid;
    assert(!Models::Obj::load((dir / "bad.obj").string(), &invalid, &error));

    fs::remove_all(dir);
    return 0;
}
