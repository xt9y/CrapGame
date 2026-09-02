#include "GBuffer.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer 
{
namespace GBuffer 
{
namespace 
{

Math::Vec3 toVec3 (const Ecs::Vec3& value) 
{
    return {value.x, value.y, value.z};
}

float edge (
                float ax,
                float ay,
                float bx,
                float by,
                float px,
                float py
        ) 
{
    return (px - ax) * (by - ay) - 
           (py - ay) * (bx - ax);
}

} // namespace

void Buffer::resize (int width, int height) 
{
    width_  = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;

    pixels_.resize(
            static_cast<std::size_t>(width_) * 
            static_cast<std::size_t>(height_)
        );

    clear();
}

void Buffer::clear () 
{
    const Pixel empty = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        0.0f,
        1.0f,
        1.0f,
        Ecs::INVALID_ENTITY,
        false,
    };

    std::fill(pixels_.begin(), pixels_.end(), empty);
}

void Buffer::rasterize (
                Ecs::Entity entity,
                const Mesh::MeshData& mesh,
                const Ecs::TransformComponent& transform,
                const Ecs::MaterialComponent& material,
                const Math::Mat4& view,
                const Math::Mat4& projection
        ) 
{
    const Math::Mat4 model = 
        Math::transform(
                toVec3(transform.position),
                toVec3(transform.rotation),
                toVec3(transform.scale)
            );

    for (std::size_t triangle = 0; 
            triangle + 2u < mesh.indices.size(); 
            triangle += 3u) 
    {
        const Shader::VertexOutput vertex_a = 
            Shader::shadeVertex(
                    mesh.vertices[mesh.indices[triangle + 0u]],
                    model, view, projection
                );

        const Shader::VertexOutput vertex_b = 
            Shader::shadeVertex(
                    mesh.vertices[mesh.indices[triangle + 1u]],
                    model, view, projection
                );

        const Shader::VertexOutput vertex_c = 
            Shader::shadeVertex(
                    mesh.vertices[mesh.indices[triangle + 2u]],
                    model, view, projection
                );

        if (vertex_a.clip_position.w <= 0.00001f 
                || vertex_b.clip_position.w <= 0.00001f 
                || vertex_c.clip_position.w <= 0.00001f) 
        {
            continue;
        }

        const float inverse_a = 1.0f / vertex_a.clip_position.w,
                    inverse_b = 1.0f / vertex_b.clip_position.w,
                    inverse_c = 1.0f / vertex_c.clip_position.w;

        const Math::Vec3 ndc_a = {
            vertex_a.clip_position.x * inverse_a,
            vertex_a.clip_position.y * inverse_a,
            vertex_a.clip_position.z * inverse_a,
        };

        const Math::Vec3 ndc_b = {
            vertex_b.clip_position.x * inverse_b,
            vertex_b.clip_position.y * inverse_b,
            vertex_b.clip_position.z * inverse_b,
        };

        const Math::Vec3 ndc_c = {
            vertex_c.clip_position.x * inverse_c,
            vertex_c.clip_position.y * inverse_c,
            vertex_c.clip_position.z * inverse_c,
        };

        const float ax = (ndc_a.x * 0.5f + 0.5f) * static_cast<float>(width_ - 1),
                    ay = (1.0f - (ndc_a.y * 0.5f + 0.5f)) * static_cast<float>(height_ - 1),
                    bx = (ndc_b.x * 0.5f + 0.5f) * static_cast<float>(width_ - 1),
                    by = (1.0f - (ndc_b.y * 0.5f + 0.5f)) * static_cast<float>(height_ - 1),
                    cx = (ndc_c.x * 0.5f + 0.5f) * static_cast<float>(width_ - 1),
                    cy = (1.0f - (ndc_c.y * 0.5f + 0.5f)) * static_cast<float>(height_ - 1);

        const float area = edge(ax, ay, bx, by, cx, cy);

        if (std::fabs(area) <= 0.00001f) 
        {
            continue;
        }

        const int minimum_x = std::max(
                0,
                static_cast<int>(std::floor(std::min({ax, bx, cx})))
            );

        const int maximum_x = std::min(
                width_ - 1,
                static_cast<int>(std::ceil(std::max({ax, bx, cx})))
            );

        const int minimum_y = std::max(
                0,
                static_cast<int>(std::floor(std::min({ay, by, cy})))
            );

        const int maximum_y = std::min(
                height_ - 1,
                static_cast<int>(std::ceil(std::max({ay, by, cy})))
            );

        for (int y = minimum_y; y <= maximum_y; ++y) 
        {
            for (int x = minimum_x; x <= maximum_x; ++x) 
            {
                const float px = static_cast<float>(x) + 0.5f,
                            py = static_cast<float>(y) + 0.5f;

                const float edge_a = edge(bx, by, cx, cy, px, py),
                            edge_b = edge(cx, cy, ax, ay, px, py),
                            edge_c = edge(ax, ay, bx, by, px, py);

                const bool inside_positive = 
                    edge_a >= 0.0f && edge_b >= 0.0f && edge_c >= 0.0f;

                const bool inside_negative = 
                    edge_a <= 0.0f && edge_b <= 0.0f && edge_c <= 0.0f;

                if (!inside_positive 
                        && !inside_negative) 
                {
                    continue;
                }

                const float barycentric_a = edge_a / area,
                            barycentric_b = edge_b / area,
                            barycentric_c = edge_c / area;

                const float perspective_sum = 
                    barycentric_a * inverse_a +
                    barycentric_b * inverse_b +
                    barycentric_c * inverse_c;

                if (std::fabs(perspective_sum) <= 0.00001f) 
                {
                    continue;
                }

                const float weight_a = barycentric_a * inverse_a / perspective_sum,
                            weight_b = barycentric_b * inverse_b / perspective_sum,
                            weight_c = barycentric_c * inverse_c / perspective_sum;

                const float ndc_depth = 
                    ndc_a.z * weight_a +
                    ndc_b.z * weight_b +
                    ndc_c.z * weight_c;

                const float depth = ndc_depth * 0.5f + 0.5f;

                if (depth < 0.0f 
                        || depth > 1.0f) 
                {
                    continue;
                }

                Pixel& destination = pixel(x, y);

                if (destination.valid 
                        && depth >= destination.depth) 
                {
                    continue;
                }

                const Shader::FragmentInput fragment = 
                    Shader::interpolate(
                            vertex_a, vertex_b, vertex_c,
                            weight_a, weight_b, weight_c
                        );

                destination.world_position = fragment.world_position;
                destination.normal = fragment.world_normal;
                destination.albedo = toVec3(material.albedo);
                destination.emissive = Math::multiply(
                        toVec3(material.emissive),
                        material.emissive_strength
                    );
                destination.metallic = Math::saturate(material.metallic);
                destination.roughness = Math::clamp(material.roughness, 0.04f, 1.0f);
                destination.depth = depth;
                destination.entity = entity;
                destination.valid = true;
            }
        }
    }
}

int Buffer::width () const 
{
    return width_;
}

int Buffer::height () const 
{
    return height_;
}

std::size_t Buffer::index (int x, int y) const 
{
    return static_cast<std::size_t>(y) * 
           static_cast<std::size_t>(width_) + 
           static_cast<std::size_t>(x);
}

const Pixel& Buffer::pixel (int x, int y) const 
{
    return pixels_[index(x, y)];
}

Pixel& Buffer::pixel (int x, int y) 
{
    return pixels_[index(x, y)];
}

} // namespace GBuffer
} // namespace Renderer
