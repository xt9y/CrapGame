#include "RadianceCache.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer 
{
namespace Lumen 
{
namespace 
{

constexpr float SPACING = 4.0f;

const Math::Vec3 DIRECTIONS[] = {
    { 1.0f,  0.0f,  0.0f}, {-1.0f,  0.0f,  0.0f},
    { 0.0f,  1.0f,  0.0f}, { 0.0f, -1.0f,  0.0f},
    { 0.0f,  0.0f,  1.0f}, { 0.0f,  0.0f, -1.0f},
    { 1.0f,  1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f},
    { 1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f},
    { 1.0f, -1.0f,  1.0f}, {-1.0f, -1.0f, -1.0f},
};

Math::Vec3 skyRadiance (const Math::Vec3& direction) 
{
    const float upward = std::max(0.0f, direction.y);

    return {
        0.012f + upward * 0.025f,
        0.016f + upward * 0.030f,
        0.024f + upward * 0.040f,
    };
}

} // namespace

void RadianceCache::update (
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Tracer& tracer,
                const SurfaceCache& surface_cache,
                const Math::Vec3& camera_position,
                std::uint64_t frame_index,
                std::size_t maximum_updates
        ) 
{
    const Math::Vec3 snapped_center = {
        std::floor(camera_position.x / SPACING) * SPACING,
        std::floor(camera_position.y / SPACING) * SPACING,
        std::floor(camera_position.z / SPACING) * SPACING,
    };

    if (!initialized_
            || Math::lengthSquared(
                    Math::subtract(snapped_center, grid_center_)
                ) > 0.0001f) 
    {
        resetGrid(camera_position);
    }

    if (probes_.empty()
            || maximum_updates == 0u) 
    {
        return;
    }

    const std::size_t update_count =
        std::min(maximum_updates, probes_.size());

    for (std::size_t update_index = 0;
            update_index < update_count;
            ++update_index) 
    {
        RadianceProbe& probe =
            probes_[update_cursor_ % probes_.size()];

        Math::Vec3 radiance = {0.0f, 0.0f, 0.0f};

        for (const Math::Vec3& raw_direction : DIRECTIONS) 
        {
            const Math::Vec3 direction =
                Math::normalize(raw_direction);

            const UnifiedTraceHit hit = tracer.trace(
                    gbuffer,
                    view,
                    projection,
                    probe.position,
                    direction,
                    28.0f
                );

            Math::Vec3 sample_radiance =
                skyRadiance(direction);

            if (hit.hit) 
            {
                sample_radiance = surface_cache.radiance(
                        hit.entity,
                        hit.position,
                        hit.normal
                    );
            }

            radiance = Math::add(radiance, sample_radiance);
        }

        probe.radiance = Math::multiply(
                radiance,
                1.0f / static_cast<float>(
                    sizeof(DIRECTIONS) / sizeof(DIRECTIONS[0])
                )
            );

        probe.updated_frame = frame_index;
        probe.valid = true;

        update_cursor_ = (update_cursor_ + 1u) % probes_.size();
    }
}

Math::Vec3 RadianceCache::sample (
                const Math::Vec3& position
        ) const 
{
    Math::Vec3 accumulated = {0.0f, 0.0f, 0.0f};
    float total_weight = 0.0f;

    for (const RadianceProbe& probe : probes_) 
    {
        if (!probe.valid) 
        {
            continue;
        }

        const float distance_squared = Math::lengthSquared(
                Math::subtract(probe.position, position)
            );

        const float weight = 1.0f / (0.5f + distance_squared);

        accumulated = Math::add(
                accumulated,
                Math::multiply(probe.radiance, weight)
            );

        total_weight += weight;
    }

    if (total_weight <= 0.00001f) 
    {
        return {0.015f, 0.020f, 0.030f};
    }

    return Math::multiply(accumulated, 1.0f / total_weight);
}

const std::vector<RadianceProbe>& RadianceCache::probes () const 
{
    return probes_;
}

void RadianceCache::resetGrid (
                const Math::Vec3& camera_position
        ) 
{
    grid_center_ = {
        std::floor(camera_position.x / SPACING) * SPACING,
        std::floor(camera_position.y / SPACING) * SPACING,
        std::floor(camera_position.z / SPACING) * SPACING,
    };

    probes_.clear();
    probes_.reserve(75u);

    for (int z = -2; z <= 2; ++z) 
    {
        for (int y = -1; y <= 1; ++y) 
        {
            for (int x = -2; x <= 2; ++x) 
            {
                probes_.push_back({
                    {
                        grid_center_.x + static_cast<float>(x) * SPACING,
                        grid_center_.y + static_cast<float>(y) * SPACING,
                        grid_center_.z + static_cast<float>(z) * SPACING,
                    },
                    {0.0f, 0.0f, 0.0f},
                    0,
                    false,
                });
            }
        }
    }

    update_cursor_ = 0;
    initialized_ = true;
}

} // namespace Lumen
} // namespace Renderer
