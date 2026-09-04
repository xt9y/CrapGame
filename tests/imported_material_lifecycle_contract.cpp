#include "Models/Models.hpp"
#include "Renderer/Material/Material.hpp"
#include "Renderer/Mesh/Mesh.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace
{

std::string readFile(const char *path)
{
    std::ifstream input(path);
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    );
}

} // namespace

int main()
{
    namespace fs = std::filesystem;

    const fs::path root =
        fs::temp_directory_path() / "crapgame-imported-material-lifecycle";
    fs::remove_all(root);
    fs::create_directories(root);

    {
        std::ofstream mtl(root / "scene.mtl");
        mtl << "newmtl cached\n"
               "Kd 0.4 0.5 0.6\n"
               "Ks 0.1 0.1 0.1\n"
               "Ns 24\n"
               "d 1\n";
    }

    {
        std::ofstream obj(root / "scene.obj");
        obj << "mtllib scene.mtl\n"
               "v 0 0 0\n"
               "v 1 0 0\n"
               "v 0 1 0\n"
               "vt 0 0\n"
               "vt 1 0\n"
               "vt 0 1\n"
               "vn 0 0 1\n"
               "usemtl cached\n"
               "f 1/1/1 2/2/1 3/3/1\n";
    }

    Models::clearCache();
    std::string error;

    const Models::ModelHandle first =
        Models::load((root / "scene.obj").string(), &error);
    assert(first != Models::INVALID_MODEL);
    assert(error.empty());

    const std::size_t material_count = Renderer::Material::count();
    const std::uint64_t material_revision = Renderer::Material::revision();
    const std::uint64_t mesh_revision = Renderer::Mesh::loadedMeshRevision();
    assert(material_count == 1u);

    const Models::ModelHandle second =
        Models::load((root / "scene.obj").string(), &error);
    assert(second == first);
    assert(error.empty());
    assert(Renderer::Material::count() == material_count);
    assert(Renderer::Material::revision() == material_revision);
    assert(Renderer::Mesh::loadedMeshRevision() == mesh_revision);

    Ecs::World world;
    const auto first_spawn = Models::spawn(world, first, {}, &error);
    const auto second_spawn = Models::spawn(world, second, {}, &error);
    assert(!first_spawn.empty());
    assert(first_spawn.size() == second_spawn.size());
    assert(Renderer::Material::count() == material_count);
    assert(Renderer::Material::revision() == material_revision);
    assert(Renderer::Mesh::loadedMeshRevision() == mesh_revision);

    const std::string trace_material =
        readFile("Sources/Renderer/Gpu/TraceMaterialGpu.cpp");
    const std::string triangle_scene =
        readFile("Sources/Renderer/Gpu/TriangleScene.cpp");
    const std::string material_gpu =
        readFile("Sources/Renderer/Gpu/MaterialGpu.cpp");
    const std::string transparent_gpu =
        readFile("Sources/Renderer/Gpu/TransparentGpu.cpp");
    const std::string render = readFile("Sources/Renderer/Render.cpp");

    assert(trace_material.find("revision == material_revision_")
           != std::string::npos);
    assert(trace_material.find("record_buffer_ != 0")
           != std::string::npos);
    assert(triangle_scene.find("if (!static_dirty_) return true;")
           != std::string::npos);
    assert(material_gpu.find("if (const GLuint existing = findTexture")
           != std::string::npos);
    assert(material_gpu.find("gpu.texture = 0;")
           != std::string::npos);
    assert(material_gpu.find("textures_.clear();")
           != std::string::npos);
    assert(transparent_gpu.find("mesh_revision_!=Mesh::loadedMeshRevision()")
           != std::string::npos);
    assert(transparent_gpu.find("clearMeshes();items_.clear();material_gpu_.shutdown()")
           != std::string::npos);
    assert(render.find("gpu_lumen_.shutdown();") != std::string::npos);
    assert(render.find("gpu_direct_lighting_.shutdown();") != std::string::npos);
    assert(render.find("gpu_gbuffer_.shutdown();") != std::string::npos);

    Models::clearCache();
    assert(Renderer::Material::count() == 0u);
    assert(Renderer::Material::revision() > material_revision);
    assert(Renderer::Mesh::loadedMeshRevision() > mesh_revision);

    fs::remove_all(root);
    return 0;
}
