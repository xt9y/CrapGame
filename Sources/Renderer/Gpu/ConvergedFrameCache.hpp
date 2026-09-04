#ifndef CRAPGAME_RENDERER_GPU_CONVERGEDFRAMECACHE_HPP
#define CRAPGAME_RENDERER_GPU_CONVERGEDFRAMECACHE_HPP

#include "Renderer/Gpu/ConvergencePolicy.hpp"
#include "Renderer/Gpu/RevisionState.hpp"

#include <cstdint>

namespace Renderer
{
namespace Gpu
{

class ConvergedFrameCache
{
public:
    void invalidate ();
    void reset ();
    void begin (const RevisionState& revisions);
    void recordSample (
        const RevisionState& revisions,
        bool history_refinement_requested
    );
    bool needsSample (const RevisionState& revisions) const;
    bool frozen (const RevisionState& revisions) const;
    std::uint32_t sampleCount () const { return sample_count_; }

private:
    RevisionState revisions_ = {};
    std::uint32_t sample_count_ = 0u;
    std::uint32_t target_samples_ = ConvergencePolicy::DEFAULT_SAMPLES;
    bool refinement_requested_ = true;
    bool valid_ = false;
};

} // namespace Gpu
} // namespace Renderer

#endif
