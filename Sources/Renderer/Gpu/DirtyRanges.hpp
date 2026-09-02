#ifndef CRAPGAME_RENDERER_GPU_DIRTYRANGES_HPP
#define CRAPGAME_RENDERER_GPU_DIRTYRANGES_HPP

#include <cstddef>
#include <cstring>
#include <type_traits>
#include <vector>

namespace Renderer
{
namespace Gpu
{

template <typename T, typename Callback>
void forEachDirtyRange (
                const std::vector<T>& current,
                const std::vector<T>& previous,
                Callback&& callback
        )
{
    static_assert(
            std::is_trivially_copyable<T>::value,
            "GPU dirty-range records must be trivially copyable"
        );

    if (current.empty())
    {
        return;
    }

    if (current.size() != previous.size())
    {
        callback(0u, current.size());
        return;
    }

    std::size_t range_begin = current.size();

    for (std::size_t index = 0; index < current.size(); ++index)
    {
        const bool changed =
            std::memcmp(
                    &current[index],
                    &previous[index],
                    sizeof(T)
                ) != 0;

        if (changed)
        {
            if (range_begin == current.size())
            {
                range_begin = index;
            }
            continue;
        }

        if (range_begin != current.size())
        {
            callback(range_begin, index - range_begin);
            range_begin = current.size();
        }
    }

    if (range_begin != current.size())
    {
        callback(range_begin, current.size() - range_begin);
    }
}

} // namespace Gpu
} // namespace Renderer

#endif
