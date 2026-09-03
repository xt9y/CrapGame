#ifndef CRAPGAME_RENDER_HPP
#define CRAPGAME_RENDER_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/CpuReferencePolicy.hpp"
#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Gpu/DirectLightingGpu.hpp"
#include "Renderer/Gpu/FrameHotPath.hpp"
#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Gpu/LumenGpu.hpp"
#include "Renderer/Gpu/LumenSchedule.hpp"
#include "Renderer/Gpu/Presenter.hpp"
#include "Renderer/Gpu/Profiler.hpp"
#include "Renderer/Gpu/RuntimeHotPath.hpp"
#include "Renderer/Gpu/RuntimeHotPathV3.hpp"
#include "Renderer/Lumen/Budget.hpp"
#include "Renderer/Lumen/Cards.hpp"
#include "Renderer/Lumen/RadianceCache.hpp"
#include "Renderer/Lumen/Reflections.hpp"
#include "Renderer/Lumen/SceneChanges.hpp"
#include "Renderer/Lumen/ScreenProbe.hpp"
#include "Renderer/Lumen/SurfaceCache.hpp"
#include "Renderer/Lumen/Tracer.hpp"
#include "Renderer/Math/Math.hpp"
#include "Renderer/PerformanceMetrics.hpp"
#include "Renderer/Shadows/Shadows.hpp"
#include "Renderer/Temporal/Temporal.hpp"

