#include "Ecs/Ecs.hpp"
#include "Models/Material.hpp"
#include "Models/Models.hpp"
#include "Models/Obj.hpp"
#include "Models/Texture.hpp"
#include "Renderer/Material/Material.hpp"

#include <cassert>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <string>

namespace
{
bool isDdn(const std::string& path)
{
    std::string stem=std::filesystem::path(path).stem().string();
    for(char& c:stem)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return stem.size()>=4u&&stem.compare(stem.size()-4u,4u,"_ddn")==0;
}

bool visiblyColored(const Models::TextureAsset& asset)
{
    const auto& rgba=asset.image.rgba;
    for(std::size_t i=0;i+3u<rgba.size();i+=4u)
    {
        const int r=rgba[i],g=rgba[i+1u],b=rgba[i+2u];
        if(std::abs(r-g)>8||std::abs(g-b)>8||std::abs(r-b)>8)return true;
    }
    return false;
}
}

int main(int argc,char** argv)
{
    const std::string path=argc>1?argv[1]:"Assets/Sponza/sponza.obj";
    Models::clearCache();

    Models::Obj::Document document;
    std::string error;
    assert(Models::Obj::load(path,&document,&error));
    assert(error.empty());
    assert(document.materials.size()==25u);
    assert(Models::detectLegacyZeroDIsOpaque(document.materials));

    const Models::ModelHandle model=Models::load(path,&error);
    assert(model!=Models::INVALID_MODEL);
    assert(error.empty());
    assert(Renderer::Material::count()==document.materials.size());

    std::size_t diffuse_maps=0u;
    std::size_t normal_maps=0u;
    std::size_t opacity_maps=0u;
    std::size_t colored_diffuse_maps=0u;
    for(std::size_t i=0;i<document.materials.size();++i)
    {
        const Models::MaterialData& parsed=document.materials[i];
        const Renderer::Material::Resource* resource=Renderer::Material::get(
            static_cast<Renderer::Material::MaterialHandle>(i));
        assert(resource);
        assert(resource->opacity>0.999f);

        const auto base=resource->textures[
            Renderer::Material::slotIndex(Renderer::Material::Slot::BaseColor)];
        if(!parsed.base_color_texture.path.empty())
        {
            ++diffuse_maps;
            assert(base.texture!=Models::INVALID_TEXTURE);
            const Models::TextureAsset* texture=Models::texture(base.texture);
            assert(texture&&texture->image.width>0&&texture->image.height>0);
            if(visiblyColored(*texture))++colored_diffuse_maps;
        }

        if(isDdn(parsed.displacement_texture.path))
        {
            ++normal_maps;
            const auto normal=resource->textures[
                Renderer::Material::slotIndex(Renderer::Material::Slot::Normal)];
            const auto displacement=resource->textures[
                Renderer::Material::slotIndex(Renderer::Material::Slot::Displacement)];
            assert(normal.texture!=Models::INVALID_TEXTURE);
            assert(displacement.texture==Models::INVALID_TEXTURE);
        }

        if(!parsed.opacity_texture.path.empty())
        {
            ++opacity_maps;
            const auto opacity=resource->textures[
                Renderer::Material::slotIndex(Renderer::Material::Slot::Opacity)];
            assert(opacity.texture!=Models::INVALID_TEXTURE);
            assert(resource->render_class==Renderer::Material::RenderClass::Masked);
        }
        else
        {
            assert(resource->render_class==Renderer::Material::RenderClass::Opaque);
        }
    }

    assert(diffuse_maps>=20u);
    assert(normal_maps>=18u);
    assert(opacity_maps==3u);
    assert(colored_diffuse_maps>=3u);

    Ecs::World world;
    const std::vector<Ecs::Entity> entities=Models::spawn(world,model,{},&error);
    assert(!entities.empty());
    assert(error.empty());
    for(Ecs::Entity entity:entities)
    {
        const Ecs::MeshComponent* mesh=world.getMesh(entity);
        const Ecs::MaterialComponent* material=world.getMaterial(entity);
        assert(mesh&&mesh->loaded_mesh!=Ecs::INVALID_ASSET_HANDLE);
        assert(material&&material->renderer_material!=Ecs::INVALID_ASSET_HANDLE);
    }

    Models::clearCache();
    return 0;
}
