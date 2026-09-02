#ifndef CRAPGAME_RENDERER_SHADER_HPP
#define CRAPGAME_RENDERER_SHADER_HPP

#include "Renderer/Math/Math.hpp"
#include "Renderer/Mesh/Mesh.hpp"

namespace Renderer 
{
namespace Shader 
{

struct VertexOutput 
{
    Math::Vec3 world_position,
               world_normal;

    Math::Vec2 uv;
    Math::Vec4 clip_position;
};

struct FragmentInput 
{
    Math::Vec3 world_position,
               world_normal;

    Math::Vec2 uv;
};

VertexOutput shadeVertex (
                const Mesh::Vertex& vertex,
                const Math::Mat4& model,
                const Math::Mat4& view,
                const Math::Mat4& projection
        );

FragmentInput interpolate (
                const VertexOutput& a,
                const VertexOutput& b,
                const VertexOutput& c,
                float weight_a,
                float weight_b,
                float weight_c
        );

} // namespace Shader
} // namespace Renderer

#endif
