#ifndef CRAPGAME_RENDERER_LUMEN_DISTANCE_FIELD_SAMPLE_POLICY_HPP
#define CRAPGAME_RENDERER_LUMEN_DISTANCE_FIELD_SAMPLE_POLICY_HPP

namespace Renderer
{
namespace Lumen
{

inline float distanceFieldClampExact(
            float value,
            float minimum,
            float maximum
    )
{
    if (value < minimum)
    {
        return minimum;
    }

    if (value > maximum)
    {
        return maximum;
    }

    return value;
}

} // namespace Lumen
} // namespace Renderer

#endif
