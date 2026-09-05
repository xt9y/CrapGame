#include "Renderer/Gpu/VirtualShadowInvalidationGpu.hpp"

#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/VirtualShadowInvalidationShader.hpp"
#include "Renderer/Gpu/VirtualShadowMapGpu.hpp"
#include "Renderer/Material/Material.hpp"
#include "Renderer/Mesh/Mesh.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer
{
namespace Gpu
{
namespace
{

void setError (std::string *error, const char *message)
{
    if (error) *error = message ? message : "virtual shadow invalidation error";
}

bool sameVector (const Math::Vec3& a, const Math::Vec3& b)
{
    constexpr float EPSILON = 1.0e-5f;
    return std::fabs(a.x - b.x) <= EPSILON
        && std::fabs(a.y - b.y) <= EPSILON
        && std::fabs(a.z - b.z) <= EPSILON;
}

bool sameCaster (
        const VirtualShadowInvalidationGpu::CasterSnapshot& a,
        const VirtualShadowInvalidationGpu::CasterSnapshot& b)
{
    return a.valid == b.valid
        && (!a.valid || (
            a.mesh == b.mesh
            && a.material == b.material
            && sameVector(a.minimum, b.minimum)
            && sameVector(a.maximum, b.maximum)));
}

Math::Vec3 lightForward (const Ecs::TransformComponent& transform)
{
    const Math::Vec3 rotation = {
        transform.rotation.x,
        transform.rotation.y,
        transform.rotation.z,
    };
    return Math::normalize(
            Math::transformDirection(
                    Math::rotationEuler(rotation),
                    {0.0f, 0.0f, -1.0f}
                )
        );
}

void shadowBasis (
        const Math::Vec3& direction,
        Math::Vec3 *right,
        Math::Vec3 *up)
{
    const Math::Vec3 forward = Math::normalize(direction);
    const Math::Vec3 reference = std::fabs(forward.y) > 0.95f
        ? Math::Vec3{0.0f, 0.0f, 1.0f}
        : Math::Vec3{0.0f, 1.0f, 0.0f};
    const Math::Vec3 r = Math::normalize(Math::cross(reference, forward));
    const Math::Vec3 u = Math::normalize(Math::cross(forward, r));
    if (right) *right = r;
    if (up) *up = u;
}

} // namespace

bool VirtualShadowInvalidationGpu::init (std::string *error)
{
    shutdown();

    program_ = createComputeProgram(
            VIRTUAL_SHADOW_INVALIDATION_COMPUTE,
            error
        );
    if (program_ == 0) return false;

    region_count_location_ = GL20.glGetUniformLocation(
            program_, "uInvalidationRegionCount");
    light_index_location_ = GL20.glGetUniformLocation(
            program_, "uInvalidationLightIndex");
    invalidate_light_location_ = GL20.glGetUniformLocation(
            program_, "uInvalidateLight");
    right_location_ = GL20.glGetUniformLocation(program_, "uShadowRight");
    up_location_ = GL20.glGetUniformLocation(program_, "uShadowUp");

    if (region_count_location_ < 0
            || light_index_location_ < 0
            || invalidate_light_location_ < 0
            || right_location_ < 0
            || up_location_ < 0)
    {
        setError(error, "virtual shadow invalidation uniforms are unavailable");
        shutdown();
        return false;
    }

    GL15.glGenBuffers(1, &region_buffer_);
    if (region_buffer_ == 0)
    {
        setError(error, "failed to allocate shadow invalidation buffer");
        shutdown();
        return false;
    }

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, region_buffer_);
    GL15.glBufferData(GL_SHADER_STORAGE_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    region_capacity_ = 32u;
    regions_.reserve(MAX_REGIONS);

    if (error) error->clear();
    return true;
}

VirtualShadowInvalidationGpu::CasterSnapshot
VirtualShadowInvalidationGpu::casterSnapshot (
        const Ecs::World& world,
        Ecs::Entity entity) const
{
    CasterSnapshot result;
    const Ecs::TransformComponent *transform = world.getTransform(entity);
    const Ecs::MeshComponent *mesh = world.getMesh(entity);
    const Ecs::RenderableComponent *renderable = world.getRenderable(entity);
    const Ecs::MaterialComponent *material = world.getMaterial(entity);
    if (!transform || !mesh || !renderable || !renderable->visible || !material
            || mesh->loaded_mesh == Ecs::INVALID_ASSET_HANDLE)
    {
        return result;
    }

    const Material::Resource *resource =
        material->renderer_material != Ecs::INVALID_ASSET_HANDLE
        ? Material::get(material->renderer_material)
        : nullptr;
    if (resource && (resource->render_class == Material::RenderClass::Transparent
            || resource->render_class == Material::RenderClass::Transmissive))
    {
        return result;
    }

    const Mesh::MeshData *source = Mesh::loadedMesh(mesh->loaded_mesh);
    if (!source) return result;

    const Math::Mat4 model = Math::transform(
            {transform->position.x, transform->position.y, transform->position.z},
            {transform->rotation.x, transform->rotation.y, transform->rotation.z},
            {transform->scale.x, transform->scale.y, transform->scale.z}
        );

    const float maximum = 3.402823466e+38F;
    result.minimum = {maximum, maximum, maximum};
    result.maximum = {-maximum, -maximum, -maximum};
    for (int z = 0; z < 2; ++z)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                const Math::Vec3 local = {
                    x ? source->bounds.maximum.x : source->bounds.minimum.x,
                    y ? source->bounds.maximum.y : source->bounds.minimum.y,
                    z ? source->bounds.maximum.z : source->bounds.minimum.z,
                };
                const Math::Vec3 world_position = Math::transformPoint(model, local);
                result.minimum.x = std::min(result.minimum.x, world_position.x);
                result.minimum.y = std::min(result.minimum.y, world_position.y);
                result.minimum.z = std::min(result.minimum.z, world_position.z);
                result.maximum.x = std::max(result.maximum.x, world_position.x);
                result.maximum.y = std::max(result.maximum.y, world_position.y);
                result.maximum.z = std::max(result.maximum.z, world_position.z);
            }
        }
    }

    result.mesh = mesh->loaded_mesh;
    result.material = material->renderer_material;
    result.valid = true;
    return result;
}

