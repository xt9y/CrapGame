#ifndef CRAPGAME_RENDERER_GPU_DIRECTLIGHTINGGPU_HPP
#define CRAPGAME_RENDERER_GPU_DIRECTLIGHTINGGPU_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Gpu/Bvh.hpp"
#include "Renderer/Gpu/BvhBench.hpp"
#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Gpu/SmrtShadowGpu.hpp"
#include "Renderer/Gpu/StaticDiffuseLightingGpu.hpp"
#include "Renderer/Gpu/TraceGeometryGpu.hpp"
#include "Renderer/Gpu/TriangleScene.hpp"
#include "Renderer/Gpu/ViewSpecularGpu.hpp"
#include "Renderer/Gpu/VirtualShadowInvalidationGpu.hpp"
#include "Renderer/Gpu/VirtualShadowMapGpu.hpp"
#include "Renderer/Math/Math.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Renderer { namespace Gpu {

class TransparentGpu;

class DirectLightingGpu
{
public:
    DirectLightingGpu() = default;
    ~DirectLightingGpu();
    DirectLightingGpu(const DirectLightingGpu&) = delete;
    DirectLightingGpu& operator=(const DirectLightingGpu&) = delete;

    bool init(std::string *error=nullptr);
    bool resize(int width,int height,std::string *error=nullptr);
    bool updateScene(const Ecs::World& world,std::string *error=nullptr);
    bool prewarm(const Ecs::World& world,std::string *error=nullptr)
    {
        return updateScene(world,error);
    }
    bool bindImportedScene(const TriangleScene& triangles,std::string *error=nullptr);
    bool dispatch(const GBufferGpu& gbuffer,const Math::Vec3& camera_position,
                  std::uint64_t frame_index,std::string *error=nullptr);
    bool dispatch(const GBufferGpu& gbuffer,const Math::Vec3& camera_position,
                  std::string *error=nullptr)
    {
        return dispatch(gbuffer,camera_position,shadow_frame_index_++,error);
    }
    bool render(const Ecs::World& world,const GBufferGpu& gbuffer,
                const Math::Vec3& camera_position,std::uint64_t frame_index,
                std::string *error=nullptr);
    bool render(const Ecs::World& world,const GBufferGpu& gbuffer,
                const Math::Vec3& camera_position,std::string *error=nullptr)
    {
        return render(world,gbuffer,camera_position,shadow_frame_index_++,error);
    }
    void shutdown();
    void releaseAcceleration();

    bool ready() const
    {
        return program_!=0&&combine_program_!=0&&direct_color_!=0&&dynamic_color_!=0
            &&virtual_shadow_invalidation_.ready()
            &&virtual_shadow_map_.ready()&&smrt_shadow_.ready();
    }
    GLuint directTexture() const { return direct_color_; }
    GLuint lightBuffer() const { return light_buffer_; }
    std::size_t lightCount() const { return lights_.size(); }
    GLuint finalTexture() const { return direct_color_; }
    GLuint primitiveBuffer() const { return primitive_buffer_; }
    std::size_t primitiveCount() const { return primitives_.size(); }
    bool bvhReady() const { return use_bvh_ && !bvh_nodes_.empty() && bvh_node_buffer_!=0; }
    bool benchmarkActive() const { return bench_config_initialized_ && bench_config_.stress_primitives>0u; }
    GLuint bvhNodeBuffer() const { return bvh_node_buffer_; }
    std::size_t bvhNodeCount() const { return bvh_nodes_.size(); }
    const TriangleScene& triangleScene() const { return triangle_scene_; }
    const Ecs::World *sceneWorld() const { return scene_world_; }
    std::uint64_t sceneRevision() const { return scene_revision_; }
    TransparentGpu *transparentPass() const { return transparent_.get(); }
    const VirtualShadowMapGpu& virtualShadowMap() const { return virtual_shadow_map_; }
    const SmrtShadowGpu& smrtShadow() const { return smrt_shadow_; }
    const SmrtShadowGpu& staticShadowCache() const { return smrt_shadow_; }
    const StaticDiffuseLightingGpu& staticDiffuse() const { return static_diffuse_; }
    const ViewSpecularGpu& viewSpecular() const { return view_specular_; }
    const TraceGeometryGpu& traceGeometry() const { return trace_geometry_; }

private:
    static constexpr std::size_t BVH_THRESHOLD=8u;
    static constexpr std::size_t BVH_LEAF_SIZE=3u;

    struct LightGpu { float position_type[4]; float direction_range[4]; float color_intensity[4]; float cone_shadow[4]; };
    struct PrimitiveGpu { float position_type[4]; float rotation[4]; float scale[4]; float albedo_metallic[4]; float emissive_roughness[4]; };

    const BvhBenchConfig& benchConfig();
    void appendStressPrimitives(std::size_t count);
    bool ensureBvhBuffer(std::string *error);
    bool uploadBuffer(GLuint buffer,std::size_t *capacity,const void *data,std::size_t size,std::string *error);
    void destroyTextures();

    template <typename T>
    bool uploadChangedRecords(GLuint buffer,std::size_t *capacity,const std::vector<T>& current,std::vector<T> *uploaded,std::string *error);

    GLuint program_=0, combine_program_=0;
    GLuint light_buffer_=0, primitive_buffer_=0, bvh_node_buffer_=0,
           direct_color_=0, dynamic_color_=0;
    GLint camera_location_=-1, light_count_location_=-1, primitive_count_location_=-1;
    GLint static_split_light_index_location_=-1;
    GLint inverse_view_projection_location_=-1;
    std::size_t light_capacity_=0, primitive_capacity_=0, bvh_node_capacity_=0, bvh_primitive_count_=0u;
    std::vector<LightGpu> lights_, uploaded_lights_;
    std::vector<PrimitiveGpu> primitives_, uploaded_primitives_;
    std::vector<BvhBoundsInput> primitive_bounds_;
    std::vector<BvhNodeGpu> bvh_nodes_, uploaded_bvh_nodes_;
    TriangleScene triangle_scene_;
    TraceGeometryGpu trace_geometry_;
    VirtualShadowInvalidationGpu virtual_shadow_invalidation_;
    VirtualShadowMapGpu virtual_shadow_map_;
    SmrtShadowGpu smrt_shadow_;
    StaticDiffuseLightingGpu static_diffuse_;
    ViewSpecularGpu view_specular_;
    mutable std::unique_ptr<TransparentGpu> transparent_;
    const Ecs::World *scene_world_=nullptr;
    std::uint64_t scene_revision_=0u,
                  shadow_frame_index_=0u;
    BvhBenchConfig bench_config_={};
    bool bench_config_initialized_=false, bench_reported_=false, use_bvh_=false;
    int width_=0,height_=0;
};

} }
#endif
