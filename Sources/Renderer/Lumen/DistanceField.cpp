#include "DistanceField.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Renderer 
{
namespace Lumen 
{
namespace 
{

constexpr float EPSILON = 0.000001f;

float pointTriangleDistance (
                const Math::Vec3& point,
                const Math::Vec3& a,
                const Math::Vec3& b,
                const Math::Vec3& c
        ) 
{
    const Math::Vec3 ab = Math::subtract(b, a),
                     ac = Math::subtract(c, a),
                     ap = Math::subtract(point, a);

    const float d1 = Math::dot(ab, ap),
                d2 = Math::dot(ac, ap);

    if (d1 <= 0.0f && d2 <= 0.0f) 
    {
        return Math::length(ap);
    }

    const Math::Vec3 bp = Math::subtract(point, b);
    const float d3 = Math::dot(ab, bp),
                d4 = Math::dot(ac, bp);

    if (d3 >= 0.0f && d4 <= d3) 
    {
        return Math::length(bp);
    }

    const float vc = d1 * d4 - d3 * d2;

    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) 
    {
        const float v = d1 / (d1 - d3);
        return Math::length(
                Math::subtract(
                        point,
                        Math::add(a, Math::multiply(ab, v))
                    )
            );
    }

    const Math::Vec3 cp = Math::subtract(point, c);
    const float d5 = Math::dot(ab, cp),
                d6 = Math::dot(ac, cp);

    if (d6 >= 0.0f && d5 <= d6) 
    {
        return Math::length(cp);
    }

    const float vb = d5 * d2 - d1 * d6;

    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) 
    {
        const float w = d2 / (d2 - d6);
        return Math::length(
                Math::subtract(
                        point,
                        Math::add(a, Math::multiply(ac, w))
                    )
            );
    }

    const float va = d3 * d6 - d5 * d4;

    if (va <= 0.0f
            && d4 - d3 >= 0.0f
            && d5 - d6 >= 0.0f) 
    {
        const Math::Vec3 bc = Math::subtract(c, b);
        const float w = (d4 - d3) /
                        ((d4 - d3) + (d5 - d6));

        return Math::length(
                Math::subtract(
                        point,
                        Math::add(b, Math::multiply(bc, w))
                    )
            );
    }

    const Math::Vec3 normal =
        Math::normalize(Math::cross(ab, ac));

    return std::fabs(Math::dot(ap, normal));
}

bool rayTriangle (
                const Math::Vec3& origin,
                const Math::Vec3& direction,
                const Math::Vec3& a,
                const Math::Vec3& b,
                const Math::Vec3& c,
                float *distance
        ) 
{
    const Math::Vec3 edge_a = Math::subtract(b, a),
                     edge_b = Math::subtract(c, a),
                     p = Math::cross(direction, edge_b);

    const float determinant = Math::dot(edge_a, p);

    if (std::fabs(determinant) <= EPSILON) 
    {
        return false;
    }

    const float inverse = 1.0f / determinant;
    const Math::Vec3 t = Math::subtract(origin, a);
    const float u = Math::dot(t, p) * inverse;

    if (u < 0.0f || u > 1.0f) 
    {
        return false;
    }

    const Math::Vec3 q = Math::cross(t, edge_a);
    const float v = Math::dot(direction, q) * inverse;

    if (v < 0.0f || u + v > 1.0f) 
    {
        return false;
    }

    const float hit_distance =
        Math::dot(edge_b, q) * inverse;

    if (hit_distance <= EPSILON) 
    {
        return false;
    }

    if (distance) 
    {
        *distance = hit_distance;
    }

    return true;
}

bool isVolumetric (const Mesh::Bounds& bounds) 
{
    const Math::Vec3 extent =
        Math::subtract(bounds.maximum, bounds.minimum);

    return extent.x > EPSILON
        && extent.y > EPSILON
        && extent.z > EPSILON;
}

bool isInside (
                const Mesh::MeshData& mesh,
                const Math::Vec3& position
        ) 
{
    const Math::Vec3 direction =
        Math::normalize({1.0f, 0.3713907f, 0.2171131f});

    int intersections = 0;

    for (std::size_t triangle = 0;
            triangle + 2u < mesh.indices.size();
            triangle += 3u) 
    {
        float hit_distance = 0.0f;

        if (rayTriangle(
                position,
                direction,
                mesh.vertices[mesh.indices[triangle + 0u]].position,
                mesh.vertices[mesh.indices[triangle + 1u]].position,
                mesh.vertices[mesh.indices[triangle + 2u]].position,
                &hit_distance
            )) 
        {
            ++intersections;
        }
    }

    return (intersections & 1) != 0;
}

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

} // namespace