VirtualShadowInvalidationGpu::LightSnapshot
VirtualShadowInvalidationGpu::lightSnapshot (const Ecs::World& world) const
{
    LightSnapshot result;
    int active_index = 0;
    for (const Ecs::Entity entity : world.entities())
    {
        const Ecs::TransformComponent *transform = world.getTransform(entity);
        const Ecs::LightComponent *light = world.getLight(entity);
        if (!transform || !light || light->intensity <= 0.0f) continue;

        if (!result.valid
                && light->casts_shadows
                && light->type == Ecs::LightType::Directional)
        {
            result.direction = lightForward(*transform);
            result.entity = entity;
            result.active_index = active_index;
            result.valid = true;
        }
        ++active_index;
    }
    return result;
}

void VirtualShadowInvalidationGpu::appendRegion (
        const CasterSnapshot& snapshot,
        bool *overflow)
{
    if (!snapshot.valid || (overflow && *overflow)) return;
    if (regions_.size() >= MAX_REGIONS)
    {
        if (overflow) *overflow = true;
        return;
    }

    RegionGpu region;
    region.minimum[0] = snapshot.minimum.x;
    region.minimum[1] = snapshot.minimum.y;
    region.minimum[2] = snapshot.minimum.z;
    region.maximum[0] = snapshot.maximum.x;
    region.maximum[1] = snapshot.maximum.y;
    region.maximum[2] = snapshot.maximum.z;
    regions_.push_back(region);
}

bool VirtualShadowInvalidationGpu::uploadRegions (std::string *error)
{
    const std::size_t required = std::max<std::size_t>(
            32u,
            regions_.size() * sizeof(RegionGpu)
        );
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, region_buffer_);
    if (required > region_capacity_)
    {
        std::size_t capacity = std::max<std::size_t>(256u, region_capacity_);
        while (capacity < required) capacity *= 2u;
        GL15.glBufferData(
                GL_SHADER_STORAGE_BUFFER,
                static_cast<LWCGLsizeiptr>(capacity),
                nullptr,
                GL_DYNAMIC_DRAW
            );
        region_capacity_ = capacity;
    }
    if (!regions_.empty())
    {
        GL15.glBufferSubData(
                GL_SHADER_STORAGE_BUFFER,
                0,
                static_cast<LWCGLsizeiptr>(regions_.size() * sizeof(RegionGpu)),
                regions_.data()
            );
    }
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    if (error) error->clear();
    return true;
}

