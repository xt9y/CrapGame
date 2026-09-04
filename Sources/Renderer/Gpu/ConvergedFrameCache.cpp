#include "Renderer/Gpu/ConvergedFrameCache.hpp"

#include <algorithm>

namespace Renderer
{
namespace Gpu
{

void ConvergedFrameCache::invalidate ()
{
    valid_ = false;
    sample_count_ = 0u;
    refinement_requested_ = true;
}

void ConvergedFrameCache::reset ()
{
    revisions_ = {};
    target_samples_ = ConvergencePolicy::DEFAULT_SAMPLES;
    invalidate();
}

void ConvergedFrameCache::begin (const RevisionState& revisions)
{
    revisions_ = revisions;
    target_samples_ = std::min(
        ConvergencePolicy::configuredSamples(),
        ConvergencePolicy::MAX_SAMPLES
    );
    sample_count_ = 0u;
    refinement_requested_ = true;
    valid_ = true;
}

void ConvergedFrameCache::recordSample (
        const RevisionState& revisions,
        bool history_refinement_requested)
{
    if (!valid_ || !sameFrameInputs(revisions_, revisions))
        begin(revisions);

    if (sample_count_ < ConvergencePolicy::MAX_SAMPLES)
        ++sample_count_;

    refinement_requested_ = history_refinement_requested;
}

bool ConvergedFrameCache::needsSample (const RevisionState& revisions) const
{
    if (!valid_ || !sameFrameInputs(revisions_, revisions)) return true;

    const std::uint32_t mandatory = std::min(
        target_samples_,
        ConvergencePolicy::MIN_SAMPLES
    );
    if (sample_count_ < mandatory) return true;
    if (sample_count_ >= target_samples_) return false;
    return refinement_requested_;
}

bool ConvergedFrameCache::frozen (const RevisionState& revisions) const
{
    return valid_
        && sameFrameInputs(revisions_, revisions)
        && !needsSample(revisions);
}

} // namespace Gpu
} // namespace Renderer
