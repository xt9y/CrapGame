#ifndef CRAPGAME_RENDERER_GPU_SHADOWPAGECACHEGPU_HPP
#define CRAPGAME_RENDERER_GPU_SHADOWPAGECACHEGPU_HPP

#include "Renderer/Gpu/ShadowPageCachePolicy.hpp"
#include "Renderer/Gpu/VirtualShadowPolicy.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Renderer
{
namespace Gpu
{

class ShadowPageCacheGpu
{
public:
    bool init (std::string *error = nullptr);
    void beginFrame (std::uint64_t frame_index);

    bool ensurePage (
                const ShadowPageKey& key,
                bool dynamic,
                std::uint64_t revision,
                std::uint32_t *physical,
                std::string *error = nullptr
        );

    void markRendered (std::uint32_t physical);
    void invalidateRevision (std::uint64_t revision);
    void invalidateLight (std::uint32_t light);
    void endFrame ();
    void shutdown ();

    GLuint metadataBuffer () const { return metadata_buffer_; }
    GLuint pageTableBuffer () const { return page_table_buffer_; }
    std::uint32_t requestedThisFrame () const { return requested_this_frame_; }
    std::uint32_t renderedThisFrame () const { return rendered_this_frame_; }
    std::uint32_t cachedThisFrame () const { return cached_this_frame_; }
    std::uint32_t evictedThisFrame () const { return evicted_this_frame_; }
    std::uint32_t staticInvalidatedThisFrame () const { return static_invalidated_this_frame_; }
    std::uint32_t dynamicInvalidatedThisFrame () const { return dynamic_invalidated_this_frame_; }
    const std::vector<ShadowPageState>& pages () const { return pages_; }

private:
    struct ShadowPageGpu
    {
        std::uint32_t light = 0u,
                      level_mip = 0u,
                      x = 0u,
                      y = 0u,
                      physical = 0u,
                      flags = 0u,
                      revision_low = 0u,
                      revision_high = 0u;
    };

    void uploadMetadata ();

    GLuint metadata_buffer_ = 0,
           page_table_buffer_ = 0;

    std::vector<ShadowPageState> pages_;
    std::vector<ShadowPageGpu> metadata_scratch_;
    std::vector<std::uint32_t> page_table_scratch_;

    std::uint64_t frame_index_ = 0u;
    std::uint32_t requested_this_frame_ = 0u,
                  rendered_this_frame_ = 0u,
                  cached_this_frame_ = 0u,
                  evicted_this_frame_ = 0u,
                  static_invalidated_this_frame_ = 0u,
                  dynamic_invalidated_this_frame_ = 0u;
};

} // namespace Gpu
} // namespace Renderer

#endif