bool VirtualShadowInvalidationGpu::update (
        const Ecs::World& world,
        VirtualShadowMapGpu& virtual_shadow_map,
        std::string *error)
{
    if (!ready() || !virtual_shadow_map.ready())
    {
        setError(error, "virtual shadow invalidation resources are not ready");
        return false;
    }

    const std::uint64_t world_revision = world.changeRevision(),
                        mesh_revision = Mesh::loadedMeshRevision(),
                        material_revision = Material::revision();
    const LightSnapshot current_light = lightSnapshot(world);

    std::size_t entity_capacity = snapshots_.size();
    for (const Ecs::Entity entity : world.entities())
        entity_capacity = std::max(entity_capacity, static_cast<std::size_t>(entity) + 1u);

    std::vector<CasterSnapshot> current(entity_capacity);
    for (const Ecs::Entity entity : world.entities())
        current[entity] = casterSnapshot(world, entity);

    if (!initialized_)
    {
        snapshots_.swap(current);
        light_ = current_light;
        world_revision_ = world_revision;
        mesh_revision_ = mesh_revision;
        material_revision_ = material_revision;
        initialized_ = true;
        if (error) error->clear();
        return true;
    }

    const bool registry_changed =
        mesh_revision != mesh_revision_ || material_revision != material_revision_;
    bool invalidate_light =
        light_.valid != current_light.valid
        || (light_.valid && current_light.valid
            && (light_.entity != current_light.entity
                || light_.active_index != current_light.active_index
                || !sameVector(light_.direction, current_light.direction)));

    regions_.clear();
    bool overflow = false;
    const std::size_t count = std::max(snapshots_.size(), current.size());
    snapshots_.resize(count);
    current.resize(count);

    if (world_revision != world_revision_ || registry_changed)
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            const CasterSnapshot& previous = snapshots_[index];
            const CasterSnapshot& next = current[index];
            if (registry_changed || !sameCaster(previous, next))
            {
                appendRegion(previous, &overflow);
                appendRegion(next, &overflow);
            }
        }
    }

    if (overflow) invalidate_light = true;
    const int invalidation_light = current_light.valid
        ? current_light.active_index
        : light_.active_index;

    if (invalidation_light >= 0 && (invalidate_light || !regions_.empty()))
    {
        if (!uploadRegions(error)) return false;

        const Math::Vec3 basis_direction = current_light.valid
            ? current_light.direction
            : light_.direction;
        Math::Vec3 right,
                   up;
        shadowBasis(basis_direction, &right, &up);

        GL30.glBindBufferBase(
                GL_SHADER_STORAGE_BUFFER,
                0,
                virtual_shadow_map.pageCache().metadataBuffer()
            );
        GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, region_buffer_);
        GL20.glUseProgram(program_);
        GL20.glUniform1i(
                region_count_location_,
                static_cast<GLint>(regions_.size())
            );
        GL20.glUniform1i(light_index_location_, invalidation_light);
        GL20.glUniform1i(invalidate_light_location_, invalidate_light ? 1 : 0);
        GL20.glUniform3f(right_location_, right.x, right.y, right.z);
        GL20.glUniform3f(up_location_, up.x, up.y, up.z);
        GL43.glDispatchCompute(
                static_cast<GLuint>(
                    (VirtualShadowPolicy::MAX_PHYSICAL_PAGES + 63) / 64),
                1,
                1
            );
        GL42.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        GL20.glUseProgram(0);
    }

    snapshots_.swap(current);
    light_ = current_light;
    world_revision_ = world_revision;
    mesh_revision_ = mesh_revision;
    material_revision_ = material_revision;
    if (error) error->clear();
    return true;
}

void VirtualShadowInvalidationGpu::shutdown ()
{
    if (region_buffer_ != 0) GL15.glDeleteBuffers(1, &region_buffer_);
    region_buffer_ = 0;
    destroyProgram(&program_);
    region_count_location_ = -1;
    light_index_location_ = -1;
    invalidate_light_location_ = -1;
    right_location_ = -1;
    up_location_ = -1;
    snapshots_.clear();
    regions_.clear();
    light_ = {};
    region_capacity_ = 0u;
    world_revision_ = 0u;
    mesh_revision_ = 0u;
    material_revision_ = 0u;
    initialized_ = false;
}

} // namespace Gpu
} // namespace Renderer
