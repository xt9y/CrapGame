#include "Renderer/Gpu/DirtyTilePolicy.hpp"
#include "Renderer/Gpu/DirtyTileShader.hpp"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static void require(bool value,const char* message)
{
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}

static std::string readFile(const char* path)
{
    std::ifstream input(path);
    std::ostringstream stream;
    stream<<input.rdbuf();
    return stream.str();
}

int main()
{
    using namespace Renderer::Gpu;
    require(DirtyTilePolicy::TILE_SIZE==8,"dirty shading tiles must be 8x8");
    require(DirtyTilePolicy::tileCount(1)==1,"one pixel still requires one tile");
    require(DirtyTilePolicy::tileCount(16)==2,"sixteen pixels require two tiles");
    require(DirtyTilePolicy::tileCount(17)==3,"partial edge tile must be retained");

    std::uint8_t mask[16*16];
    for(auto& value:mask)value=1u;
    require(!DirtyTilePolicy::tileDirty(mask,16,16,0,0),"fully valid tile stays reusable");
    mask[7*16+7]=0u;
    require(DirtyTilePolicy::tileDirty(mask,16,16,0,0),"one invalid pixel dirties the whole tile");
    require(!DirtyTilePolicy::tileDirty(mask,16,16,1,0),"neighbor tile remains reusable");

    const std::string shader=DIRTY_TILE_COMPACT_COMPUTE;
    require(shader.find("local_size_x=8,local_size_y=8")!=std::string::npos,
            "GPU compactor must scan one 8x8 tile per workgroup");
    require(shader.find("atomicAdd(groupCountX,1u)")!=std::string::npos,
            "dirty tiles must compact through the indirect group counter");
    require(shader.find("dirtyTiles[index]=gl_WorkGroupID.xy")!=std::string::npos,
            "compacted buffer must store dirty tile coordinates");

    const std::string gpu=readFile("Sources/Renderer/Gpu/DirtyTileGpu.cpp");
    require(gpu.find("glDispatchComputeIndirect(0)")!=std::string::npos,
            "dirty shading must use GPU indirect dispatch");
    require(gpu.find("glGetBufferSubData")==std::string::npos,
            "dirty tile count must never be read back to the CPU");
    require(gpu.find("glMapBuffer")==std::string::npos,
            "dirty tile compaction must not map the counter buffer");

    const std::string scene=readFile("Sources/Renderer/Gpu/LumenImportedScene.cpp");
    require(scene.find("dirty_tile_gpu_.compact(reprojection_cache_.validMaskTexture()")!=std::string::npos,
            "reprojection validity must feed dirty tile compaction");
    require(scene.find("dirty_tile_gpu_.bindTiles(8)")!=std::string::npos,
            "Lumen must bind compacted tile coordinates");
    require(scene.find("dirty_tile_gpu_.dispatchIndirect()")!=std::string::npos,
            "reprojected frames must trace only compacted dirty tiles");

    std::cout<<"dirty_tile_contract=PASS\n";
    return 0;
}
