#include "Models/Texture.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Models
{
namespace
{

std::vector<TextureAsset>& assets()
{
    static std::vector<TextureAsset> values;
    return values;
}

std::unordered_map<std::string, TextureHandle>& cache()
{
    static std::unordered_map<std::string, TextureHandle> values;
    return values;
}

std::unordered_map<std::string, TextureHandle>& derivedCache()
{
    static std::unordered_map<std::string, TextureHandle> values;
    return values;
}

std::string normalizedPath(const std::string& path)
{
    std::error_code error;
    const std::filesystem::path absolute =
        std::filesystem::absolute(std::filesystem::path(path), error);
    return (error ? std::filesystem::path(path) : absolute)
        .lexically_normal()
        .string();
}

float selectedChannel(const Tga::Image& image,std::size_t pixel,char channel)
{
    const std::size_t base=pixel*4u;
    const char key=static_cast<char>(std::tolower(static_cast<unsigned char>(channel)));
    if(key=='g')return static_cast<float>(image.rgba[base+1u])/255.0f;
    if(key=='b')return static_cast<float>(image.rgba[base+2u])/255.0f;
    if(key=='a')return static_cast<float>(image.rgba[base+3u])/255.0f;
    if(key=='m'||key=='l')
    {
        const float r=static_cast<float>(image.rgba[base]);
        const float g=static_cast<float>(image.rgba[base+1u]);
        const float b=static_cast<float>(image.rgba[base+2u]);
        return (r+g+b)/(3.0f*255.0f);
    }
    return static_cast<float>(image.rgba[base])/255.0f;
}

} // namespace

TextureHandle loadTexture(const std::string& path, std::string *error)
{
    if (error) error->clear();
    const std::string key = normalizedPath(path);
    const auto found = cache().find(key);
    if (found != cache().end()) return found->second;

    Tga::Image image;
    if (!Tga::load(key, &image, error)) return INVALID_TEXTURE;

    const TextureHandle handle = static_cast<TextureHandle>(assets().size());
    assets().push_back({key, std::move(image)});
    cache().emplace(key, handle);
    return handle;
}

bool normalMapUsesNegativeY(const std::string& path)
{
    std::string stem = std::filesystem::path(path).stem().string();
    std::transform(
        stem.begin(), stem.end(), stem.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });

    return (stem.size() >= 4u
            && stem.compare(stem.size() - 4u, 4u, "_ddn") == 0)
        || (stem.size() >= 5u
            && stem.compare(stem.size() - 5u, 5u, "_ddna") == 0);
}

TextureHandle shininessToRoughnessTexture(TextureHandle shininess,
                                           char channel,
                                           std::string *error)
{
    if(error)error->clear();
    const TextureAsset *source=texture(shininess);
    if(!source||source->image.width<=0||source->image.height<=0
            || source->image.rgba.size()!=static_cast<std::size_t>(source->image.width*source->image.height)*4u)
    {
        if(error)*error="invalid shininess texture for roughness conversion";
        return INVALID_TEXTURE;
    }

    const std::string key=source->path+"#mapNsToRoughness:"+
        std::string(1,channel=='\0'?'r':channel);
    const auto found=derivedCache().find(key);
    if(found!=derivedCache().end())return found->second;

    Tga::Image image;
    image.width=source->image.width;
    image.height=source->image.height;
    image.meaningful_alpha=false;
    image.rgba.resize(source->image.rgba.size());
    const std::size_t pixels=image.rgba.size()/4u;
    for(std::size_t pixel=0;pixel<pixels;++pixel)
    {
        const float normalized=selectedChannel(source->image,pixel,channel);
        const float ns=normalized*1000.0f;
        const float roughness=std::max(0.04f,std::min(1.0f,std::sqrt(2.0f/(ns+2.0f))));
        const std::uint8_t encoded=static_cast<std::uint8_t>(
            std::lround(roughness*255.0f));
        const std::size_t base=pixel*4u;
        image.rgba[base]=encoded;
        image.rgba[base+1u]=encoded;
        image.rgba[base+2u]=encoded;
        image.rgba[base+3u]=255u;
    }

    const TextureHandle handle=static_cast<TextureHandle>(assets().size());
    assets().push_back({key,std::move(image)});
    derivedCache().emplace(key,handle);
    return handle;
}

const TextureAsset *texture(TextureHandle handle)
{
    return handle < assets().size() ? &assets()[handle] : nullptr;
}

void clearTextureCache()
{
    cache().clear();
    derivedCache().clear();
    assets().clear();
}

} // namespace Models
