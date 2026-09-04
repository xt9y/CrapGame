#include "Models/Material.hpp"
#include "Models/Texture.hpp"
#include "Renderer/Gpu/DirectLightingImportedShader.hpp"
#include "Renderer/Material/Material.hpp"

#include <cassert>
#include <cstdint>
#include <fstream>
#include <string>

namespace
{
void writePixelTga(const std::string& path,std::uint8_t value)
{
    unsigned char header[18]={};
    header[2]=2;
    header[12]=1;
    header[14]=1;
    header[16]=24;
    header[17]=0x20;
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(header),sizeof(header));
    const unsigned char pixel[3]={value,value,value};
    out.write(reinterpret_cast<const char*>(pixel),sizeof(pixel));
}
}

int main()
{
    const std::string path="/tmp/crapgame-material-map-default.tga";
    writePixelTga(path,255u);
    Models::clearTextureCache();

    Models::MaterialData material;
    material.metallic_texture.path=path;
    material.shininess_texture.path=path;
    material.emissive_texture.path=path;
    material.specular_texture.path=path;
    material.reflection_texture.path=path;
    material.transmission_texture.path=path;
    material.clearcoat_texture.path=path;
    material.clearcoat_roughness_texture.path=path;
    material.sheen_texture.path=path;
    material.anisotropy_texture.path=path;

    std::vector<std::string> warnings;
    const Renderer::Material::Resource resource=
        Models::resolveMaterial(material,false,&warnings);
    assert(warnings.empty());
    assert(resource.metallic>0.99f);
    assert(resource.transmission>0.99f);
    assert(resource.reflectivity>0.99f);
    assert(resource.clearcoat>0.99f);
    assert(resource.clearcoat_roughness>0.99f);
    assert(resource.sheen>0.99f);
    assert(resource.anisotropy>0.90f);
    assert(resource.emissive.x>0.99f&&resource.emissive.y>0.99f&&resource.emissive.z>0.99f);
    assert(resource.specular.x>0.99f&&resource.specular.y>0.99f&&resource.specular.z>0.99f);
    assert(resource.render_class==Renderer::Material::RenderClass::Transmissive);

    const auto roughness_index=Renderer::Material::slotIndex(Renderer::Material::Slot::Roughness);
    const auto shininess_index=Renderer::Material::slotIndex(Renderer::Material::Slot::Shininess);
    assert(resource.textures[roughness_index].texture!=Models::INVALID_TEXTURE);
    assert(resource.textures[shininess_index].texture==Models::INVALID_TEXTURE);
    assert(resource.roughness>0.99f);
    const Models::TextureAsset* roughness=Models::texture(resource.textures[roughness_index].texture);
    assert(roughness&&roughness->image.rgba.size()==4u);
    assert(roughness->image.rgba[0]<32u);

    const std::string shader=Renderer::Gpu::directLightingImportedShader();
    assert(shader.find("ambient*albedo*0.025")!=std::string::npos);
    assert(shader.find("importedShadowVisibility")!=std::string::npos);

    Models::clearTextureCache();
    return 0;
}
