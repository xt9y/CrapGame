#ifndef CRAPGAME_MODELS_TGA_HPP
#define CRAPGAME_MODELS_TGA_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace Models
{
namespace Tga
{

struct Image
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
    bool meaningful_alpha = false;
};

bool load(
    const std::string& path,
    Image *image,
    std::string *error = nullptr
);

} // namespace Tga
} // namespace Models

#endif
