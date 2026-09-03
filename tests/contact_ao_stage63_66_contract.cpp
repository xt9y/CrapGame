#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Lumen/SphereTrace.hpp"

#include <type_traits>
#include <utility>

int main()
{
    using namespace Renderer;

    static_assert(std::is_same_v<
        decltype(std::declval<const GBuffer::Buffer&>().data()),
        const GBuffer::Pixel *
    >);
    static_assert(std::is_same_v<
        decltype(std::declval<GBuffer::Buffer&>().data()),
        GBuffer::Pixel *
    >);
    static_assert(std::is_member_function_pointer_v<
        decltype(&Lumen::DistanceFieldScene::traceNormalized)
    >);
    static_assert(std::is_member_function_pointer_v<
        decltype(&Lumen::DistanceFieldScene::traceDistanceNormalized)
    >);

    return 0;
}
