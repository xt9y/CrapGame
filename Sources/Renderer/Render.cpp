#include "Render.hpp"

#include "Renderer/Gpu/CameraCache.hpp"
#include "Renderer/Gpu/FrameHotPath.hpp"
#include "Renderer/Lighting/Lighting.hpp"
#include "Renderer/Lumen/Radiosity.hpp"
#include "Renderer/Lumen/SceneLighting.hpp"
#include "Renderer/Mesh/Mesh.hpp"
#include "Renderer/Shadows/Shadows.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Renderer 
{
namespace 
{

Math::Vec3 toVec3 (const Ecs::Vec3& value) 
{
    return {value.x, value.y, value.z};
}

std::uint8_t toByte (float value) 
{
    return static_cast<std::uint8_t>(
            Math::saturate(value) * 255.0f + 0.5f
        );
}

struct ActiveLight 
{
    const Ecs::TransformComponent *transform;
    const Ecs::LightComponent *light;
};

} // namespace

bool Rendering::init () 
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    glClearColor(0.055f, 0.070f, 0.105f, 1.0f);

    /* RendererCheck deliberately owns the old deterministic CPU reference
     * pipeline. It must not depend on GPU shader compilation or GL43 passes. */
    if (!test_name_.empty())
    {
        return true;
    }

#if defined(__APPLE__)
    std::fprintf(
            stderr,
            "Interactive GPU renderer requires the OpenGL 4.3 lwcgl path\n"
        );
    return false;
#else
    std::string error;

    if (!presenter_.init(&error))
    {
        std::fprintf(stderr, "GPU presenter initialization failed: %s\n", error.c_str());
        return false;
    }

    if (!gpu_gbuffer_.init(&error))
    {
        std::fprintf(stderr, "GPU GBuffer initialization failed: %s\n", error.c_str());
        presenter_.shutdown();
        return false;
    }

    if (!gpu_direct_lighting_.init(&error))
    {
        std::fprintf(stderr, "GPU direct lighting initialization failed: %s\n", error.c_str());
        gpu_gbuffer_.shutdown();
        presenter_.shutdown();
        return false;
    }

    if (!gpu_lumen_.init(&error))
    {
        std::fprintf(stderr, "GPU Lumen initialization failed: %s\n", error.c_str());
        gpu_direct_lighting_.shutdown();
        gpu_gbuffer_.shutdown();
        presenter_.shutdown();
        return false;
    }

    if (!gpu_profiler_.init())
    {
        std::fprintf(
                stderr,
                "GPU timer queries unavailable; renderer will continue without pass timings\n"
            );
    }

    gpu_profiler_enabled_ = gpu_profiler_.enabled();
    gpu_error_scratch_.clear();
    gpu_lumen_schedule_.reset();
    gpu_lumen_sample_index_ = 0;
    gpu_camera_matrices_valid_ = false;
    gpu_camera_data_valid_ = false;
    gpu_pipeline_enabled_ = true;
    return true;
#endif
}

void Rendering::resize (int width, int height) 
{
    const int new_width = width > 0 ? width : 1,
              new_height = height > 0 ? height : 1;

    const bool gpu_frame_ready =
        test_name_.empty()
        && gpu_pipeline_enabled_
        && gpu_gbuffer_.ready()
        && gpu_direct_lighting_.ready()
        && gpu_lumen_.ready();

    const bool cpu_frame_ready =
        !test_name_.empty()
        && !color_buffer_.empty();

    if (new_width == width_
            && new_height == height_
            && (gpu_frame_ready || cpu_frame_ready))
    {
        return;
    }

    width_ = new_width;
    height_ = new_height;

    if (presenter_.ready())
    {
        std::string error;

        if (!presenter_.resize(width_, height_, &error))
        {
            std::fprintf(stderr, "GPU presenter resize failed: %s\n", error.c_str());
            gpu_pipeline_enabled_ = false;
        }
    }

    if (test_name_.empty())
    {
#if !defined(__APPLE__)
        if (gpu_pipeline_enabled_)
        {
            std::string error;

            if (!gpu_gbuffer_.resize(width_, height_, &error)
                    || !gpu_direct_lighting_.resize(width_, height_, &error)
                    || !gpu_lumen_.resize(width_, height_, &error))
            {
                std::fprintf(stderr, "GPU renderer resize failed: %s\n", error.c_str());
                gpu_pipeline_enabled_ = false;
            }
        }
#endif

        /* New attachments have no valid scene data. Force the next GPU frame
         * to repopulate geometry/direct textures and restart Lumen history. */
        change_tracker_.clear();
        gpu_lumen_schedule_.reset();
        gpu_lumen_sample_index_ = 0;
        gpu_camera_matrices_valid_ = false;

        direct_color_.clear();
        indirect_color_.clear();
        indirect_resolved_.clear();
        reflection_color_.clear();
        frame_color_.clear();
        resolved_color_.clear();
        color_buffer_.clear();
        present_buffer_.clear();
        gi_history_.clear();
        history_.clear();

        glViewport(0, 0, width_, height_);
        return;
    }

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

    if (presenter_.ready())
    {
        present_buffer_.clear();
    }
    else
    {
        present_buffer_.resize(pixel_count * 3u);
    }

    gbuffer_.resize(width_, height_);
    history_.resize(width_, height_);
    gi_history_.resize(width_, height_);

    glViewport(0, 0, width_, height_);
}

