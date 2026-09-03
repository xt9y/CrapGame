#ifndef CRAPGAME_RENDERER_LUMEN_PARALLEL_ROWS_HPP
#define CRAPGAME_RENDERER_LUMEN_PARALLEL_ROWS_HPP

#include "Renderer/Lumen/ScreenProbePolicy.hpp"

#include <atomic>
#include <thread>
#include <vector>

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
    if (row_count <= 0)
    {
        return;
    }

    const unsigned workers = screenProbeWorkerCount(
            row_count,
            hardware_threads
        );

    if (workers <= 1u)
    {
        for (int row = 0; row < row_count; ++row)
        {
            function(row);
        }
        return;
    }

    std::atomic<int> next_row{0};
    std::vector<std::thread> threads;
    threads.reserve(workers);

    for (unsigned worker = 0; worker < workers; ++worker)
    {
        threads.emplace_back([&]()
        {
            for (;;)
            {
                const int row = next_row.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );

                if (row >= row_count)
                {
                    break;
                }

                function(row);
            }
        });
    }

    for (std::thread& thread : threads)
    {
        thread.join();
    }
}

} // namespace Lumen
} // namespace Renderer

#endif
