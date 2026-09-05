#ifndef CRAPGAME_RENDERER_GPU_STATICSHADOWPOLICY_HPP
#define CRAPGAME_RENDERER_GPU_STATICSHADOWPOLICY_HPP

namespace Renderer
{
namespace Gpu
{

struct StaticShadowPolicy
{
    static constexpr int SIZE = 2048;
    static constexpr int PCF_RADIUS = 2;
    static constexpr float PADDING = 0.05f;
};

} // namespace Gpu
} // namespace Renderer

#endif
