#ifndef CRAPGAME_RENDERER_LUMEN_SCREENPROBE_HPP
#define CRAPGAME_RENDERER_LUMEN_SCREENPROBE_HPP

#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Lumen/RadianceCache.hpp"
#include "Renderer/Lumen/Sampling.hpp"
#include "Renderer/Lumen/SurfaceCache.hpp"
#include "Renderer/Lumen/Tracer.hpp"

#include <cstdint>
#include <vector>

namespace Renderer 
{
namespace Lumen 
{

struct ScreenProbeTimings
{
    double trace_ms = 0.0;
    double reconstruct_ms = 0.0;
    double filter_ms = 0.0;
    double contact_ao_ms = 0.0;
};

struct ScreenProbeSample
{
    Math::Vec3 position,
               normal,
               indirect;

    Ecs::Entity entity = Ecs::INVALID_ENTITY;

    int x = 0,
        y = 0;

    bool valid = false;
};

class ScreenProbeGather 
{

public:
    void gather (
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Tracer& tracer,
                const SurfaceCache& surface_cache,
                const RadianceCache& radiance_cache,
                std::uint64_t frame_index,
                int spacing,
                int ray_count,
                std::vector<Math::Vec3> *output,
                ScreenProbeTimings *timings = nullptr
        );

private:
    std::vector<ScreenProbeSample> probes_scratch_;
    std::vector<HemisphereSample> probe_sequence_scratch_;
    std::vector<HemisphereSample> ao_sequence_scratch_;
    std::vector<Math::Vec3> reconstructed_scratch_;
    std::vector<Math::Vec3> filtered_indirect_scratch_;
    std::vector<std::uint8_t> filtered_valid_scratch_;
};

} // namespace Lumen
} // namespace Renderer

#endif