#include <lwcgl/lwcgl.h>
#include <rendercheck/capture.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace Renderer 
{

class Rendering 
{

public:
    ~Rendering ();

    bool init ();
    void resize (int width, int height);
    void render (const Ecs::World& world, std::uint64_t frame_time_ns);

    bool initHeadlessReference () const
    {
        return !test_name_.empty();
    }

    void resizeHeadlessReference (int width, int height)
    {
        width_ = width > 0 ? width : 1;
        height_ = height > 0 ? height : 1;

        const std::size_t pixel_count =
            static_cast<std::size_t>(width_) *
            static_cast<std::size_t>(height_);

        direct_color_.resize(pixel_count);
        indirect_color_.resize(pixel_count);
        indirect_resolved_.resize(pixel_count);
        reflection_color_.resize(pixel_count);
        frame_color_.resize(pixel_count);
        resolved_color_.resize(pixel_count);
        color_buffer_.resize(pixel_count * 3u);
        present_buffer_.clear();

        gbuffer_.resize(width_, height_);
        history_.resize(width_, height_);
        gi_history_.resize(width_, height_);
        change_tracker_.clear();
        cpu_geometry_valid_ = false;
        cpu_direct_valid_ = false;
        cpu_motion_valid_ = false;
        cpu_motion_settle_pending_ = false;
    }

    void renderCached (
                const Ecs::World& world,
                std::uint64_t frame_time_ns
        )
    {
        if (!test_name_.empty())
        {
            const char *metrics_path = std::getenv("RENDERCHECK_METRICS_PATH");
            cpu_visual_metrics_enabled_ = Gpu::visualRunTimingRequired(
                    true,
                    PerformanceMetrics::requested(),
                    metrics_path && *metrics_path
                );

            if (!cpu_visual_metrics_enabled_)
            {
                renderCpuReference(world, frame_time_ns);
                return;
            }

            if (cpu_render_samples_.capacity() == 0u)
            {
                constexpr std::size_t expected_visual_frames = 64u;
                cpu_render_samples_.reserve(expected_visual_frames);
                cpu_geometry_samples_.reserve(expected_visual_frames);
                cpu_raster_samples_.reserve(expected_visual_frames);
                cpu_motion_samples_.reserve(expected_visual_frames);
                cpu_scene_samples_.reserve(expected_visual_frames);
                cpu_direct_samples_.reserve(expected_visual_frames);
                cpu_lumen_samples_.reserve(expected_visual_frames);
                cpu_radiosity_samples_.reserve(expected_visual_frames);
                cpu_radiance_cache_samples_.reserve(expected_visual_frames);
                cpu_screen_probe_samples_.reserve(expected_visual_frames);
                cpu_gi_taa_samples_.reserve(expected_visual_frames);
                cpu_reflection_samples_.reserve(expected_visual_frames);
                cpu_resolve_samples_.reserve(expected_visual_frames);
            }

            const auto render_started = std::chrono::steady_clock::now();
            renderCpuReference(world, frame_time_ns);
            const auto render_finished = std::chrono::steady_clock::now();
            cpu_render_samples_.push_back(
                    std::chrono::duration<double, std::milli>(
                            render_finished - render_started
                        ).count()
                );
            return;
        }

        Lumen::ChangeSet changes;
        const std::uint64_t world_revision = world.changeRevision();
        const bool revision_valid =
            gpu_world_revision_valid_ && gpu_camera_matrices_valid_;

        if (Gpu::rendererRevisionNeedsUpdate(
                revision_valid,
                world_revision,
                gpu_world_revision_))
        {
            changes = change_tracker_.update(world);
            gpu_world_revision_ = world_revision;
            gpu_world_revision_valid_ = true;
        }

        const bool refresh_camera = Gpu::cameraDataNeedsRefresh(
                gpu_camera_data_valid_ && gpu_camera_matrices_valid_,
                changes.camera_changed
            );

        if (refresh_camera)
        {
            const Ecs::Entity camera_entity = world.activeCamera();

            if (camera_entity == Ecs::INVALID_ENTITY)
            {
                gpu_camera_data_valid_ = false;
                return;
            }

            const Ecs::TransformComponent *camera_transform =
                world.getTransform(camera_entity);
            const Ecs::CameraComponent *camera =
                world.getCamera(camera_entity);

            if (!camera_transform || !camera)
            {
                gpu_camera_data_valid_ = false;
                return;
            }

            gpu_camera_position_ = {
                camera_transform->position.x,
                camera_transform->position.y,
                camera_transform->position.z,
            };
            applyCamera(*camera_transform, *camera);
            gpu_camera_data_valid_ = true;
            gpu_camera_matrices_valid_ = true;
        }

        renderGpuFrame(
                world,
                gpu_camera_position_,
                changes,
                frame_time_ns
            );
    }

    void shutdown ();

    bool setTestName (const char *test_name);
    int captureFrame (std::uint64_t frame);

private:
    using RenderMode = CpuReferenceMode;

    void applyCamera (
                const Ecs::TransformComponent& transform,
                const Ecs::CameraComponent& camera
        );

    void renderCpuReference (
                const Ecs::World& world,
                std::uint64_t frame_time_ns
        );

    bool renderGpuFrame (
                const Ecs::World& world,
                const Math::Vec3& camera_position,
                const Lumen::ChangeSet& changes,
                std::uint64_t frame_time_ns
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
    Gpu::LumenSchedule gpu_lumen_schedule_;
    Gpu::Presenter presenter_;
    Gpu::Profiler gpu_profiler_;

    Math::Mat4 view_       = Math::identity(),
               projection_ = Math::identity();
    Math::Vec3 gpu_camera_position_ = {0.0f, 0.0f, 0.0f};

    std::vector<Math::Vec3> direct_color_,
                            indirect_color_,
                            indirect_resolved_,
                            reflection_color_,
                            frame_color_,
                            resolved_color_;

    std::vector<std::uint8_t> color_buffer_,
                              present_buffer_;

    std::vector<double> cpu_render_samples_,
                        cpu_geometry_samples_,
                        cpu_raster_samples_,
                        cpu_motion_samples_,
                        cpu_scene_samples_,
                        cpu_direct_samples_,
                        cpu_lumen_samples_,
                        cpu_radiosity_samples_,
                        cpu_radiance_cache_samples_,
                        cpu_screen_probe_samples_,
                        cpu_gi_taa_samples_,
                        cpu_reflection_samples_,
                        cpu_resolve_samples_;

    RenderMode render_mode_ = RenderMode::Final;
    std::string test_name_;
    std::string gpu_error_scratch_;
    bool cpu_geometry_valid_ = false;
    bool cpu_direct_valid_ = false;
    bool cpu_motion_valid_ = false;
    bool cpu_motion_settle_pending_ = false;
    bool cpu_visual_metrics_enabled_ = false;
    bool gpu_pipeline_enabled_ = false;
    bool gpu_error_reported_ = false;
    bool gpu_camera_matrices_valid_ = false;
    bool gpu_camera_data_valid_ = false;
    bool gpu_profiler_enabled_ = false;
    bool gpu_world_revision_valid_ = false;
    std::uint64_t gpu_world_revision_ = 0u;
    std::uint64_t gpu_frame_index_ = 0;
    std::uint64_t gpu_lumen_sample_index_ = 0;

    int width_  = 1,
        height_ = 1;
};

} // namespace Renderer

#endif