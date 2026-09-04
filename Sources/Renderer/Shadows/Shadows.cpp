#include "Shadows.hpp"

#include "Renderer/Mesh/Mesh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Renderer 
{
namespace Shadows 
{
namespace 
{

constexpr float EPSILON = 0.00001f;
constexpr float SHADOW_BIAS = 0.003f;

Math::Vec3 toVec3 (const Ecs::Vec3& value) 
{
    return {value.x, value.y, value.z};
}

} // namespace

bool intersectTriangle (
                const Math::Vec3& origin,
                const Math::Vec3& direction,
                const Triangle& triangle,
                float maximum_distance,
                float *distance
        ) 
{
    const Math::Vec3 edge_a = Math::subtract(triangle.b, triangle.a);
    const Math::Vec3 edge_b = Math::subtract(triangle.c, triangle.a);
    const Math::Vec3 p = Math::cross(direction, edge_b);
    const float determinant = Math::dot(edge_a, p);

    if (std::fabs(determinant) <= EPSILON) return false;

    const float inverse = 1.0f / determinant;
    const Math::Vec3 t = Math::subtract(origin, triangle.a);
    const float u = Math::dot(t, p) * inverse;
    if (u < 0.0f || u > 1.0f) return false;

    const Math::Vec3 q = Math::cross(t, edge_a);
    const float v = Math::dot(direction, q) * inverse;
    if (v < 0.0f || u + v > 1.0f) return false;

    const float hit_distance = Math::dot(edge_b, q) * inverse;
    if (hit_distance <= SHADOW_BIAS || hit_distance >= maximum_distance) return false;

    if (distance) *distance = hit_distance;
    return true;
}

void Scene::build (const Ecs::World& world) 
{
    triangles_.clear();

    for (const Ecs::Entity entity : world.entities()) 
    {
        const Ecs::TransformComponent *transform = world.getTransform(entity);
        const Ecs::MeshComponent *mesh = world.getMesh(entity);
        const Ecs::RenderableComponent *renderable = world.getRenderable(entity);

        if (!transform || !mesh || !renderable || !renderable->visible) continue;

        const Mesh::MeshData& mesh_data = Mesh::meshForComponent(*mesh);
        const Math::Mat4 model = Math::transform(
                toVec3(transform->position),
                toVec3(transform->rotation),
                toVec3(transform->scale)
            );

        for (std::size_t index = 0; index + 2u < mesh_data.indices.size(); index += 3u) 
        {
            const Mesh::Vertex& vertex_a = mesh_data.vertices[mesh_data.indices[index + 0u]];
            const Mesh::Vertex& vertex_b = mesh_data.vertices[mesh_data.indices[index + 1u]];
            const Mesh::Vertex& vertex_c = mesh_data.vertices[mesh_data.indices[index + 2u]];

            triangles_.push_back({
                Math::transformPoint(model, vertex_a.position),
                Math::transformPoint(model, vertex_b.position),
                Math::transformPoint(model, vertex_c.position),
                entity,
            });
        }
    }
}

float Scene::visibility (
                const Math::Vec3& world_position,
                const Math::Vec3& normal,
                const Lighting::LightSample& light_sample
        ) const 
{
    if (!light_sample.valid) return 1.0f;

    const Math::Vec3 ray_direction = Math::normalize(light_sample.direction);
    const Math::Vec3 ray_origin = Math::add(
            world_position,
            Math::multiply(Math::normalize(normal), SHADOW_BIAS * 2.0f)
        );

    float maximum_distance = light_sample.distance;
    if (!std::isfinite(maximum_distance)) maximum_distance = 10000.0f;
    else maximum_distance = std::max(SHADOW_BIAS, maximum_distance - SHADOW_BIAS * 2.0f);

    for (const Triangle& triangle : triangles_) 
    {
        if (intersectTriangle(ray_origin, ray_direction, triangle, maximum_distance, nullptr)) return 0.0f;
    }
    return 1.0f;
}

std::size_t Scene::triangleCount () const 
{
    return triangles_.size();
}

} // namespace Shadows
} // namespace Renderer
