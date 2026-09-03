#include "PerformanceMetrics.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <string_view>

namespace Renderer
{
namespace PerformanceMetrics
{
namespace
{

bool truthy (const char *value)
{
    if (!value)
    {
        return false;
    }

    const std::string_view text(value);
    return text == "1"
        || text == "true"
        || text == "yes"
        || text == "on";
}

double nonnegativeEnvironmentMilliseconds (const char *name)
{
    const char *value = std::getenv(name);

    if (!value || !*value)
    {
        return 0.0;
    }

    char *end = nullptr;
    errno = 0;
    const double parsed = std::strtod(value, &end);

    if (errno != 0
            || end == value
            || *end != '\0'
            || !std::isfinite(parsed)
            || parsed < 0.0)
    {
        return 0.0;
    }

    return parsed;
}

bool metricNameValid (const char *name)
{
    if (!name || !*name)
    {
        return false;
    }

    for (const unsigned char *cursor =
            reinterpret_cast<const unsigned char *>(name);
            *cursor;
            ++cursor)
    {
        const unsigned char c = *cursor;
        const bool valid =
            (c >= 'a' && c <= 'z')
            || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9')
            || c == '_'
            || c == '-'
            || c == '.';

        if (!valid)
        {
            return false;
        }
    }

    return true;
}

} // namespace

bool requested ()
{
    return truthy(std::getenv("RENDERCHECK_PERF"));
}

double warmupMilliseconds ()
{
    return nonnegativeEnvironmentMilliseconds(
            "RENDERCHECK_PERF_WARMUP_MS"
        );
}

double durationMilliseconds ()
{
    return nonnegativeEnvironmentMilliseconds(
            "RENDERCHECK_PERF_DURATION_MS"
        );
}

Writer::~Writer ()
{
    close();
}

bool Writer::open ()
{
    if (stream_.is_open())
    {
        return true;
    }

    const char *path = std::getenv("RENDERCHECK_METRICS_PATH");

    if (!path || !*path)
    {
        return false;
    }

    stream_.open(path, std::ios::out | std::ios::app);
    return stream_.is_open();
}

bool Writer::write (const char *name, double value)
{
    if (!metricNameValid(name)
            || !std::isfinite(value)
            || value < 0.0
            || (!stream_.is_open() && !open()))
    {
        return false;
    }

    stream_
        << name
        << '='
        << std::fixed
        << std::setprecision(9)
        << value
        << '\n';

    return static_cast<bool>(stream_);
}

void Writer::flush ()
{
    if (stream_.is_open())
    {
        stream_.flush();
    }
}

void Writer::close ()
{
    if (stream_.is_open())
    {
        stream_.flush();
        stream_.close();
    }
}

bool appendSamples (
        const char *name,
        const std::vector<double>& samples
    )
{
    if (samples.empty())
    {
        return true;
    }

    Writer writer;

    if (!writer.open())
    {
        return false;
    }

    for (const double sample : samples)
    {
        if (!writer.write(name, sample))
        {
            return false;
        }
    }

    writer.flush();
    return true;
}

} // namespace PerformanceMetrics
} // namespace Renderer
