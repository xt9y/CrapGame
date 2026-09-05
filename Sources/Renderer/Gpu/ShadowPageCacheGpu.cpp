#include "Renderer/Gpu/ShadowPageCacheGpu.hpp"

#include <algorithm>
#include <limits>

namespace Renderer
{
namespace Gpu
{
namespace
{

void setError (std::string *error, const char *message)
{
    if (error) *error = message ? message : "shadow page cache error";
}

} // namespace

bool ShadowPageCacheGpu::init (std::string *error)
{
    shutdown();

    GL15.glGenBuffers(1, &metadata_buffer_);
    GL15.glGenBuffers(1, &page_table_buffer_);
    if (metadata_buffer_ == 0 || page_table_buffer_ == 0)
    {
        setError(error, "failed to allocate shadow page-cache buffers");
        shutdown();
        return false;
    }

    pages_.resize(VirtualShadowPolicy::MAX_PHYSICAL_PAGES);
    metadata_scratch_.resize(VirtualShadowPolicy::MAX_PHYSICAL_PAGES);
    page_table_scratch_.resize(
            VirtualShadowPolicy::MAX_PHYSICAL_PAGES,
            std::numeric_limits<std::uint32_t>::max()
        );

    for (std::size_t index = 0; index < pages_.size(); ++index)
    {
        pages_[index].physical = static_cast<std::uint32_t>(index);
    }

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, metadata_buffer_);
    GL15.glBufferData(
            GL_SHADER_STORAGE_BUFFER,
            static_cast<LWCGLsizeiptr>(
                    metadata_scratch_.size() * sizeof(ShadowPageGpu)
                ),
            metadata_scratch_.data(),
            GL_DYNAMIC_DRAW
        );

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, page_table_buffer_);
    GL15.glBufferData(
            GL_SHADER_STORAGE_BUFFER,
            static_cast<LWCGLsizeiptr>(
                    page_table_scratch_.size() * sizeof(std::uint32_t)
                ),
            page_table_scratch_.data(),
            GL_DYNAMIC_DRAW
        );
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    if (error) error->clear();
    return true;
}

void ShadowPageCacheGpu::beginFrame (std::uint64_t frame_index)
{
    frame_index_ = frame_index;
    requested_this_frame_ = 0u;
    rendered_this_frame_ = 0u;
    cached_this_frame_ = 0u;
    evicted_this_frame_ = 0u;
    static_invalidated_this_frame_ = 0u;
    dynamic_invalidated_this_frame_ = 0u;
}

bool ShadowPageCacheGpu::ensurePage (
            const ShadowPageKey& key,
            bool dynamic,
            std::uint64_t revision,
            std::uint32_t *physical,
            std::string *error
    )
{
    if (!physical || pages_.empty())
    {
        setError(error, "shadow page cache is not initialized");
        return false;
    }

    ++requested_this_frame_;

    for (ShadowPageState& page : pages_)
    {
        if (!page.allocated || !sameShadowPageKey(page.key, key))
        {
            continue;
        }

        if (page.revision != revision)
        {
            page.revision = revision;
            page.dirty = true;
        }
        else if (!page.dirty)
        {
            ++cached_this_frame_;
        }

        page.dynamic = dynamic;
        page.last_requested = frame_index_;
        *physical = page.physical;
        if (error) error->clear();
        return true;
    }

    int selected = -1;
    for (std::size_t index = 0; index < pages_.size(); ++index)
    {
        if (!pages_[index].allocated)
        {
            selected = static_cast<int>(index);
            break;
        }
    }

    if (selected < 0)
    {
        selected = chooseShadowPageEviction(pages_, frame_index_);
        if (selected < 0)
        {
            setError(error, "shadow page pool exhausted by current-frame requests");
            return false;
        }
        ++evicted_this_frame_;
    }

    ShadowPageState& page = pages_[static_cast<std::size_t>(selected)];
    const std::uint32_t physical_page = page.physical;
    page = {};
    page.key = key;
    page.physical = physical_page;
    page.last_requested = frame_index_;
    page.revision = revision;
    page.allocated = true;
    page.dirty = true;
    page.dynamic = dynamic;
    *physical = page.physical;

    if (error) error->clear();
    return true;
}

