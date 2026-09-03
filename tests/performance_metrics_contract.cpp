#include "Renderer/PerformanceMetrics.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

int main ()
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "crapgame-rendercheck-metrics.txt";

    std::filesystem::remove(path);

#if defined(_WIN32)
    _putenv_s("RENDERCHECK_PERF", "1");
    _putenv_s("RENDERCHECK_PERF_WARMUP_MS", "125.5");
    _putenv_s("RENDERCHECK_PERF_DURATION_MS", "725.5");
    _putenv_s("RENDERCHECK_METRICS_PATH", path.string().c_str());
#else
    setenv("RENDERCHECK_PERF", "1", 1);
    setenv("RENDERCHECK_PERF_WARMUP_MS", "125.5", 1);
    setenv("RENDERCHECK_PERF_DURATION_MS", "725.5", 1);
    setenv("RENDERCHECK_METRICS_PATH", path.string().c_str(), 1);
#endif

    assert(Renderer::PerformanceMetrics::requested());
    assert(Renderer::PerformanceMetrics::warmupMilliseconds() == 125.5);
    assert(Renderer::PerformanceMetrics::durationMilliseconds() == 725.5);

    {
        Renderer::PerformanceMetrics::Writer writer;
        assert(writer.open());
        assert(writer.write("gpu_pipeline_ms", 1.25));
        assert(writer.write("lumen_trace_ms", 0.75));
        writer.flush();
    }

    const std::vector<double> cpu_samples = {2.0, 2.5, 3.0};
    assert(Renderer::PerformanceMetrics::appendSamples("cpu_frame_ms", cpu_samples));

    std::ifstream input(path);
    const std::string text(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );

    assert(text.find("gpu_pipeline_ms=") == std::string::npos);
    assert(text.find("lumen_trace_ms=0.750000000") != std::string::npos);
    assert(text.find("cpu_frame_ms=2.000000000") != std::string::npos);
    assert(text.find("cpu_frame_ms=2.500000000") != std::string::npos);
    assert(text.find("cpu_frame_ms=3.000000000") != std::string::npos);

    std::filesystem::remove(path);
    return 0;
}
