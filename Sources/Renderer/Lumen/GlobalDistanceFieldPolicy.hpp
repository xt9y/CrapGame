#ifndef CRAPGAME_RENDERER_LUMEN_GLOBAL_DISTANCE_FIELD_POLICY_HPP
#define CRAPGAME_RENDERER_LUMEN_GLOBAL_DISTANCE_FIELD_POLICY_HPP

namespace Renderer
{
namespace Lumen
{

inline float gdfClampExact(float value, float minimum, float maximum)
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
