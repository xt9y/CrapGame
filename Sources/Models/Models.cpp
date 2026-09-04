#include "Models/Models.hpp"

#include "Models/Material.hpp"
#include "Models/Obj.hpp"
#include "Models/Texture.hpp"
#include "Renderer/Material/Material.hpp"
#include "Renderer/Mesh/Mesh.hpp"

#include <deque>
#include <filesystem>
#include <unordered_map>
#include <utility>

namespace Models
{
namespace
{

struct Part
{
    std::uint32_t mesh = Ecs::INVALID_ASSET_HANDLE;
    std::uint32_t material = Ecs::INVALID_ASSET_HANDLE;
    Renderer::Material::MaterialHandle renderer_material =
        Renderer::Material::INVALID_MATERIAL;
};

struct Model
{
    std::string path;
    std::vector<Part> parts;
};

std::vector<Model>& models ()
{
    static std::vector<Model> values;
    return values;
}

std::unordered_map<std::string, ModelHandle>& cache ()
{
    static std::unordered_map<std::string, ModelHandle> values;
    return values;
}

std::deque<MaterialData>& materials ()
{
    static std::deque<MaterialData> values;
    return values;
}

std::string normalizedPath (const std::string& path)
{
    std::error_code error;
    const std::filesystem::path absolute =
        std::filesystem::absolute(std::filesystem::path(path), error);

    return (error ? std::filesystem::path(path) : absolute)
        .lexically_normal()
        .string();
}

Ecs::Vec3 toVec3 (const Renderer::Math::Vec3& value)
{
    return {value.x, value.y, value.z};
}

Ecs::MaterialComponent materialComponent (
                std::uint32_t handle,
                Renderer::Material::MaterialHandle renderer_material
        )
{
    Ecs::MaterialComponent result = {
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        0.0f,
        1.0f,
        1.0f,
    };
    result.renderer_material = renderer_material;

    if (handle == Ecs::INVALID_ASSET_HANDLE
            || handle >= materials().size())
    {
        return result;
    }

    const MaterialData& material = materials()[handle];
    result.albedo = toVec3(material.base_color);
    result.emissive = toVec3(material.emissive);
    result.metallic = material.metallic;
    result.roughness = material.roughness;
    result.emissive_strength = 1.0f;
    result.ambient = toVec3(material.ambient);
    result.specular = toVec3(material.specular);
    result.transmission_color = toVec3(material.transmission_color);
    result.specular_strength = material.specular_strength;
    result.shininess = material.shininess;
    result.ior = material.ior;
    result.opacity = material.opacity;
    result.transparency = material.transparency;
    result.transmission = material.transmission;
    result.reflectivity = material.reflectivity;
    result.clearcoat = material.clearcoat;
    result.clearcoat_roughness = material.clearcoat_roughness;
    result.sheen = material.sheen;
    result.anisotropy = material.anisotropy;
    result.illumination_model = material.illumination_model;
    result.model_material = handle;
    return result;
}

} // namespace

ModelHandle load (const std::string& path, std::string *error)
{
    if (error)
    {
        error->clear();
    }

    const std::string key = normalizedPath(path);
    const auto found = cache().find(key);

    if (found != cache().end())
    {
        return found->second;
    }

    Obj::Document document;

    if (!Obj::load(key, &document, error))
    {
        return INVALID_MODEL;
    }

    const bool legacy_zero_d_is_opaque =
        detectLegacyZeroDIsOpaque(document.materials);

    const std::uint32_t material_base =
        static_cast<std::uint32_t>(materials().size());

    std::vector<Renderer::Material::MaterialHandle> resolved_materials;
    resolved_materials.reserve(document.materials.size());

    for (MaterialData& material : document.materials)
    {
        Renderer::Material::Resource resource = resolveMaterial(
                material,
                legacy_zero_d_is_opaque,
                &document.warnings
            );
        resolved_materials.push_back(
            Renderer::Material::registerMaterial(std::move(resource))
        );
        materials().push_back(std::move(material));
    }

    Model model;
    model.path = key;
    model.parts.reserve(document.submeshes.size());

    for (Obj::Submesh& submesh : document.submeshes)
    {
        const std::uint32_t mesh =
            Renderer::Mesh::registerLoadedMesh(std::move(submesh.mesh));

        const bool has_material =
            submesh.material_index != UINT32_MAX
            && submesh.material_index < resolved_materials.size();

        const std::uint32_t material = has_material
            ? material_base + submesh.material_index
            : Ecs::INVALID_ASSET_HANDLE;

        const Renderer::Material::MaterialHandle renderer_material =
            has_material
            ? resolved_materials[submesh.material_index]
            : Renderer::Material::INVALID_MATERIAL;

        model.parts.push_back({mesh, material, renderer_material});
    }

    const ModelHandle handle =
        static_cast<ModelHandle>(models().size());

    models().push_back(std::move(model));
    cache().emplace(key, handle);
    return handle;
}

std::vector<Ecs::Entity> spawn (
                Ecs::World& world,
                ModelHandle handle,
                const SpawnOptions& options,
                std::string *error
        )
{
    if (error)
    {
        error->clear();
    }

    std::vector<Ecs::Entity> entities;

    if (handle >= models().size())
    {
        if (error)
        {
            *error = "invalid model handle";
        }
        return entities;
    }

    const Model& model = models()[handle];
    entities.reserve(model.parts.size());

    for (const Part& part : model.parts)
    {
        const Ecs::Entity entity = world.createEntity();
        world.addTransform(entity, options.transform);
        world.addMesh(entity, {Ecs::MeshType::Cube, part.mesh});
        world.addRenderable(entity, {options.visible});
        world.addMaterial(
            entity,
            materialComponent(part.material, part.renderer_material)
        );
        entities.push_back(entity);
    }

    return entities;
}

std::vector<Ecs::Entity> loadInto (
                Ecs::World& world,
                const std::string& path,
                const SpawnOptions& options,
                std::string *error
        )
{
    const ModelHandle model = load(path, error);

    if (model == INVALID_MODEL)
    {
        return {};
    }

    return spawn(world, model, options, error);
}

void clearCache ()
{
    cache().clear();
    models().clear();
    materials().clear();
    Renderer::Material::clear();
    clearTextureCache();
    Renderer::Mesh::clearLoadedMeshes();
}

} // namespace Models
