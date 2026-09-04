#include "Renderer/Gpu/TriangleScene.hpp"

#include "Renderer/Material/Material.hpp"
#include "Renderer/Mesh/Mesh.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace Renderer { namespace Gpu { namespace {

void setError(std::string *error, const char *message)
{
    if (error) *error = message ? message : "triangle scene error";
}

void deleteBuffer(GLuint *buffer)
{
    if (buffer && *buffer != 0)
    {
        GL15.glDeleteBuffers(1, buffer);
        *buffer = 0;
    }
}

Math::Mat4 inverseTransform(const Ecs::TransformComponent& transform)
{
    const auto inverse = [](float value)
    {
        if (std::fabs(value) > 1.0e-8f) return 1.0f / value;
        return value < 0.0f ? -100000000.0f : 100000000.0f;
    };
    const Math::Vec3 position = {
        transform.position.x, transform.position.y, transform.position.z};
    const Math::Vec3 rotation = {
        transform.rotation.x, transform.rotation.y, transform.rotation.z};
    const Math::Vec3 scale = {
        transform.scale.x, transform.scale.y, transform.scale.z};
    return Math::multiply(
        Math::scaling({inverse(scale.x), inverse(scale.y), inverse(scale.z)}),
        Math::multiply(
            Math::rotationZ(-rotation.z),
            Math::multiply(
                Math::rotationX(-rotation.x),
                Math::multiply(
                    Math::rotationY(-rotation.y),
                    Math::translation({-position.x, -position.y, -position.z})
                )
            )
        )
    );
}

BvhBoundsInput worldBounds(const Mesh::Bounds& bounds,
                           const Ecs::TransformComponent& transform,
                           std::uint32_t instance_index)
{
    const Math::Mat4 model = Math::transform(
        {transform.position.x, transform.position.y, transform.position.z},
        {transform.rotation.x, transform.rotation.y, transform.rotation.z},
        {transform.scale.x, transform.scale.y, transform.scale.z});
    const float maximum = std::numeric_limits<float>::max();
    Math::Vec3 minimum = {maximum, maximum, maximum};
    Math::Vec3 maximum_point = {-maximum, -maximum, -maximum};
    for (int z = 0; z < 2; ++z)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                const Math::Vec3 local = {
                    x ? bounds.maximum.x : bounds.minimum.x,
                    y ? bounds.maximum.y : bounds.minimum.y,
                    z ? bounds.maximum.z : bounds.minimum.z,
                };
                const Math::Vec3 world = Math::transformPoint(model, local);
                minimum.x = std::min(minimum.x, world.x);
                minimum.y = std::min(minimum.y, world.y);
                minimum.z = std::min(minimum.z, world.z);
                maximum_point.x = std::max(maximum_point.x, world.x);
                maximum_point.y = std::max(maximum_point.y, world.y);
                maximum_point.z = std::max(maximum_point.z, world.z);
            }
        }
    }
    BvhBoundsInput output;
    output.minimum[0] = minimum.x;
    output.minimum[1] = minimum.y;
    output.minimum[2] = minimum.z;
    output.maximum[0] = maximum_point.x;
    output.maximum[1] = maximum_point.y;
    output.maximum[2] = maximum_point.z;
    output.primitive_index = instance_index;
    return output;
}

BvhBoundsInput triangleBounds(const Mesh::Vertex& a,
                              const Mesh::Vertex& b,
                              const Mesh::Vertex& c,
                              std::uint32_t local_triangle)
{
    BvhBoundsInput output;
    output.minimum[0] = std::min(a.position.x, std::min(b.position.x, c.position.x));
    output.minimum[1] = std::min(a.position.y, std::min(b.position.y, c.position.y));
    output.minimum[2] = std::min(a.position.z, std::min(b.position.z, c.position.z));
    output.maximum[0] = std::max(a.position.x, std::max(b.position.x, c.position.x));
    output.maximum[1] = std::max(a.position.y, std::max(b.position.y, c.position.y));
    output.maximum[2] = std::max(a.position.z, std::max(b.position.z, c.position.z));
    constexpr float padding = 1.0e-5f;
    for (int axis = 0; axis < 3; ++axis)
    {
        output.minimum[axis] -= padding;
        output.maximum[axis] += padding;
    }
    output.primitive_index = local_triangle;
    return output;
}

} // namespace

bool TriangleScene::uploadBuffer(GLuint *buffer, std::size_t *capacity,
                                 const void *data, std::size_t size,
                                 GLenum usage, std::string *error)
{
    if (!buffer || !capacity)
    {
        setError(error, "invalid triangle scene upload destination");
        return false;
    }
    if (*buffer == 0) GL15.glGenBuffers(1, buffer);
    if (*buffer == 0)
    {
        setError(error, "failed to allocate triangle scene buffer");
        return false;
    }
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, *buffer);
    const std::size_t required = std::max<std::size_t>(16u, size);
    if (required > *capacity)
    {
        std::size_t new_capacity = std::max<std::size_t>(256u, *capacity);
        while (new_capacity < required) new_capacity *= 2u;
        GL15.glBufferData(GL_SHADER_STORAGE_BUFFER,
                          static_cast<LWCGLsizeiptr>(new_capacity),
                          nullptr, usage);
        *capacity = new_capacity;
    }
    if (size != 0)
    {
        GL15.glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                             static_cast<LWCGLsizeiptr>(size), data);
    }
    return true;
}

