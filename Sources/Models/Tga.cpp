#include "Models/Tga.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <utility>

namespace Models
{
namespace Tga
{
namespace
{

struct Header
{
    std::uint8_t id_length = 0;
    std::uint8_t color_map_type = 0;
    std::uint8_t image_type = 0;
    std::uint16_t color_map_first = 0;
    std::uint16_t color_map_length = 0;
    std::uint8_t color_map_depth = 0;
    std::uint16_t x_origin = 0;
    std::uint16_t y_origin = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint8_t pixel_depth = 0;
    std::uint8_t descriptor = 0;
};

struct Pixel
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

bool fail(std::string *error, const std::string& message)
{
    if (error)
    {
        *error = message;
    }
    return false;
}

std::uint16_t read16(const std::uint8_t *bytes)
{
    return static_cast<std::uint16_t>(bytes[0])
        | static_cast<std::uint16_t>(bytes[1]) << 8u;
}

Header parseHeader(const std::uint8_t *bytes)
{
    Header h;
    h.id_length = bytes[0];
    h.color_map_type = bytes[1];
    h.image_type = bytes[2];
    h.color_map_first = read16(bytes + 3);
    h.color_map_length = read16(bytes + 5);
    h.color_map_depth = bytes[7];
    h.x_origin = read16(bytes + 8);
    h.y_origin = read16(bytes + 10);
    h.width = read16(bytes + 12);
    h.height = read16(bytes + 14);
    h.pixel_depth = bytes[16];
    h.descriptor = bytes[17];
    return h;
}

std::vector<std::uint8_t> readWholeFile(
    const std::string& path,
    std::string *error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        fail(error, "failed to open TGA: " + path);
        return {};
    }

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0)
    {
        fail(error, "failed to determine TGA size: " + path);
        return {};
    }
    input.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty())
    {
        input.read(
            reinterpret_cast<char *>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())
        );
        if (!input)
        {
            fail(error, "failed to read TGA: " + path);
            return {};
        }
    }
    return bytes;
}

bool isColorMapped(std::uint8_t type)
{
    return type == 1u || type == 9u;
}

bool isTrueColor(std::uint8_t type)
{
    return type == 2u || type == 10u;
}

bool isGray(std::uint8_t type)
{
    return type == 3u || type == 11u;
}

bool isRle(std::uint8_t type)
{
    return type == 9u || type == 10u || type == 11u;
}

bool validColorDepth(std::uint8_t depth)
{
    return depth == 15u || depth == 16u || depth == 24u || depth == 32u;
}

std::size_t byteWidth(std::uint8_t bits)
{
    return (static_cast<std::size_t>(bits) + 7u) / 8u;
}

bool validate(const Header& h, std::size_t size, std::string *error)
{
    if (h.width == 0u || h.height == 0u)
    {
        return fail(error, "invalid zero-sized TGA");
    }
    if (!isColorMapped(h.image_type)
            && !isTrueColor(h.image_type)
            && !isGray(h.image_type))
    {
        return fail(error, "unsupported TGA image type");
    }

    if (isColorMapped(h.image_type))
    {
        if (h.color_map_type != 1u || h.color_map_length == 0u)
        {
            return fail(error, "color-mapped TGA has no color map");
        }
        if (h.pixel_depth != 8u && h.pixel_depth != 16u)
        {
            return fail(error, "unsupported TGA color-map index depth");
        }
        if (!validColorDepth(h.color_map_depth))
        {
            return fail(error, "unsupported TGA color-map entry depth");
        }
    }
    else if (h.color_map_type != 0u)
    {
        return fail(error, "unexpected TGA color map");
    }

    if (isTrueColor(h.image_type) && !validColorDepth(h.pixel_depth))
    {
        return fail(error, "unsupported TGA true-color depth");
    }
    if (isGray(h.image_type) && h.pixel_depth != 8u && h.pixel_depth != 16u)
    {
        return fail(error, "unsupported TGA grayscale depth");
    }

    const std::size_t start = 18u + static_cast<std::size_t>(h.id_length);
    if (start > size)
    {
        return fail(error, "truncated TGA image id");
    }

    const std::size_t palette_bytes = isColorMapped(h.image_type)
        ? static_cast<std::size_t>(h.color_map_length) * byteWidth(h.color_map_depth)
        : 0u;
    if (palette_bytes > size - start)
    {
        return fail(error, "truncated TGA color map");
    }

    const std::uint64_t pixel_count =
        static_cast<std::uint64_t>(h.width) * static_cast<std::uint64_t>(h.height);
    if (pixel_count > std::numeric_limits<std::size_t>::max() / 4u)
    {
        return fail(error, "TGA dimensions overflow output buffer");
    }
    return true;
}