void Rendering::applyCamera (
                const Ecs::TransformComponent& transform,
                const Ecs::CameraComponent& camera
        ) 
{
    const float aspect = 
        static_cast<float>(width_) /
        static_cast<float>(height_);

    const Math::Vec3 position = toVec3(transform.position);
    const Math::Vec3 rotation = toVec3(transform.rotation);

    const float pitch = Math::radians(rotation.x),
                yaw   = Math::radians(rotation.y);

    const Math::Vec3 forward = 
        Math::normalize({
            std::cos(pitch) * std::sin(yaw),
            std::sin(pitch),
            -std::cos(pitch) * std::cos(yaw),
        });

    projection_ = Math::perspective(
            camera.fov_degrees,
            aspect,
            camera.near_plane,
            camera.far_plane
        );

    view_ = Math::lookAt(
            position,
            Math::add(position, forward),
            {0.0f, 1.0f, 0.0f}
        );
}

bool Rendering::renderGpuFrame (
                const Ecs::World& world,
                const Math::Vec3& camera_position,
                const Lumen::ChangeSet& changes,
                std::uint64_t frame_time_ns
        )
{
#if defined(__APPLE__)
    (void)world;
    (void)camera_position;
    (void)changes;
    (void)frame_time_ns;
    return false;
#else
    if (!gpu_pipeline_enabled_)
    {
        if (!gpu_error_reported_)
        {
            std::fprintf(
                    stderr,
                    "GPU frame pipeline is unavailable; refusing the old minute-per-frame interactive path\n"
                );
            gpu_error_reported_ = true;
        }
        return false;
    }

    const bool scene_geometry_dirty =
        changes.geometry_changed
        || changes.material_changed;

    const bool geometry_dirty =
        scene_geometry_dirty
        || changes.camera_changed;

    const bool lighting_scene_dirty =
        scene_geometry_dirty
        || changes.lighting_changed;

    const bool direct_dirty =
        geometry_dirty
        || changes.lighting_changed;

    const bool lumen_due = gpu_lumen_schedule_.due(
            frame_time_ns,
            direct_dirty
        );

    const bool profile_frame =
        Gpu::profilerCallChainRequired(gpu_profiler_enabled_);
    std::string& error = gpu_error_scratch_;
    error.clear();

    if (profile_frame)
    {
        gpu_profiler_.beginFrame(gpu_frame_index_);
    }

    if (scene_geometry_dirty)
    {
        if (!gpu_gbuffer_.updateScene(world, &error))
        {
            if (profile_frame)
            {
                gpu_profiler_.endFrame();
            }
            if (!gpu_error_reported_)
            {
                std::fprintf(stderr, "GPU GBuffer scene update failed: %s\n", error.c_str());
                gpu_error_reported_ = true;
            }
            return false;
        }
    }

    if (geometry_dirty)
    {
        if (profile_frame)
        {
            gpu_profiler_.begin(Gpu::Profiler::Pass::Geometry);
        }
        const bool geometry_ok = gpu_gbuffer_.draw(
                view_,
                projection_,
                &error
            );
        if (profile_frame)
        {
            gpu_profiler_.end(Gpu::Profiler::Pass::Geometry);
        }

        if (!geometry_ok)
        {
            if (profile_frame)
            {
                gpu_profiler_.endFrame();
            }
            if (!gpu_error_reported_)
            {
                std::fprintf(stderr, "GPU GBuffer draw failed: %s\n", error.c_str());
                gpu_error_reported_ = true;
            }
            return false;
        }
    }

    if (lighting_scene_dirty)
    {
        if (!gpu_direct_lighting_.updateScene(world, &error))
        {
            if (profile_frame)
            {
                gpu_profiler_.endFrame();
            }
            if (!gpu_error_reported_)
            {
                std::fprintf(stderr, "GPU lighting scene update failed: %s\n", error.c_str());
                gpu_error_reported_ = true;
            }
            return false;
        }
    }

    if (direct_dirty)
    {
        if (profile_frame)
        {
            gpu_profiler_.begin(Gpu::Profiler::Pass::DirectLighting);
        }
        const bool direct_ok = gpu_direct_lighting_.dispatch(
                gpu_gbuffer_,
                camera_position,
                &error
            );
        if (profile_frame)
        {
            gpu_profiler_.end(Gpu::Profiler::Pass::DirectLighting);
        }

        if (!direct_ok)
        {
            if (profile_frame)
            {
                gpu_profiler_.endFrame();
            }
            if (!gpu_error_reported_)
            {
                std::fprintf(stderr, "GPU direct-light dispatch failed: %s\n", error.c_str());
                gpu_error_reported_ = true;
            }
            return false;
        }
    }

    if (lumen_due)
    {
        if (profile_frame)
        {
            gpu_profiler_.begin(Gpu::Profiler::Pass::LumenTrace);
        }
        const bool trace_ok = gpu_lumen_.traceShared(
                gpu_gbuffer_,
                gpu_direct_lighting_,
                view_,
                projection_,
                camera_position,
                gpu_lumen_sample_index_,
                &error
            );
        if (profile_frame)
        {
            gpu_profiler_.end(Gpu::Profiler::Pass::LumenTrace);
        }

        if (!trace_ok)
        {
            if (profile_frame)
            {
                gpu_profiler_.endFrame();
            }
            if (!gpu_error_reported_)
            {
                std::fprintf(stderr, "GPU Lumen trace failed: %s\n", error.c_str());
                gpu_error_reported_ = true;
            }
            return false;
        }

        ++gpu_lumen_sample_index_;

        if (profile_frame)
        {
            gpu_profiler_.begin(Gpu::Profiler::Pass::LumenComposite);
        }
        const bool composite_ok = gpu_lumen_.composite(
                gpu_gbuffer_,
                gpu_direct_lighting_,
                &error
            );
        if (profile_frame)
        {
            gpu_profiler_.end(Gpu::Profiler::Pass::LumenComposite);
        }

        if (!composite_ok)
        {
            if (profile_frame)
            {
                gpu_profiler_.endFrame();
            }
            if (!gpu_error_reported_)
            {
                std::fprintf(stderr, "GPU Lumen composite failed: %s\n", error.c_str());
                gpu_error_reported_ = true;
            }
            return false;
        }

        /* Schedule from the frame-start timestamp so a stall never creates
         * catch-up work and the render hot path needs no second clock read. */
        gpu_lumen_schedule_.markUpdated(frame_time_ns);
    }

    if (Gpu::gpuWorkInvalidatesPresenter(
            geometry_dirty,
            direct_dirty,
            lumen_due))
    {
        presenter_.invalidateGpuState();
    }

    if (profile_frame)
    {
        gpu_profiler_.begin(Gpu::Profiler::Pass::Present);
    }
    const bool present_ok = presenter_.presentTexture(
            gpu_lumen_.finalTexture(),
            &error
        );
    if (profile_frame)
    {
        gpu_profiler_.end(Gpu::Profiler::Pass::Present);
        gpu_profiler_.endFrame();
    }

    if (!present_ok)
    {
        if (!gpu_error_reported_)
        {
            std::fprintf(stderr, "GPU presentation failed: %s\n", error.c_str());
            gpu_error_reported_ = true;
        }
        return false;
    }

    if (profile_frame)
    {
        gpu_profiler_.printIfDue(gpu_frame_index_);
    }
    ++gpu_frame_index_;
    gpu_error_reported_ = false;
    return true;
#endif
}