MeshDistanceField buildDistanceField (
                const Mesh::MeshData& mesh,
                int resolution
        ) 
{
    MeshDistanceField field;

    field.resolution = std::max(4, resolution);
    field.signed_distance = isVolumetric(mesh.bounds);

    const Math::Vec3 original_extent =
        Math::subtract(mesh.bounds.maximum, mesh.bounds.minimum);

    const float maximum_extent = std::max(
            original_extent.x,
            std::max(original_extent.y, original_extent.z)
        );

    const float padding = std::max(0.05f, maximum_extent * 0.15f);

    field.bounds.minimum = {
        mesh.bounds.minimum.x - padding,
        mesh.bounds.minimum.y - padding,
        mesh.bounds.minimum.z - padding,
    };

    field.bounds.maximum = {
        mesh.bounds.maximum.x + padding,
        mesh.bounds.maximum.y + padding,
        mesh.bounds.maximum.z + padding,
    };

    const std::size_t voxel_count =
        static_cast<std::size_t>(field.resolution) *
        static_cast<std::size_t>(field.resolution) *
        static_cast<std::size_t>(field.resolution);

    field.distance.resize(voxel_count);

    const Math::Vec3 extent =
        Math::subtract(field.bounds.maximum, field.bounds.minimum);

    for (int z = 0; z < field.resolution; ++z) 
    {
        for (int y = 0; y < field.resolution; ++y) 
        {
            for (int x = 0; x < field.resolution; ++x) 
            {
                const Math::Vec3 position = {
                    field.bounds.minimum.x +
                        (static_cast<float>(x) + 0.5f) /
                        static_cast<float>(field.resolution) * extent.x,
                    field.bounds.minimum.y +
                        (static_cast<float>(y) + 0.5f) /
                        static_cast<float>(field.resolution) * extent.y,
                    field.bounds.minimum.z +
                        (static_cast<float>(z) + 0.5f) /
                        static_cast<float>(field.resolution) * extent.z,
                };

                float minimum_distance =
                    std::numeric_limits<float>::max();

                for (std::size_t triangle = 0;
                        triangle + 2u < mesh.indices.size();
                        triangle += 3u) 
                {
                    minimum_distance = std::min(
                            minimum_distance,
                            pointTriangleDistance(
                                    position,
                                    mesh.vertices[mesh.indices[triangle + 0u]].position,
                                    mesh.vertices[mesh.indices[triangle + 1u]].position,
                                    mesh.vertices[mesh.indices[triangle + 2u]].position
                                )
                        );
                }

                if (field.signed_distance
                        && isInside(mesh, position)) 
                {
                    minimum_distance = -minimum_distance;
                }

                field.distance[index(
                        x,
                        y,
                        z,
                        field.resolution
                    )] = minimum_distance;
            }
        }
    }

    return field;
}

float sampleDistanceField (
                const MeshDistanceField& field,
                const Math::Vec3& position
        ) 
{
    if (field.resolution <= 1
            || field.distance.empty()) 
    {
        return std::numeric_limits<float>::max();
    }

    const Math::Vec3 extent =
        Math::subtract(field.bounds.maximum, field.bounds.minimum);

    if (extent.x <= EPSILON
            || extent.y <= EPSILON
            || extent.z <= EPSILON) 
    {
        return std::numeric_limits<float>::max();
    }

    const float normalized_x = Math::clamp(
                (position.x - field.bounds.minimum.x) / extent.x,
                0.0f,
                1.0f
            ),
            normalized_y = Math::clamp(
                (position.y - field.bounds.minimum.y) / extent.y,
                0.0f,
                1.0f
            ),
            normalized_z = Math::clamp(
                (position.z - field.bounds.minimum.z) / extent.z,
                0.0f,
                1.0f
            );

    const float grid_x = normalized_x *
                static_cast<float>(field.resolution - 1),
                grid_y = normalized_y *
                static_cast<float>(field.resolution - 1),
                grid_z = normalized_z *
                static_cast<float>(field.resolution - 1);

    const int x0 = static_cast<int>(std::floor(grid_x)),
              y0 = static_cast<int>(std::floor(grid_y)),
              z0 = static_cast<int>(std::floor(grid_z)),
              x1 = std::min(field.resolution - 1, x0 + 1),
              y1 = std::min(field.resolution - 1, y0 + 1),
              z1 = std::min(field.resolution - 1, z0 + 1);

    const float tx = grid_x - static_cast<float>(x0),
                ty = grid_y - static_cast<float>(y0),
                tz = grid_z - static_cast<float>(z0);

    const auto sample = [&] (int x, int y, int z) 
    {
        return field.distance[index(x, y, z, field.resolution)];
    };

    const float c00 = sample(x0, y0, z0) * (1.0f - tx) + sample(x1, y0, z0) * tx,
                c10 = sample(x0, y1, z0) * (1.0f - tx) + sample(x1, y1, z0) * tx,
                c01 = sample(x0, y0, z1) * (1.0f - tx) + sample(x1, y0, z1) * tx,
                c11 = sample(x0, y1, z1) * (1.0f - tx) + sample(x1, y1, z1) * tx,
                c0 = c00 * (1.0f - ty) + c10 * ty,
                c1 = c01 * (1.0f - ty) + c11 * ty;

    return c0 * (1.0f - tz) + c1 * tz;
}

} // namespace Lumen
} // namespace Renderer
