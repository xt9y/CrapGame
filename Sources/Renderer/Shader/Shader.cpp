#include "Shader.hpp"

namespace Renderer 
{
namespace Shader 
{

VertexOutput shadeVertex (
                const Mesh::Vertex& vertex,
                const Math::Mat4& model,
                const Math::Mat4& view,
                const Math::Mat4& projection
        ) 
{
    const Math::Vec4 local_position = {
        vertex.position.x,
        vertex.position.y,
        vertex.position.z,
        1.0f,
    };

    const Math::Vec4 world_position = 
        Math::transform(model, local_position);

    const Math::Vec4 view_position = 
        Math::transform(view, world_position);

    const Math::Vec4 clip_position = 
        Math::transform(projection, view_position);

    const Math::Vec3 world_normal = 
        Math::normalize(
                Math::transformDirection(model, vertex.normal)
            );

    return {
        {world_position.x, world_position.y, world_position.z},
        world_normal,
        vertex.uv,
        clip_position,
    };
}

FragmentInput interpolate (
                const VertexOutput& a,
                const VertexOutput& b,
                const VertexOutput& c,
                float weight_a,
                float weight_b,
                float weight_c
        ) 
{
    FragmentInput result;

    result.world_position = Math::add(
            Math::add(
                    Math::multiply(a.world_position, weight_a),
                    Math::multiply(b.world_position, weight_b)
                ),
            Math::multiply(c.world_position, weight_c)
        );

    result.world_normal = Math::normalize(
            Math::add(
                    Math::add(
                            Math::multiply(a.world_normal, weight_a),
                            Math::multiply(b.world_normal, weight_b)
                        ),
                    Math::multiply(c.world_normal, weight_c)
                )
        );

    result.uv = {
        a.uv.x * weight_a + b.uv.x * weight_b + c.uv.x * weight_c,
        a.uv.y * weight_a + b.uv.y * weight_b + c.uv.y * weight_c,
    };

    return result;
}

} // namespace Shader
} // namespace Renderer