void ShadowPageCacheGpu::markRendered (std::uint32_t physical)
{
    if (physical >= pages_.size())
    {
        return;
    }

    ShadowPageState& page = pages_[physical];
    if (!page.allocated || !page.dirty)
    {
        return;
    }

    page.dirty = false;
    ++rendered_this_frame_;
}

void ShadowPageCacheGpu::invalidateRevision (std::uint64_t revision)
{
    for (ShadowPageState& page : pages_)
    {
        if (!page.allocated || page.revision == revision)
        {
            continue;
        }

        if (!page.dirty)
        {
            if (page.dynamic) ++dynamic_invalidated_this_frame_;
            else ++static_invalidated_this_frame_;
        }
        page.dirty = true;
    }
}

void ShadowPageCacheGpu::invalidateLight (std::uint32_t light)
{
    for (ShadowPageState& page : pages_)
    {
        if (!page.allocated || page.key.light != light)
        {
            continue;
        }

        if (!page.dirty)
        {
            if (page.dynamic) ++dynamic_invalidated_this_frame_;
            else ++static_invalidated_this_frame_;
        }
        page.dirty = true;
    }
}

void ShadowPageCacheGpu::uploadMetadata ()
{
    const std::uint32_t invalid = std::numeric_limits<std::uint32_t>::max();

    for (std::size_t index = 0; index < pages_.size(); ++index)
    {
        const ShadowPageState& page = pages_[index];
        ShadowPageGpu& gpu = metadata_scratch_[index];
        gpu = {};
        page_table_scratch_[index] = invalid;

        if (!page.allocated)
        {
            continue;
        }

        gpu.light = page.key.light;
        gpu.level_mip = static_cast<std::uint32_t>(page.key.level)
            | (static_cast<std::uint32_t>(page.key.mip) << 16u);
        gpu.x = static_cast<std::uint32_t>(page.key.x);
        gpu.y = static_cast<std::uint32_t>(page.key.y);
        gpu.physical = page.physical;
        gpu.flags = 1u
            | (page.dirty ? 2u : 0u)
            | (page.dynamic ? 4u : 0u);
        gpu.revision_low = static_cast<std::uint32_t>(page.revision);
        gpu.revision_high = static_cast<std::uint32_t>(page.revision >> 32u);
        page_table_scratch_[index] = page.physical;
    }

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, metadata_buffer_);
    GL15.glBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            0,
            static_cast<LWCGLsizeiptr>(
                    metadata_scratch_.size() * sizeof(ShadowPageGpu)
                ),
            metadata_scratch_.data()
        );

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, page_table_buffer_);
    GL15.glBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            0,
            static_cast<LWCGLsizeiptr>(
                    page_table_scratch_.size() * sizeof(std::uint32_t)
                ),
            page_table_scratch_.data()
        );
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ShadowPageCacheGpu::endFrame ()
{
    if (metadata_buffer_ == 0 || page_table_buffer_ == 0)
    {
        return;
    }

    uploadMetadata();
}

void ShadowPageCacheGpu::shutdown ()
{
    if (metadata_buffer_ != 0)
    {
        GL15.glDeleteBuffers(1, &metadata_buffer_);
    }
    if (page_table_buffer_ != 0)
    {
        GL15.glDeleteBuffers(1, &page_table_buffer_);
    }

    metadata_buffer_ = 0;
    page_table_buffer_ = 0;
    pages_.clear();
    metadata_scratch_.clear();
    page_table_scratch_.clear();
    frame_index_ = 0u;
    requested_this_frame_ = 0u;
    rendered_this_frame_ = 0u;
    cached_this_frame_ = 0u;
    evicted_this_frame_ = 0u;
    static_invalidated_this_frame_ = 0u;
    dynamic_invalidated_this_frame_ = 0u;
}

} // namespace Gpu
} // namespace Renderer