void Rendering::renderGeometry (const Ecs::World& world) 
{
    gbuffer_.clear();

    for (const Ecs::Entity entity : world.entities()) 
    {
        const Ecs::TransformComponent *transform = world.getTransform(entity);
        const Ecs::MeshComponent *mesh = world.getMesh(entity);
        const Ecs::RenderableComponent *renderable = world.getRenderable(entity);
        const Ecs::MaterialComponent *material = world.getMaterial(entity);

        if (!transform 
                || !mesh 
                || !renderable 
                || !renderable->visible 
                || !material) 
        {
            continue;
        }

        gbuffer_.rasterize(
                entity,
                Mesh::meshForType(mesh->mesh),
                *transform,
                *material,
                view_,
                projection_
            );
    }
}

void Rendering::composeLighting (
                const Ecs::World& world,
                const Math::Vec3& camera_position
        ) 
{
    const Math::Vec3 clear_color = {0.055f, 0.070f, 0.105f};
    std::vector<ActiveLight> lights;

    for (const Ecs::Entity entity : world.entities()) 
    {
        const Ecs::TransformComponent *transform = world.getTransform(entity);
        const Ecs::LightComponent *light = world.getLight(entity);

        if (!transform || !light) 
        {
            continue;
        }

        lights.push_back({transform, light});
    }

    for (int y = 0; y < height_; ++y) 
    {
        for (int x = 0; x < width_; ++x) 
        {
            const GBuffer::Pixel& pixel = gbuffer_.pixel(x, y);
            Math::Vec3 color = clear_color;

            if (pixel.valid) 
            {
                color = pixel.emissive;

                for (const ActiveLight& active_light : lights) 
                {
                    const Lighting::LightSample light_sample = 
                        Lighting::sampleLight(
                                *active_light.light,
                                *active_light.transform,
                                pixel.world_position
                            );

                    float visibility = 1.0f;

                    if (active_light.light->casts_shadows
                            && light_sample.valid) 
                    {
                        visibility = shadows_.visibility(
                                pixel.world_position,
                                pixel.normal,
                                light_sample
                            );
                    }

                    color = Math::add(
                            color,
                            Lighting::evaluateDirect(
                                    pixel,
                                    camera_position,
                                    light_sample,
                                    visibility
                                )
                        );
                }
            }

            direct_color_[
                static_cast<std::size_t>(y) *
                static_cast<std::size_t>(width_) +
                static_cast<std::size_t>(x)
            ] = color;
        }
    }
}

