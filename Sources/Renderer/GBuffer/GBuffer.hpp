#ifndef CRAPGAME_RENDERER_GBUFFER_HPP
#define CRAPGAME_RENDERER_GBUFFER_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Math/Math.hpp"
#include "Renderer/Mesh/Mesh.hpp"
#include "Renderer/Shader/Shader.hpp"

#include <cstddef>
#include <vector>

namespace Renderer 
{
namespace GBuffer 
{

struct Pixel 
{
    Math::Vec3 world_position,
               normal,
               albedo,
               emissive;

    float metallic,
          roughness,
          depth;

    Ecs::Entity entity;
    bool valid;

    Math::Vec2 motion = {0.0f, 0.0f};
};

class Buffer 
{

public:
    void resize (int width, int height);
    void clear ();

    void rasterize (
                Ecs::Entity entity,
                const Mesh::MeshData& mesh,
                const Ecs::TransformComponent& transform,
                const Ecs::MaterialComponent& material,
                const Math::Mat4& view,
                const Math::Mat4& projection
        );

    int width () const;
    int height () const;

    const Pixel& pixel (int x, int y) const;
    Pixel& pixel (int x, int y);

private:
    std::size_t index (int x, int y) const;

    int width_  = 1,
        height_ = 1;

    std::vector<Pixel> pixels_;
};

} // namespace GBuffer
} // namespace Renderer

#endif
