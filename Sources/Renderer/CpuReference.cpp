#include "Render.hpp"

#include "Renderer/Lumen/Radiosity.hpp"
#include "Renderer/Lumen/SceneLighting.hpp"

#include <chrono>

namespace Renderer
{
namespace
{

using CpuClock = std::chrono::steady_clock;

inline double milliseconds (
            CpuClock::time_point started,
            CpuClock::time_point finished
    )
{
    return std::chrono::duration<double, std::milli>(
            finished - started
        ).count();
}

inline bool lumenPhaseRequired (const CpuReferencePassPlan& plan)
{
    return plan.radiosity
        || plan.radiance_cache
        || plan.screen_probes
        || plan.gi_taa;
}

} // namespace

Rendering::~Rendering ()
{
    if (!cpu_visual_metrics_enabled_)
    {
        return;
    }

    (void)PerformanceMetrics::appendSamples(
            "cpu_render_ms",
            cpu_render_samples_
        );

    /* Compatibility metric: now contains rasterization only, not motion. */
    (void)PerformanceMetrics::appendSamples(
            "cpu_geometry_ms",
            cpu_geometry_samples_
        );
    (void)PerformanceMetrics::appendSamples(
            "cpu_raster_ms",
            cpu_raster_samples_
        );
    (void)PerformanceMetrics::appendSamples(
            "cpu_motion_ms",
            cpu_motion_samples_
        );
    (void)PerformanceMetrics::appendSamples(
            "cpu_scene_ms",
            cpu_scene_samples_
        );
    (void)PerformanceMetrics::appendSamples(
            "cpu_direct_ms",
            cpu_direct_samples_
        );
    (void)PerformanceMetrics::appendSamples(
            "cpu_lumen_ms",
            cpu_lumen_samples_
        );
    (void)PerformanceMetrics::appendSamples(
            "cpu_radiosity_ms",
            cpu_radiosity_samples_
        );
    (void)PerformanceMetrics::appendSamples(
            "cpu_radiance_cache_ms",
            cpu_radiance_cache_samples_
        );
    (void)PerformanceMetrics::appendSamples(
            "cpu_screen_probe_ms",
            cpu_screen_probe_samples_
        );
    (void)PerformanceMetrics::appendSamples(
            "cpu_probe_trace_ms",
            cpu_probe_trace_samples_
        );
    (void)PerformanceMetrics::appendSamples(
            "cpu_probe_reconstruct_ms",
            cpu_probe_reconstruct_samples_
        );
    (void)PerformanceMetrics::appendSamples(
            "cpu_probe_filter_ms",
            cpu_probe_filter_samples_
        );
    (void)PerformanceMetrics::appendSamples(
            "cpu_contact_ao_ms",
            cpu_contact_ao_samples_
        );
    (void)PerformanceMetrics::appendSamples(
            "cpu_gi_taa_ms",
            cpu_gi_taa_samples_
        );
    (void)PerformanceMetrics::appendSamples(
            "cpu_reflection_ms",
            cpu_reflection_samples_
        );
    (void)PerformanceMetrics::appendSamples(
            "cpu_resolve_ms",
            cpu_resolve_samples_
        );
}

void Rendering::renderCpuReference (
            const Ecs::World& world,
            std::uint64_t frame_time_ns
    )
{
    (void)frame_time_ns;

    const Ecs::Entity camera_entity = world.activeCamera();

    if (camera_entity == Ecs::INVALID_ENTITY)
    {
        return;
    }

    const Ecs::TransformComponent *camera_transform =
        world.getTransform(camera_entity);
    const Ecs::CameraComponent *camera =
        world.getCamera(camera_entity);

    if (!camera_transform || !camera)
    {
        return;
    }

    const Math::Vec3 camera_position = {
        camera_transform->position.x,
        camera_transform->position.y,
        camera_transform->position.z,
    };

    applyCamera(*camera_transform, *camera);

    const CpuReferencePassPlan plan = cpuReferencePlan(render_mode_);

    const auto tracking_started = CpuClock::now();
    const Lumen::ChangeSet changes = change_tracker_.update(world);
    const auto tracking_finished = CpuClock::now();
    double scene_ms = milliseconds(tracking_started, tracking_finished);

    const bool geometry_dirty = cpuGeometryNeedsRefresh(
            cpu_geometry_valid_,
            changes.geometry_changed,
            changes.material_changed,
            changes.camera_changed
        );

    double raster_ms = 0.0;

    if (plan.geometry && geometry_dirty)
    {
        const auto raster_started = CpuClock::now();
        renderGeometry(world);
        const auto raster_finished = CpuClock::now();
        raster_ms = milliseconds(raster_started, raster_finished);

        cpu_geometry_valid_ = true;
        cpu_direct_valid_ = false;

        if (cpu_visual_metrics_enabled_)
        {
            cpu_raster_samples_.push_back(raster_ms);
        }
    }

    if (cpu_visual_metrics_enabled_)
    {
        cpu_geometry_samples_.push_back(raster_ms);
    }

    if (plan.motion)
    {
        const bool motion_changed =
            changes.geometry_changed || changes.camera_changed;

        if (cpuMotionNeedsRefresh(
                cpu_motion_valid_,
                changes.geometry_changed,
                changes.camera_changed,
                cpu_motion_settle_pending_))
        {
            const auto motion_started = CpuClock::now();
            Temporal::calculateMotion(
                    &gbuffer_,
                    world,
                    frame_state_
                );
            const auto motion_finished = CpuClock::now();

            if (cpu_visual_metrics_enabled_)
            {
                cpu_motion_samples_.push_back(
                        milliseconds(motion_started, motion_finished)
                    );
            }

            cpu_motion_valid_ = true;
            cpu_motion_settle_pending_ = motion_changed;
        }
    }

    const auto scene_started = CpuClock::now();

    if (plan.tracer
            && (changes.geometry_changed
                || changes.camera_changed))
    {
        tracer_.build(world, camera_position);
    }

    if (plan.cards && changes.geometry_changed)
    {
        cards_.build(world);
    }

    if (plan.shadows && changes.geometry_changed)
    {
        shadows_.build(world);
    }

    if (plan.surface_cache
            && (changes.geometry_changed
                || changes.material_changed))
    {
        surface_cache_.build(world, cards_);
    }

    if (plan.scene_lighting
            && (changes.geometry_changed
                || changes.material_changed
                || changes.lighting_changed))
    {
        Lumen::updateSceneLighting(
                &surface_cache_,
                world,
                shadows_
            );
    }

    const auto scene_finished = CpuClock::now();
    scene_ms += milliseconds(scene_started, scene_finished);

    if (cpu_visual_metrics_enabled_)
    {
        cpu_scene_samples_.push_back(scene_ms);
    }

    if (plan.direct)
    {
        const auto direct_started = CpuClock::now();
        const bool direct_dirty = cpuDirectNeedsRefresh(
                cpu_direct_valid_,
                geometry_dirty,
                changes.lighting_changed
            );

        if (direct_dirty)
        {
            composeLighting(world, camera_position);
            cpu_direct_valid_ = true;
        }

        const auto direct_finished = CpuClock::now();

        if (cpu_visual_metrics_enabled_)
        {
            cpu_direct_samples_.push_back(
                    milliseconds(direct_started, direct_finished)
                );
        }
    }

    if (lumenPhaseRequired(plan))
    {
        const auto lumen_started = CpuClock::now();
        const Lumen::FrameBudget frame_budget =
            Lumen::budgetForFrame(
                    width_,
                    height_,
                    frame_state_.frameIndex()
                );

        if (plan.radiosity)
        {
            const auto pass_started = CpuClock::now();
            Lumen::updateRadiosity(
                    &surface_cache_,
                    radiance_cache_,
                    frame_budget.radiosity_feedback
                );
            const auto pass_finished = CpuClock::now();

            if (cpu_visual_metrics_enabled_)
            {
                cpu_radiosity_samples_.push_back(
                        milliseconds(pass_started, pass_finished)
                    );
            }
        }

        if (plan.radiance_cache)
        {
            const auto pass_started = CpuClock::now();
            radiance_cache_.update(
                    gbuffer_,
                    view_,
                    projection_,
                    tracer_,
                    surface_cache_,
                    camera_position,
                    frame_state_.frameIndex(),
                    frame_budget.radiance_probes_per_frame
                );
            const auto pass_finished = CpuClock::now();

            if (cpu_visual_metrics_enabled_)
            {
                cpu_radiance_cache_samples_.push_back(
                        milliseconds(pass_started, pass_finished)
                    );
            }
        }

        if (plan.screen_probes)
        {
            const auto pass_started = CpuClock::now();
            Lumen::ScreenProbeTimings probe_timings;
            screen_probe_gather_.gather(
                    gbuffer_,
                    view_,
                    projection_,
                    tracer_,
                    surface_cache_,
                    radiance_cache_,
                    frame_state_.frameIndex(),
                    frame_budget.screen_probe_spacing,
                    frame_budget.screen_probe_rays,
                    &indirect_color_,
                    cpu_visual_metrics_enabled_ ? &probe_timings : nullptr
                );
            const auto pass_finished = CpuClock::now();

            if (cpu_visual_metrics_enabled_)
            {
                cpu_screen_probe_samples_.push_back(
                        milliseconds(pass_started, pass_finished)
                    );
                cpu_probe_trace_samples_.push_back(probe_timings.trace_ms);
                cpu_probe_reconstruct_samples_.push_back(
                        probe_timings.reconstruct_ms
                    );
                cpu_probe_filter_samples_.push_back(probe_timings.filter_ms);
                cpu_contact_ao_samples_.push_back(probe_timings.contact_ao_ms);
            }
        }

        if (plan.gi_taa)
        {
            const auto pass_started = CpuClock::now();
            Temporal::resolveTaa(
                    gbuffer_,
                    gi_history_,
                    indirect_color_,
                    &indirect_resolved_
                );
            const auto pass_finished = CpuClock::now();

            if (cpu_visual_metrics_enabled_)
            {
                cpu_gi_taa_samples_.push_back(
                        milliseconds(pass_started, pass_finished)
                    );
            }
        }

        const auto lumen_finished = CpuClock::now();

        if (cpu_visual_metrics_enabled_)
        {
            cpu_lumen_samples_.push_back(
                    milliseconds(lumen_started, lumen_finished)
                );
        }
    }

    if (plan.reflections)
    {
        const auto reflection_started = CpuClock::now();

        reflection_system_.render(
                gbuffer_,
                view_,
                projection_,
                tracer_,
                surface_cache_,
                radiance_cache_,
                camera_position,
                frame_state_.frameIndex(),
                &reflection_color_
            );

        const auto reflection_finished = CpuClock::now();

        if (cpu_visual_metrics_enabled_)
        {
            cpu_reflection_samples_.push_back(
                    milliseconds(reflection_started, reflection_finished)
                );
        }
    }

    const auto resolve_started = CpuClock::now();

    if (render_mode_ == RenderMode::Final)
    {
        composeFinal();
        Temporal::resolveTaa(
                gbuffer_,
                history_,
                frame_color_,
                &resolved_color_
            );
    }
    else
    {
        composeDebug(
                world,
                camera_position
            );

        resolved_color_ = frame_color_;
    }

    writeColorBuffer(resolved_color_);

    if (cpuWindowPresentationRequired(!test_name_.empty()))
    {
        present();
    }

    if (plan.gi_taa)
    {
        gi_history_.store(gbuffer_, indirect_resolved_);
    }

    if (plan.final_taa)
    {
        history_.store(gbuffer_, resolved_color_);
    }

    frame_state_.capture(world, view_, projection_);

    const auto resolve_finished = CpuClock::now();

    if (cpu_visual_metrics_enabled_)
    {
        cpu_resolve_samples_.push_back(
                milliseconds(resolve_started, resolve_finished)
            );
    }
}

} // namespace Renderer
