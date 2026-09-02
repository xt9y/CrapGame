#include "GlobalDistanceField.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Renderer 
{
namespace Lumen 
{
namespace 
{

std::size_t index (
                int x,
                int y,
                int z,
                int resolution
        ) 
{
    return static_cast<std::size_t>(z) *
               static_cast<std::size_t>(resolution) *
               static_cast<std::size_t>(resolution) +
           static_cast<std::size_t>(y) *
               static_cast<std::size_t>(resolution) +
           static_cast<std::size_t>(x);
}

bool contains (
                const Math::Vec3& center,
                float half_extent,
                const Math::Vec3& position
        ) 
{
    return std::fabs(position.x - center.x) <= half_extent
        && std::fabs(position.y - center.y) <= half_extent
        && std::fabs(position.z - center.z) <= half_extent;
}

} // namespace

void GlobalDistanceField::build (
                const DistanceFieldScene& scene,
                const Math::Vec3& camera_position
        ) 
{
    clipmaps_.resize(3u);

    buildClipmap(&clipmaps_[0], scene, camera_position, 8.0f, 18);
    buildClipmap(&clipmaps_[1], scene, camera_position, 24.0f, 18);
    buildClipmap(&clipmaps_[2], scene, camera_position, 64.0f, 18);
}

float GlobalDistanceField::sample (
                const Math::Vec3& position
        ) const 
{
    for (const Clipmap& clipmap : clipmaps_) 
    {
        if (contains(clipmap.center, clipmap.half_extent, position)) 
        {
            return sampleClipmap(clipmap, position);
        }
    }

    return std::numeric_limits<float>::max();
}

void GlobalDistanceField::buildClipmap (
                Clipmap *clipmap,
                const DistanceFieldScene& scene,
                const Math::Vec3& camera_position,
                float half_extent,
                int resolution
        ) 
{
    if (!clipmap) 
    {
        return;
    }

    clipmap->half_extent = half_extent;
    clipmap->resolution = std::max(4, resolution);

    const float voxel_size =
        half_extent * 2.0f / static_cast<float>(clipmap->resolution);

    clipmap->center = {
        std::floor(camera_position.x / voxel_size) * voxel_size,
        std::floor(camera_position.y / voxel_size) * voxel_size,
        std::floor(camera_position.z / voxel_size) * voxel_size,
    };

    const std::size_t voxel_count =
        static_cast<std::size_t>(clipmap->resolution) *
        static_cast<std::size_t>(clipmap->resolution) *
        static_cast<std::size_t>(clipmap->resolution);

    clipmap->distance.resize(voxel_count);

    const Math::Vec3 minimum = {
        clipmap->center.x - half_extent,
        clipmap->center.y - half_extent,
        clipmap->center.z - half_extent,
    };

    for (int z = 0; z < clipmap->resolution; ++z) 
    {
        for (int y = 0; y < clipmap->resolution; ++y) 
        {
            for (int x = 0; x < clipmap->resolution; ++x) 
            {
                const Math::Vec3 position = {
                    minimum.x + (static_cast<float>(x) + 0.5f) * voxel_size,
                    minimum.y + (static_cast<float>(y) + 0.5f) * voxel_size,
                    minimum.z + (static_cast<float>(z) + 0.5f) * voxel_size,
                };

                clipmap->distance[index(
                        x,
                        y,
                        z,
                        clipmap->resolution
                    )] = scene.distance(position);
            }
        }
    }
}

float GlobalDistanceField::sampleClipmap (
                const Clipmap& clipmap,
                const Math::Vec3& position
        ) const 
{
    if (clipmap.resolution <= 1
            || clipmap.distance.empty()) 
    {
        return std::numeric_limits<float>::max();
    }

    const float extent = clipmap.half_extent * 2.0f;

    const Math::Vec3 minimum = {
        clipmap.center.x - clipmap.half_extent,
        clipmap.center.y - clipmap.half_extent,
        clipmap.center.z - clipmap.half_extent,
    };

    const float normalized_x = Math::clamp(
                (position.x - minimum.x) / extent,
                0.0f,
                1.0f
            ),
            normalized_y = Math::clamp(
                (position.y - minimum.y) / extent,
                0.0f,
                1.0f
            ),
            normalized_z = Math::clamp(
                (position.z - minimum.z) / extent,
                0.0f,
                1.0f
            );

    const float grid_x = normalized_x *
                static_cast<float>(clipmap.resolution - 1),
                grid_y = normalized_y *
                static_cast<float>(clipmap.resolution - 1),
                grid_z = normalized_z *
                static_cast<float>(clipmap.resolution - 1);

    const int x0 = static_cast<int>(std::floor(grid_x)),
              y0 = static_cast<int>(std::floor(grid_y)),
              z0 = static_cast<int>(std::floor(grid_z)),
              x1 = std::min(clipmap.resolution - 1, x0 + 1),
              y1 = std::min(clipmap.resolution - 1, y0 + 1),
              z1 = std::min(clipmap.resolution - 1, z0 + 1);

    const float tx = grid_x - static_cast<float>(x0),
                ty = grid_y - static_cast<float>(y0),
                tz = grid_z - static_cast<float>(z0);

    const auto sample_value = [&] (int x, int y, int z) 
    {
        return clipmap.distance[index(
                x,
                y,
                z,
                clipmap.resolution
            )];
    };

    const float c00 = sample_value(x0, y0, z0) * (1.0f - tx) + sample_value(x1, y0, z0) * tx,
                c10 = sample_value(x0, y1, z0) * (1.0f - tx) + sample_value(x1, y1, z0) * tx,
                c01 = sample_value(x0, y0, z1) * (1.0f - tx) + sample_value(x1, y0, z1) * tx,
                c11 = sample_value(x0, y1, z1) * (1.0f - tx) + sample_value(x1, y1, z1) * tx,
                c0 = c00 * (1.0f - ty) + c10 * ty,
                c1 = c01 * (1.0f - ty) + c11 * ty;

    return c0 * (1.0f - tz) + c1 * tz;
}

} // namespace Lumen
} // namespace Renderer
