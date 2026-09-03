#include "SurfaceCache.hpp"

#include <cstddef>
#include <limits>

namespace Renderer 
{
namespace Lumen 
{
namespace 
{

Math::Vec3 toVec3 (const Ecs::Vec3& value) 
{
    return {value.x, value.y, value.z};
}

Math::Vec3 previousIndirect (
                const std::vector<SurfaceSample>& previous,
                const Card& card,
                std::size_t begin,
                std::size_t end
        ) 
{
    Math::Vec3 result = {0.0f, 0.0f, 0.0f};
    float best_score = std::numeric_limits<float>::max();

    for (std::size_t index = begin; index < end; ++index) 
    {
        const SurfaceSample& sample = previous[index];

        const float facing = Math::dot(
                sample.card.normal,
                card.normal
            );

        if (facing < 0.75f) 
        {
            continue;
        }

        const float distance = Math::lengthSquared(
                Math::subtract(
                        sample.card.position,
                        card.position
                    )
            );

        const float score = distance + (1.0f - facing) * 0.5f;

        if (score < best_score) 
        {
            best_score = score;
            result = sample.indirect_lighting;
        }
    }

    return result;
}

} // namespace

void SurfaceCache::build (
                const Ecs::World& world,
                const CardScene& cards
        ) 
{
    previous_samples_.swap(samples_);
    samples_.clear();

    if (samples_.capacity() < cards.cards().size())
    {
        samples_.reserve(cards.cards().size());
    }

    std::size_t previous_cursor = 0u,
                previous_begin = 0u,
                previous_end = 0u;
    Ecs::Entity previous_entity = Ecs::INVALID_ENTITY;

    for (const Card& card : cards.cards()) 
    {
        const Ecs::MaterialComponent *material =
            world.getMaterial(card.entity);

        if (!material) 
        {
            continue;
        }

        if (card.entity != previous_entity)
        {
            while (previous_cursor < previous_samples_.size()
                    && previous_samples_[previous_cursor].card.entity < card.entity)
            {
                ++previous_cursor;
            }

            previous_begin = previous_cursor;

            while (previous_cursor < previous_samples_.size()
                    && previous_samples_[previous_cursor].card.entity == card.entity)
            {
                ++previous_cursor;
            }

            previous_end = previous_cursor;
            previous_entity = card.entity;
        }

        samples_.push_back({
            card,
            toVec3(material->albedo),
            {
                material->emissive.x * material->emissive_strength,
                material->emissive.y * material->emissive_strength,
                material->emissive.z * material->emissive_strength,
            },
            {0.0f, 0.0f, 0.0f},
            previousIndirect(
                    previous_samples_,
                    card,
                    previous_begin,
                    previous_end
                ),
            material->metallic,
            material->roughness,
        });
    }
}

const SurfaceSample *SurfaceCache::sample (
                Ecs::Entity entity,
                const Math::Vec3& position,
                const Math::Vec3& normal
        ) const 
{
    const SurfaceSample *best = nullptr;
    float best_score = std::numeric_limits<float>::max();

    for (const SurfaceSample& surface : samples_) 
    {
        if (surface.card.entity != entity) 
        {
            continue;
        }

        const float facing = Math::dot(
                surface.card.normal,
                normal
            );

        if (facing < -0.25f) 
        {
            continue;
        }

        const float distance = Math::lengthSquared(
                Math::subtract(
                        surface.card.position,
                        position
                    )
            );

        const float score = distance + (1.0f - facing) * 0.5f;

        if (score < best_score) 
        {
            best = &surface;
            best_score = score;
        }
    }

    return best;
}

Math::Vec3 SurfaceCache::radiance (
                Ecs::Entity entity,
                const Math::Vec3& position,
                const Math::Vec3& normal
        ) const 
{
    const SurfaceSample *surface =
        sample(entity, position, normal);

    if (!surface) 
    {
        return {0.0f, 0.0f, 0.0f};
    }

    return Math::add(
            surface->emissive,
            Math::add(
                    surface->direct_lighting,
                    surface->indirect_lighting
                )
        );
}

std::vector<SurfaceSample>& SurfaceCache::samples () 
{
    return samples_;
}

const std::vector<SurfaceSample>& SurfaceCache::samples () const 
{
    return samples_;
}

} // namespace Lumen
} // namespace Renderer
