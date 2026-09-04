#include "Models/Tga.hpp"
#include "Models/Texture.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

void put16(std::vector<std::uint8_t> *out, std::size_t offset, std::uint16_t value)
{
    (*out)[offset] = static_cast<std::uint8_t>(value & 0xffu);
    (*out)[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
}

std::vector<std::uint8_t> header(
    std::uint8_t type,
    int width,
    int height,
    std::uint8_t depth,
    std::uint8_t descriptor = 0x20u)
{
    std::vector<std::uint8_t> bytes(18u, 0u);
    bytes[2] = type;
    put16(&bytes, 12u, static_cast<std::uint16_t>(width));
    put16(&bytes, 14u, static_cast<std::uint16_t>(height));
    bytes[16] = depth;
    bytes[17] = descriptor;
    return bytes;
}

void writeFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
{
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void appendBgr(std::vector<std::uint8_t> *bytes, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    bytes->push_back(b);
    bytes->push_back(g);
    bytes->push_back(r);
}

void appendBgra(std::vector<std::uint8_t> *bytes, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    appendBgr(bytes, r, g, b);
    bytes->push_back(a);
}

void expectPixel(const Models::Tga::Image& image, int x, int y,
                 int r, int g, int b, int a = 255)
{
    const std::size_t i = (static_cast<std::size_t>(y) * image.width + x) * 4u;
    assert(image.rgba[i] == r);
    assert(image.rgba[i + 1u] == g);
    assert(image.rgba[i + 2u] == b);
    assert(image.rgba[i + 3u] == a);
}

} // namespace

int main()
{
    const std::filesystem::path dir = "/tmp/crapgame-tga-contract";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "sub");

    // Top-left, uncompressed 24-bit.
    auto rgb = header(2u, 2, 2, 24u, 0x20u);
    appendBgr(&rgb, 255, 0, 0);
    appendBgr(&rgb, 0, 255, 0);
    appendBgr(&rgb, 0, 0, 255);
    appendBgr(&rgb, 255, 255, 255);
    writeFile(dir / "rgb.tga", rgb);

    Models::Tga::Image image;
    std::string error;
    assert(Models::Tga::load((dir / "rgb.tga").string(), &image, &error));
    assert(image.width == 2 && image.height == 2 && !image.meaningful_alpha);
    expectPixel(image, 0, 0, 255, 0, 0);
    expectPixel(image, 1, 0, 0, 255, 0);
    expectPixel(image, 0, 1, 0, 0, 255);
    expectPixel(image, 1, 1, 255, 255, 255);

    // Bottom-right origin + 32-bit alpha must normalize to top-left.
    auto oriented = header(2u, 2, 2, 32u, 0x18u);
    appendBgra(&oriented, 255, 255, 255, 255); // bottom-right
    appendBgra(&oriented, 0, 0, 255, 255);     // bottom-left
    appendBgra(&oriented, 0, 255, 0, 128);     // top-right
    appendBgra(&oriented, 255, 0, 0, 255);     // top-left
    writeFile(dir / "oriented.tga", oriented);
    assert(Models::Tga::load((dir / "oriented.tga").string(), &image, &error));
    expectPixel(image, 0, 0, 255, 0, 0);
    expectPixel(image, 1, 0, 0, 255, 0, 128);
    expectPixel(image, 0, 1, 0, 0, 255);
    expectPixel(image, 1, 1, 255, 255, 255);
    assert(image.meaningful_alpha);

    // RLE true-color: 2 red then blue/green raw.
    auto rle = header(10u, 4, 1, 24u, 0x20u);
    rle.push_back(0x81u);
    appendBgr(&rle, 255, 0, 0);
    rle.push_back(0x01u);
    appendBgr(&rle, 0, 0, 255);
    appendBgr(&rle, 0, 255, 0);
    writeFile(dir / "rle.tga", rle);
    assert(Models::Tga::load((dir / "rle.tga").string(), &image, &error));
    expectPixel(image, 0, 0, 255, 0, 0);
    expectPixel(image, 1, 0, 255, 0, 0);
    expectPixel(image, 2, 0, 0, 0, 255);
    expectPixel(image, 3, 0, 0, 255, 0);

    // 16-bit grayscale (gray + alpha).
    auto gray = header(3u, 2, 1, 16u, 0x28u);
    gray.push_back(42u); gray.push_back(255u);
    gray.push_back(200u); gray.push_back(10u);
    writeFile(dir / "gray.tga", gray);
    assert(Models::Tga::load((dir / "gray.tga").string(), &image, &error));
    expectPixel(image, 0, 0, 42, 42, 42, 255);
    expectPixel(image, 1, 0, 200, 200, 200, 10);
    assert(image.meaningful_alpha);

    // Color map with non-zero first index.
    auto cmap = header(1u, 2, 1, 8u, 0x20u);
    cmap[1] = 1u;
    put16(&cmap, 3u, 5u);
    put16(&cmap, 5u, 2u);
    cmap[7] = 24u;
    appendBgr(&cmap, 10, 20, 30);
    appendBgr(&cmap, 40, 50, 60);
    cmap.push_back(5u);
    cmap.push_back(6u);
    writeFile(dir / "cmap.tga", cmap);
    assert(Models::Tga::load((dir / "cmap.tga").string(), &image, &error));
    expectPixel(image, 0, 0, 10, 20, 30);
    expectPixel(image, 1, 0, 40, 50, 60);

    // 16-bit true-color path.
    auto rgb16 = header(2u, 1, 1, 16u, 0x21u);
    rgb16.push_back(0x00u);
    rgb16.push_back(0xfcu); // opaque bright red in 5-5-5-1
    writeFile(dir / "rgb16.tga", rgb16);
    assert(Models::Tga::load((dir / "rgb16.tga").string(), &image, &error));
    assert(image.rgba[0] > 240u && image.rgba[1] == 0u && image.rgba[2] == 0u);

    // Cache normalization/reuse.
    Models::clearTextureCache();
    const Models::TextureHandle first = Models::loadTexture((dir / "rgb.tga").string(), &error);
    const Models::TextureHandle second = Models::loadTexture((dir / "sub" / ".." / "rgb.tga").string(), &error);
    assert(first != Models::INVALID_TEXTURE && first == second);
    assert(Models::texture(first) && Models::texture(first)->image.width == 2);

    // Truncation and malformed RLE must fail with an error.
    writeFile(dir / "truncated.tga", {0u, 0u, 2u});
    error.clear();
    assert(!Models::Tga::load((dir / "truncated.tga").string(), &image, &error));
    assert(!error.empty());

    auto bad_rle = header(10u, 1, 1, 24u, 0x20u);
    bad_rle.push_back(0x81u); // packet says 2 pixels into a 1-pixel image
    appendBgr(&bad_rle, 255, 0, 0);
    writeFile(dir / "bad-rle.tga", bad_rle);
    error.clear();
    assert(!Models::Tga::load((dir / "bad-rle.tga").string(), &image, &error));
    assert(!error.empty());

    auto zero = header(2u, 0, 1, 24u, 0x20u);
    writeFile(dir / "zero.tga", zero);
    error.clear();
    assert(!Models::Tga::load((dir / "zero.tga").string(), &image, &error));
    assert(!error.empty());

    return 0;
}
