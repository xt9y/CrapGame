#ifndef CRAPGAME_RENDERER_PERFORMANCE_METRICS_HPP
#define CRAPGAME_RENDERER_PERFORMANCE_METRICS_HPP

#include <fstream>
#include <string>
#include <vector>

namespace Renderer
{
namespace PerformanceMetrics
{

bool requested ();
double warmupMilliseconds ();
double durationMilliseconds ();

class Writer
{
public:
    Writer () = default;
    ~Writer ();

    Writer (const Writer&) = delete;
    Writer& operator= (const Writer&) = delete;

    bool open ();
    bool write (const char *name, double value);
    void flush ();
    void close ();

private:
    std::ofstream stream_;
};

bool appendSamples (
        const char *name,
        const std::vector<double>& samples
    );

} // namespace PerformanceMetrics
} // namespace Renderer

#endif
