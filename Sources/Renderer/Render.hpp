#ifndef CRAPGAME_RENDER_HPP
#define CRAPGAME_RENDER_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Gpu/DirectLightingGpu.hpp"
#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Gpu/LumenGpu.hpp"
#include "Renderer/Gpu/Presenter.hpp"
#include "Renderer/Lumen/Budget.hpp"
#include "Renderer/Lumen/Cards.hpp"
#include "Renderer/Lumen/RadianceCache.hpp"
#include "Renderer/Lumen/Reflections.hpp"
#include "Renderer/Lumen/SceneChanges.hpp"
#include "Renderer/Lumen/ScreenProbe.hpp"
#include "Renderer/Lumen/SurfaceCache.hpp"
#include "Renderer/Lumen/Tracer.hpp"
#include "Renderer/Math/Math.hpp"
#include "Renderer/Shadows/Shadows.hpp"
#include "Renderer/Temporal/Temporal.hpp"

#include <lwcgl/lwcgl.h>
#include <rendercheck/capture.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Renderer 
{

class Rendering 
{

public:
    bool init ();
    void resize (int width, int height);
    void render (const Ecs::World& world);
    void shutdown ();

    bool setTestName (const char *test_name);
    int captureFrame (std::uint64_t frame);

private:
    enum class RenderMode
    {
        Final,
        Albedo,
        Normal,
        Depth,
        Material,
        Direct,
        Shadow,
        Motion,
        MeshSdf,
        GlobalSdf,
        Trace,
        SurfaceCache,
        SurfaceLighting,
        RadianceCache,
        ScreenProbes,
        Indirect,
        Ao,
        Reflection,
    };

    void applyCamera (
                const Ecs::TransformComponent& transform,
                const Ecs::CameraComponent& camera
        );

    bool renderGpuFrame (
                const Ecs::World& world,
                const Math::Vec3& camera_position
        );

    void renderGeometry (const Ecs::World& world);

    void composeLighting (
                const Ecs::World& world,
                const Math::Vec3& camera_position
        );

    void composeFinal ();

    void composeDebug (
                const Ecs::World& world,
                const Math::Vec3& camera_position
        );

    void writeColorBuffer (const std::vector<Math::Vec3>& color);
    void present ();

    GBuffer::Buffer gbuffer_;
    Shadows::Scene shadows_;
    Temporal::FrameState frame_state_;
    Temporal::HistoryBuffer history_,
                            gi_history_;
    Lumen::Tracer tracer_;
    Lumen::CardScene cards_;
    Lumen::SurfaceCache surface_cache_;
    Lumen::RadianceCache radiance_cache_;
    Lumen::ChangeTracker change_tracker_;
    Lumen::ReflectionSystem reflection_system_;
    Lumen::ScreenProbeGather screen_probe_gather_;

    Gpu::GBufferGpu gpu_gbuffer_;
    Gpu::DirectLightingGpu gpu_direct_lighting_;
    Gpu::LumenGpu gpu_lumen_;
    Gpu::Presenter presenter_;

    Math::Mat4 view_       = Math::identity(),
               projection_ = Math::identity();

    std::vector<Math::Vec3> direct_color_,
                            indirect_color_,
                            indirect_resolved_,
                            reflection_color_,
                            frame_color_,
                            resolved_color_;

    std::vector<std::uint8_t> color_buffer_,
                              present_buffer_;

    RenderMode render_mode_ = RenderMode::Final;
    std::string test_name_;
    bool gpu_pipeline_enabled_ = false;
    bool gpu_error_reported_ = false;
    std::uint64_t gpu_frame_index_ = 0;

    int width_  = 1,
        height_ = 1;
};

} // namespace Renderer

#endif