Pixel decodeColor(
    const std::uint8_t *data,
    std::uint8_t depth,
    bool alpha_bit_valid)
{
    Pixel p;
    if (depth == 15u || depth == 16u)
    {
        const std::uint16_t packed = read16(data);
        const std::uint8_t b5 = static_cast<std::uint8_t>(packed & 31u);
        const std::uint8_t g5 = static_cast<std::uint8_t>((packed >> 5u) & 31u);
        const std::uint8_t r5 = static_cast<std::uint8_t>((packed >> 10u) & 31u);
        p.r = static_cast<std::uint8_t>((r5 << 3u) | (r5 >> 2u));
        p.g = static_cast<std::uint8_t>((g5 << 3u) | (g5 >> 2u));
        p.b = static_cast<std::uint8_t>((b5 << 3u) | (b5 >> 2u));
        p.a = depth == 16u && alpha_bit_valid
            ? ((packed & 0x8000u) != 0u ? 255u : 0u)
            : 255u;
    }
    else
    {
        p.b = data[0];
        p.g = data[1];
        p.r = data[2];
        p.a = depth == 32u ? data[3] : 255u;
    }
    return p;
}

bool readIndex(
    const std::vector<std::uint8_t>& bytes,
    std::size_t *cursor,
    std::uint8_t depth,
    std::uint16_t *value,
    std::string *error)
{
    const std::size_t count = depth == 16u ? 2u : 1u;
    if (*cursor > bytes.size() || bytes.size() - *cursor < count)
    {
        return fail(error, "truncated TGA color-map index");
    }
    *value = depth == 16u ? read16(bytes.data() + *cursor) : bytes[*cursor];
    *cursor += count;
    return true;
}

bool readSourcePixel(
    const std::vector<std::uint8_t>& bytes,
    const Header& h,
    std::size_t palette_start,
    std::size_t *cursor,
    Pixel *pixel,
    std::string *error)
{
    if (isColorMapped(h.image_type))
    {
        std::uint16_t index = 0;
        if (!readIndex(bytes, cursor, h.pixel_depth, &index, error))
        {
            return false;
        }
        const std::uint32_t first = h.color_map_first;
        const std::uint32_t end = first + h.color_map_length;
        if (index < first || index >= end)
        {
            return fail(error, "TGA color-map index out of range");
        }
        const std::size_t entry_bytes = byteWidth(h.color_map_depth);
        const std::size_t offset = palette_start
            + static_cast<std::size_t>(index - first) * entry_bytes;
        if (offset > bytes.size() || bytes.size() - offset < entry_bytes)
        {
            return fail(error, "truncated TGA color-map entry");
        }
        *pixel = decodeColor(
            bytes.data() + offset,
            h.color_map_depth,
            h.color_map_depth == 16u
        );
        return true;
    }

    if (isGray(h.image_type))
    {
        const std::size_t count = h.pixel_depth == 16u ? 2u : 1u;
        if (*cursor > bytes.size() || bytes.size() - *cursor < count)
        {
            return fail(error, "truncated TGA grayscale pixel");
        }
        const std::uint8_t gray = bytes[*cursor];
        pixel->r = gray;
        pixel->g = gray;
        pixel->b = gray;
        pixel->a = h.pixel_depth == 16u ? bytes[*cursor + 1u] : 255u;
        *cursor += count;
        return true;
    }

    const std::size_t count = byteWidth(h.pixel_depth);
    if (*cursor > bytes.size() || bytes.size() - *cursor < count)
    {
        return fail(error, "truncated TGA true-color pixel");
    }
    *pixel = decodeColor(
        bytes.data() + *cursor,
        h.pixel_depth,
        (h.descriptor & 0x0fu) != 0u
    );
    *cursor += count;
    return true;
}