void Rendering::composeFinal () 
{
    const std::size_t pixel_count =
        static_cast<std::size_t>(width_) *
        static_cast<std::size_t>(height_);

    for (std::size_t index = 0; index < pixel_count; ++index) 
    {
        const int x = static_cast<int>(index % static_cast<std::size_t>(width_)),
                  y = static_cast<int>(index / static_cast<std::size_t>(width_));

        Math::Vec3 color = direct_color_[index];

        if (gbuffer_.pixel(x, y).valid) 
        {
            color = Lighting::toneMap(
                    Math::add(
                            Math::add(
                                    direct_color_[index],
                                    indirect_resolved_[index]
                                ),
                            reflection_color_[index]
                        )
                );
        }

        frame_color_[index] = color;
    }
}

void Rendering::writeColorBuffer (const std::vector<Math::Vec3>& color) 
{
    const std::size_t pixel_count =
        static_cast<std::size_t>(width_) *
        static_cast<std::size_t>(height_);

    if (color.size() != pixel_count) 
    {
        return;
    }

    for (std::size_t index = 0; index < pixel_count; ++index) 
    {
        const std::size_t offset = index * 3u;

        color_buffer_[offset + 0u] = toByte(color[index].x);
        color_buffer_[offset + 1u] = toByte(color[index].y);
        color_buffer_[offset + 2u] = toByte(color[index].z);
    }
}

