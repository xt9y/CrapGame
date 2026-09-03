#ifndef CRAPGAME_RENDERER_PARALLEL_ROWS_HPP
#define CRAPGAME_RENDERER_PARALLEL_ROWS_HPP

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

namespace Renderer
{

inline unsigned parallelWorkerCount(
            int work_items,
            unsigned hardware_threads,
            unsigned maximum_workers = 16u
    )
{
    if (work_items <= 1)
    {
        return 1u;
    }

    const unsigned capped_maximum = std::max(1u, maximum_workers);
    const unsigned available = std::min(
            capped_maximum,
            std::max(1u, hardware_threads)
        );

    return std::min(
            static_cast<unsigned>(work_items),
            available
        );
}

template <typename Function>
void parallelRowsDynamic(
            int row_count,
            unsigned hardware_threads,
            Function&& function,
            unsigned maximum_workers = 16u
    )
{
    if (row_count <= 0)
    {
        return;
    }

    const unsigned workers = parallelWorkerCount(
            row_count,
            hardware_threads,
            maximum_workers
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

} // namespace Renderer

#endif
