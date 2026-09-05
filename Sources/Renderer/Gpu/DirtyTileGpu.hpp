#ifndef CRAPGAME_RENDERER_GPU_DIRTYTILEGPU_HPP
#define CRAPGAME_RENDERER_GPU_DIRTYTILEGPU_HPP

#include "Renderer/Gpu/DirtyTilePolicy.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstdint>
#include <string>

namespace Renderer
{
namespace Gpu
{

class DirtyTileGpu
{
public:
    static constexpr int TILE_SIZE = DirtyTilePolicy::TILE_SIZE;

    ~DirtyTileGpu() { shutdown(); }
    DirtyTileGpu() = default;
    DirtyTileGpu(const DirtyTileGpu&) = delete;
    DirtyTileGpu& operator=(const DirtyTileGpu&) = delete;

    bool init(std::string *error=nullptr);
    bool resize(int width,int height,std::string *error=nullptr);
    bool ensure(int width,int height,std::string *error=nullptr);
    bool compact(GLuint valid_mask,std::string *error=nullptr)
    {
        return compact(valid_mask,0u,1u,error);
    }
    bool compact(GLuint valid_mask,
                 std::uint32_t slice_index,
                 std::uint32_t slice_count,
                 std::string *error=nullptr);
    void bindTiles(GLuint binding) const;
    void dispatchIndirect() const;
    void shutdown();

    bool ready() const;
    GLuint tileBuffer() const { return tile_buffer_; }
    GLuint indirectBuffer() const { return indirect_buffer_; }
    std::uint32_t totalCount() const { return total_tiles_; }

private:
    GLuint program_=0;
    GLuint tile_buffer_=0;
    GLuint indirect_buffer_=0;
    GLint slice_index_location_=-1;
    GLint slice_count_location_=-1;
    int width_=0,height_=0;
    int tiles_x_=0,tiles_y_=0;
    std::uint32_t total_tiles_=0u;
};

} // namespace Gpu
} // namespace Renderer

#endif