void Rendering::present () 
{
    if (presenter_.ready())
    {
        std::string error;

        if (presenter_.present(color_buffer_, &error))
        {
            return;
        }

        std::fprintf(
                stderr,
                "GPU presenter failed; using GL11 fallback for RendererCheck: %s\n",
                error.c_str()
            );
        presenter_.shutdown();

        const std::size_t pixel_count =
            static_cast<std::size_t>(width_) *
            static_cast<std::size_t>(height_);
        present_buffer_.resize(pixel_count * 3u);
    }

    const std::size_t row_bytes = 
        static_cast<std::size_t>(width_) * 3u;

    for (int y = 0; y < height_; ++y) 
    {
        const std::size_t source = 
            static_cast<std::size_t>(height_ - 1 - y) * row_bytes;

        const std::size_t destination = 
            static_cast<std::size_t>(y) * row_bytes;

        std::copy_n(
                color_buffer_.data() + source,
                row_bytes,
                present_buffer_.data() + destination
            );
    }

    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRasterPos2f(-1.0f, -1.0f);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glDrawPixels(
            width_, height_,
            GL_RGB, GL_UNSIGNED_BYTE,
            present_buffer_.data()
        );
}

void Rendering::render (
                const Ecs::World& world,
                std::uint64_t frame_time_ns
        )
{
    const Ecs::Entity camera_entity = world.activeCamera();

    if (camera_entity == Ecs::INVALID_ENTITY) 
    {
        return;
    }

    const Ecs::TransformComponent *camera_transform = world.getTransform(camera_entity);
    const Ecs::CameraComponent *camera = world.getCamera(camera_entity);

    if (!camera_transform || !camera) 
    {
        return;
    }

    const Math::Vec3 camera_position = toVec3(camera_transform->position);

    if (test_name_.empty())
    {
        const Lumen::ChangeSet changes = change_tracker_.update(world);

        if (Gpu::shouldUpdateCameraMatrices(
                gpu_camera_matrices_valid_,
                changes.camera_changed,
                false))
        {
            applyCamera(*camera_transform, *camera);
            gpu_camera_matrices_valid_ = true;
        }

        renderGpuFrame(
                world,
                camera_position,
                changes,
                frame_time_ns
            );
        return;
    }

    /* RendererCheck's deterministic CPU reference path intentionally keeps
     * rebuilding camera matrices per captured frame. */
    applyCamera(*camera_transform, *camera);

    renderGeometry(world);

    Temporal::calculateMotion(
            &gbuffer_,
            world,
            frame_state_
        );

    const Lumen::ChangeSet changes = change_tracker_.update(world);

    if (changes.geometry_changed
            || changes.camera_changed) 
    {
        tracer_.build(world, camera_position);
    }

    if (changes.geometry_changed) 
    {
        cards_.build(world);
        shadows_.build(world);
    }

    if (changes.geometry_changed
            || changes.material_changed) 
    {
        surface_cache_.build(world, cards_);
    }

    if (changes.geometry_changed
            || changes.material_changed
            || changes.lighting_changed) 
    {
        Lumen::updateSceneLighting(
                &surface_cache_,
                world,
                shadows_
            );
    }

    const Lumen::FrameBudget frame_budget =
        Lumen::budgetForFrame(
                width_,
                height_,
                frame_state_.frameIndex()
            );

    Lumen::updateRadiosity(
            &surface_cache_,
            radiance_cache_,
            frame_budget.radiosity_feedback
        );

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

    composeLighting(world, camera_position);

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
            &indirect_color_
        );

    Temporal::resolveTaa(
            gbuffer_,
            gi_history_,
            indirect_color_,
            &indirect_resolved_
        );

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
    present();

    gi_history_.store(gbuffer_, indirect_resolved_);
    history_.store(gbuffer_, resolved_color_);
    frame_state_.capture(world, view_, projection_);
}

void Rendering::shutdown () 
{
    gpu_profiler_.shutdown();
    gpu_lumen_.shutdown();
    gpu_direct_lighting_.shutdown();
    gpu_gbuffer_.shutdown();
    presenter_.shutdown();
    gpu_pipeline_enabled_ = false;
    gpu_error_reported_ = false;
    gpu_camera_matrices_valid_ = false;
    gpu_camera_data_valid_ = false;
    gpu_profiler_enabled_ = false;
    gpu_frame_index_ = 0;
    gpu_lumen_sample_index_ = 0;
    gpu_lumen_schedule_.reset();
    gpu_error_scratch_.clear();

    direct_color_.clear();
    indirect_color_.clear();
    indirect_resolved_.clear();
    reflection_color_.clear();
    frame_color_.clear();
    resolved_color_.clear();
    color_buffer_.clear();
    present_buffer_.clear();
    gi_history_.clear();
    history_.clear();
    change_tracker_.clear();
}

} // namespace Renderer
