#ifndef CRAPGAME_RENDERER_LUMEN_PARALLEL_ROWS_HPP
#define CRAPGAME_RENDERER_LUMEN_PARALLEL_ROWS_HPP

#include "Renderer/ParallelRows.hpp"

namespace Renderer
{
namespace Lumen
{

template <typename Function>
void parallelRowsDynamic(
            int row_count,
            unsigned hardware_threads,
            Function&& function
    )
{
    Renderer::parallelRowsDynamic(
            row_count,
            hardware_threads,
            function
        );
}

} // namespace Lumen
} // namespace Renderer

#endif