void TriangleScene::clearMeshCache()
{
    triangles_.clear();
    blas_nodes_.clear();
    meshes_.clear();
    cached_meshes_.clear();
    static_dirty_ = true;
}

bool TriangleScene::ensureMesh(std::uint32_t loaded_mesh,
                               std::uint32_t *gpu_index,
                               std::string *error)
{
    for (const CachedMesh& cached : cached_meshes_)
    {
        if (cached.loaded_mesh == loaded_mesh)
        {
            if (gpu_index) *gpu_index = cached.gpu_mesh_index;
            return true;
        }
    }

    const Mesh::MeshData *mesh = Mesh::loadedMesh(loaded_mesh);
    if (!mesh || mesh->vertices.empty() || mesh->indices.size() < 3u)
    {
        setError(error, "imported triangle mesh is unavailable");
        return false;
    }

    TriangleMeshGpu mesh_gpu;
    mesh_gpu.triangle_offset = static_cast<std::uint32_t>(triangles_.size());
    std::vector<BvhBoundsInput> triangle_bounds;
    triangle_bounds.reserve(mesh->indices.size() / 3u);

    for (std::size_t index = 0; index + 2u < mesh->indices.size(); index += 3u)
    {
        const std::uint32_t ia = mesh->indices[index];
        const std::uint32_t ib = mesh->indices[index + 1u];
        const std::uint32_t ic = mesh->indices[index + 2u];
        if (ia >= mesh->vertices.size() || ib >= mesh->vertices.size()
                || ic >= mesh->vertices.size())
        {
            setError(error, "imported triangle index is out of range");
            return false;
        }
        const Mesh::Vertex& a = mesh->vertices[ia];
        const Mesh::Vertex& b = mesh->vertices[ib];
        const Mesh::Vertex& c = mesh->vertices[ic];
        TriangleGpu triangle;
        triangle.p0[0] = a.position.x;
        triangle.p0[1] = a.position.y;
        triangle.p0[2] = a.position.z;
        triangle.p1[0] = b.position.x;
        triangle.p1[1] = b.position.y;
        triangle.p1[2] = b.position.z;
        triangle.p2[0] = c.position.x;
        triangle.p2[1] = c.position.y;
        triangle.p2[2] = c.position.z;
        triangle.uv0_uv1[0] = a.uv.x;
        triangle.uv0_uv1[1] = a.uv.y;
        triangle.uv0_uv1[2] = b.uv.x;
        triangle.uv0_uv1[3] = b.uv.y;
        triangle.uv2_material[0] = c.uv.x;
        triangle.uv2_material[1] = c.uv.y;
        triangle.n0[0] = a.normal.x;
        triangle.n0[1] = a.normal.y;
        triangle.n0[2] = a.normal.z;
        triangle.n1[0] = b.normal.x;
        triangle.n1[1] = b.normal.y;
        triangle.n1[2] = b.normal.z;
        triangle.n2[0] = c.normal.x;
        triangle.n2[1] = c.normal.y;
        triangle.n2[2] = c.normal.z;
        const std::uint32_t local_triangle =
            static_cast<std::uint32_t>(triangle_bounds.size());
        triangles_.push_back(triangle);
        triangle_bounds.push_back(triangleBounds(a, b, c, local_triangle));
    }

    mesh_gpu.triangle_count = static_cast<std::uint32_t>(triangle_bounds.size());
    mesh_gpu.node_offset = static_cast<std::uint32_t>(blas_nodes_.size());
    BvhBuild build = buildBvh(triangle_bounds, 3u);
    for (BvhNodeGpu& node : build.nodes)
    {
        if (node.meta[3] == 0)
        {
            if (node.meta[0] >= 0)
                node.meta[0] += static_cast<std::int32_t>(mesh_gpu.node_offset);
            if (node.meta[1] >= 0)
                node.meta[1] += static_cast<std::int32_t>(mesh_gpu.node_offset);
        }
    }
    mesh_gpu.node_count = static_cast<std::uint32_t>(build.nodes.size());
    const std::uint32_t index = static_cast<std::uint32_t>(meshes_.size());
    meshes_.push_back(mesh_gpu);
    blas_nodes_.insert(blas_nodes_.end(), build.nodes.begin(), build.nodes.end());
    cached_meshes_.push_back({loaded_mesh, index,
                              mesh->bounds.minimum, mesh->bounds.maximum});
    static_dirty_ = true;
    if (gpu_index) *gpu_index = index;
    if (error) error->clear();
    return true;
}

