#ifndef CRAPGAME_RENDERER_GPU_TRIANGLESCENE_HPP
#define CRAPGAME_RENDERER_GPU_TRIANGLESCENE_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Gpu/Bvh.hpp"
#include "Renderer/Gpu/TraceMaterialGpu.hpp"
#include "Renderer/Math/Math.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Renderer { namespace Gpu {

struct TriangleGpu
{
    float p0[4] = {};
    float p1[4] = {};
    float p2[4] = {};
    float uv0_uv1[4] = {};
    float uv2_material[4] = {};
    float n0[4] = {};
    float n1[4] = {};
    float n2[4] = {};
};
static_assert(sizeof(TriangleGpu) == 128u,
              "TriangleGpu must match std430 layout");

struct TriangleMeshGpu
{
    std::uint32_t triangle_offset = 0u;
    std::uint32_t triangle_count = 0u;
    std::uint32_t node_offset = 0u;
    std::uint32_t node_count = 0u;
};
static_assert(sizeof(TriangleMeshGpu) == 16u,
              "TriangleMeshGpu must match std430 layout");

struct TriangleInstanceGpu
{
    float inverse_model[16] = {};
    std::uint32_t mesh_index = 0u;
    std::uint32_t material_handle = Ecs::INVALID_ASSET_HANDLE;
    std::uint32_t flags = 0u;
    std::uint32_t padding = 0u;
};
static_assert(sizeof(TriangleInstanceGpu) == 80u,
              "TriangleInstanceGpu must match std430 layout");

class TriangleScene
{
public:
    bool buildCpu(const Ecs::World& world, std::string *error = nullptr);
    bool update(const Ecs::World& world, std::string *error = nullptr);
    void shutdown();

    GLuint triangleBuffer() const { return triangle_buffer_; }
    GLuint blasNodeBuffer() const { return blas_node_buffer_; }
    GLuint meshBuffer() const { return mesh_buffer_; }
    GLuint instanceBuffer() const { return instance_buffer_; }
    GLuint tlasNodeBuffer() const { return tlas_node_buffer_; }
    GLuint traceRecordBuffer() const { return trace_materials_.recordBuffer(); }
    GLuint colorAtlas() const { return trace_materials_.colorAtlas(); }
    GLuint dataAtlas() const { return trace_materials_.dataAtlas(); }

    std::size_t triangleCount() const { return triangles_.size(); }
    std::size_t meshCount() const { return meshes_.size(); }
    std::size_t blasNodeCount() const { return blas_nodes_.size(); }
    std::size_t instanceCount() const { return instances_.size(); }
    std::size_t tlasNodeCount() const { return tlas_nodes_.size(); }
    std::size_t traceMaterialCount() const { return trace_materials_.materialCount(); }
    bool ready() const
    {
        return triangle_buffer_ != 0 && blas_node_buffer_ != 0
            && mesh_buffer_ != 0 && instance_buffer_ != 0
            && tlas_node_buffer_ != 0 && traceRecordBuffer() != 0
            && colorAtlas() != 0
            && dataAtlas() != 0;
    }

private:
    struct CachedMesh
    {
        std::uint32_t loaded_mesh = Ecs::INVALID_ASSET_HANDLE;
        std::uint32_t gpu_mesh_index = 0u;
        Math::Vec3 bounds_min = {0.0f, 0.0f, 0.0f};
        Math::Vec3 bounds_max = {0.0f, 0.0f, 0.0f};
    };

    bool ensureMesh(std::uint32_t loaded_mesh, std::uint32_t *gpu_index,
                    std::string *error);
    bool uploadStatic(std::string *error);
    bool uploadDynamic(std::string *error);
    bool uploadBuffer(GLuint *buffer, std::size_t *capacity,
                      const void *data, std::size_t size,
                      GLenum usage, std::string *error);
    void clearMeshCache();

    GLuint triangle_buffer_ = 0;
    GLuint blas_node_buffer_ = 0;
    GLuint mesh_buffer_ = 0;
    GLuint instance_buffer_ = 0;
    GLuint tlas_node_buffer_ = 0;
    std::size_t triangle_capacity_ = 0u;
    std::size_t blas_capacity_ = 0u;
    std::size_t mesh_capacity_ = 0u;
    std::size_t instance_capacity_ = 0u;
    std::size_t tlas_capacity_ = 0u;

    std::vector<TriangleGpu> triangles_;
    std::vector<BvhNodeGpu> blas_nodes_;
    std::vector<TriangleMeshGpu> meshes_;
    std::vector<CachedMesh> cached_meshes_;
    std::vector<TriangleInstanceGpu> instances_;
    std::vector<BvhBoundsInput> instance_bounds_;
    std::vector<BvhNodeGpu> tlas_nodes_;
    TraceMaterialGpu trace_materials_;
    std::uint64_t mesh_revision_ = 0u;
    bool static_dirty_ = false;
};

} }
#endif