void writePixel(
    const Header& h,
    std::size_t linear_index,
    const Pixel& pixel,
    Image *image)
{
    const std::size_t source_y = linear_index / h.width;
    const std::size_t source_x = linear_index % h.width;
    const bool top_origin = (h.descriptor & 0x20u) != 0u;
    const bool right_origin = (h.descriptor & 0x10u) != 0u;

    const std::size_t x = right_origin
        ? static_cast<std::size_t>(h.width) - 1u - source_x
        : source_x;
    const std::size_t y = top_origin
        ? source_y
        : static_cast<std::size_t>(h.height) - 1u - source_y;
    const std::size_t output = (y * h.width + x) * 4u;
    image->rgba[output + 0u] = pixel.r;
    image->rgba[output + 1u] = pixel.g;
    image->rgba[output + 2u] = pixel.b;
    image->rgba[output + 3u] = pixel.a;
    if (pixel.a != 255u)
    {
        image->meaningful_alpha = true;
    }
}

bool decodePixels(
    const std::vector<std::uint8_t>& bytes,
    const Header& h,
    Image *image,
    std::string *error)
{
    const std::size_t image_start = 18u + static_cast<std::size_t>(h.id_length);
    const std::size_t palette_bytes = isColorMapped(h.image_type)
        ? static_cast<std::size_t>(h.color_map_length) * byteWidth(h.color_map_depth)
        : 0u;
    const std::size_t palette_start = image_start;
    std::size_t cursor = image_start + palette_bytes;
    const std::size_t pixel_count =
        static_cast<std::size_t>(h.width) * static_cast<std::size_t>(h.height);

    image->width = static_cast<int>(h.width);
    image->height = static_cast<int>(h.height);
    image->meaningful_alpha = false;
    image->rgba.assign(pixel_count * 4u, 0u);

    std::size_t output_index = 0u;
    if (!isRle(h.image_type))
    {
        while (output_index < pixel_count)
        {
            Pixel pixel;
            if (!readSourcePixel(bytes, h, palette_start, &cursor, &pixel, error))
            {
                return false;
            }
            writePixel(h, output_index++, pixel, image);
        }
        return true;
    }

    while (output_index < pixel_count)
    {
        if (cursor >= bytes.size())
        {
            return fail(error, "truncated TGA RLE packet header");
        }
        const std::uint8_t packet = bytes[cursor++];
        const std::size_t count = static_cast<std::size_t>(packet & 0x7fu) + 1u;
        if (count > pixel_count - output_index)
        {
            return fail(error, "TGA RLE packet exceeds image bounds");
        }

        if ((packet & 0x80u) != 0u)
        {
            Pixel pixel;
            if (!readSourcePixel(bytes, h, palette_start, &cursor, &pixel, error))
            {
                return false;
            }
            for (std::size_t i = 0; i < count; ++i)
            {
                writePixel(h, output_index++, pixel, image);
            }
        }
        else
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                Pixel pixel;
                if (!readSourcePixel(bytes, h, palette_start, &cursor, &pixel, error))
                {
                    return false;
                }
                writePixel(h, output_index++, pixel, image);
            }
        }
    }
    return true;
}

} // namespace

bool load(const std::string& path, Image *image, std::string *error)
{
    if (error)
    {
        error->clear();
    }
    if (!image)
    {
        return fail(error, "null TGA output image");
    }

    const std::vector<std::uint8_t> bytes = readWholeFile(path, error);
    if (bytes.size() < 18u)
    {
        if (error && error->empty())
        {
            *error = "truncated TGA header";
        }
        return false;
    }

    const Header header = parseHeader(bytes.data());
    if (!validate(header, bytes.size(), error))
    {
        return false;
    }

    Image decoded;
    if (!decodePixels(bytes, header, &decoded, error))
    {
        return false;
    }
    *image = std::move(decoded);
    return true;
}

} // namespace Tga
} // namespace Models