bool TriangleScene::uploadStatic(std::string *error)
{
    if (!static_dirty_) return true;
    if (!uploadBuffer(&triangle_buffer_, &triangle_capacity_,
                      triangles_.data(), triangles_.size() * sizeof(TriangleGpu),
                      GL_STATIC_DRAW, error)
            || !uploadBuffer(&blas_node_buffer_, &blas_capacity_,
                             blas_nodes_.data(),
                             blas_nodes_.size() * sizeof(BvhNodeGpu),
                             GL_STATIC_DRAW, error)
            || !uploadBuffer(&mesh_buffer_, &mesh_capacity_,
                             meshes_.data(),
                             meshes_.size() * sizeof(TriangleMeshGpu),
                             GL_STATIC_DRAW, error))
    {
        return false;
    }
    static_dirty_ = false;
    return true;
}

bool TriangleScene::uploadDynamic(std::string *error)
{
    return uploadBuffer(&instance_buffer_, &instance_capacity_,
                        instances_.data(),
                        instances_.size() * sizeof(TriangleInstanceGpu),
                        GL_DYNAMIC_DRAW, error)
        && uploadBuffer(&tlas_node_buffer_, &tlas_capacity_,
                        tlas_nodes_.data(),
                        tlas_nodes_.size() * sizeof(BvhNodeGpu),
                        GL_DYNAMIC_DRAW, error);
}

bool TriangleScene::buildCpu(const Ecs::World& world, std::string *error)
{
    const std::uint64_t mesh_revision = Mesh::loadedMeshRevision();
    if (mesh_revision_ != mesh_revision)
    {
        clearMeshCache();
        mesh_revision_ = mesh_revision;
    }

    instances_.clear();
    instance_bounds_.clear();
    for (const Ecs::Entity entity : world.entities())
    {
        const Ecs::TransformComponent *transform = world.getTransform(entity);
        const Ecs::MeshComponent *mesh = world.getMesh(entity);
        const Ecs::RenderableComponent *renderable = world.getRenderable(entity);
        const Ecs::MaterialComponent *material = world.getMaterial(entity);
        if (!transform || !mesh || !renderable || !renderable->visible
                || !material || mesh->loaded_mesh == Ecs::INVALID_ASSET_HANDLE)
        {
            continue;
        }

        std::uint32_t gpu_mesh_index = 0u;
        if (!ensureMesh(mesh->loaded_mesh, &gpu_mesh_index, error)) return false;
        TriangleInstanceGpu instance;
        const Math::Mat4 inverse = inverseTransform(*transform);
        std::memcpy(instance.inverse_model, inverse.value,
                    sizeof(instance.inverse_model));
        instance.mesh_index = gpu_mesh_index;
        instance.material_handle = material->renderer_material;
        const Material::Resource *resource = Material::get(material->renderer_material);
        if (resource)
        {
            if (resource->render_class == Material::RenderClass::Masked)
                instance.flags |= 1u;
            if (resource->render_class == Material::RenderClass::Transparent
                    || resource->render_class == Material::RenderClass::Transmissive)
                instance.flags |= 2u;
        }
        const std::uint32_t instance_index =
            static_cast<std::uint32_t>(instances_.size());
        instances_.push_back(instance);
        const CachedMesh& cached = cached_meshes_[gpu_mesh_index];
        const Mesh::Bounds bounds = {cached.bounds_min, cached.bounds_max};
        instance_bounds_.push_back(
            worldBounds(bounds, *transform, instance_index));
    }

    if (instances_.empty())
    {
        tlas_nodes_.clear();
    }
    else if (tlas_nodes_.empty()
            || !refitBvh(&tlas_nodes_, instance_bounds_))
    {
        tlas_nodes_ = buildBvh(instance_bounds_, 3u).nodes;
    }

    if (error) error->clear();
    return true;
}

bool TriangleScene::update(const Ecs::World& world, std::string *error)
{
    if (!buildCpu(world, error)) return false;
    if (!trace_materials_.rebuild(error)
            || !uploadStatic(error)
            || !uploadDynamic(error))
    {
        return false;
    }
    if (error) error->clear();
    return true;
}

void TriangleScene::shutdown()
{
    deleteBuffer(&triangle_buffer_);
    deleteBuffer(&blas_node_buffer_);
    deleteBuffer(&mesh_buffer_);
    deleteBuffer(&instance_buffer_);
    deleteBuffer(&tlas_node_buffer_);
    triangle_capacity_ = blas_capacity_ = mesh_capacity_ = 0u;
    instance_capacity_ = tlas_capacity_ = 0u;
    triangles_.clear();
    blas_nodes_.clear();
    meshes_.clear();
    cached_meshes_.clear();
    instances_.clear();
    instance_bounds_.clear();
    tlas_nodes_.clear();
    trace_materials_.shutdown();
    mesh_revision_ = 0u;
    static_dirty_ = false;
}

} }
