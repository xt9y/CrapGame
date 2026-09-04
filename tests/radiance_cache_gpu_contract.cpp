#include "Renderer/Gpu/LumenImportedShader.hpp"
#include "Renderer/Gpu/RadianceCachePolicy.hpp"
#include "Renderer/Gpu/RadianceCacheShader.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static void require(bool value,const char *message)
{
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}

static std::string readFile(const char *path)
{
    std::ifstream input(path);
    std::ostringstream stream;
    stream<<input.rdbuf();
    return stream.str();
}

static std::string compact(std::string value)
{
    std::string result;
    result.reserve(value.size());
    for(char c:value)
        if(!std::isspace(static_cast<unsigned char>(c)))result.push_back(c);
    return result;
}

int main()
{
    using namespace Renderer::Gpu;

    require(RadianceCachePolicy::CELL_SIZE==0.5f,"radiance cell size");
    require(RadianceCachePolicy::INITIAL_CAPACITY==65536u,"radiance capacity");
    require((RadianceCachePolicy::INITIAL_CAPACITY
             &(RadianceCachePolicy::INITIAL_CAPACITY-1u))==0u,
            "radiance capacity must be power of two");
    require(RadianceCachePolicy::HIGH_CONFIDENCE_SAMPLES==16u,"confidence cap");
    require(RadianceCachePolicy::ACCEPT_CONFIDENCE==4u,"accept confidence");
    require(RadianceCachePolicy::MAX_LINEAR_PROBES==8u,"probe cap");
    require(sizeof(RadianceCacheRecordGpu)==64u,"record must stay 64 bytes");

    const auto c=RadianceCachePolicy::cell(-0.01f,0.49f,0.50f);
    require(c[0]==-1&&c[1]==0&&c[2]==1,"signed world-cell mapping");
    require(RadianceCachePolicy::hashCell(-1,0,1)
            ==RadianceCachePolicy::hashCell(-1,0,1),"hash deterministic");
    require(RadianceCachePolicy::hashCell(-1,0,1)
            !=RadianceCachePolicy::hashCell(0,0,1),"neighbor hash differentiation");

    RevisionState cached{};
    cached.geometry=1u;cached.material=2u;cached.lighting=3u;cached.camera=4u;
    cached.resolution=5u;cached.mesh_registry=6u;cached.material_registry=7u;
    RevisionState current=cached;
    current.camera++;
    require(!RadianceCachePolicy::generationChanges(cached,current),
            "camera revision must not advance radiance generation");
    current=cached;current.geometry++;
    require(RadianceCachePolicy::generationChanges(cached,current),"geometry generation");
    current=cached;current.material++;
    require(RadianceCachePolicy::generationChanges(cached,current),"material generation");
    current=cached;current.lighting++;
    require(RadianceCachePolicy::generationChanges(cached,current),"lighting generation");
    current=cached;current.mesh_registry++;
    require(RadianceCachePolicy::generationChanges(cached,current),"mesh generation");
    current=cached;current.material_registry++;
    require(RadianceCachePolicy::generationChanges(cached,current),"material registry generation");

    const std::string cache=compact(RADIANCE_CACHE_GLSL);
    require(cache.find("binding=9")!=std::string::npos,"radiance SSBO binding");
    require(cache.find("RADIANCE_MAX_PROBES=8u")!=std::string::npos,"bounded probes");
    require(cache.find("RADIANCE_ACCEPT_CONFIDENCE=4u")!=std::string::npos,"lookup confidence");
    require(cache.find("RADIANCE_HIGH_CONFIDENCE=16u")!=std::string::npos,"update confidence cap");
    require(cache.find("for(intz=0;z<2;++z)for(inty=0;y<2;++y)for(intx=0;x<2;++x)")!=std::string::npos,
            "lookup must sample eight neighboring cells");
    require(cache.find("if(state==1u)return")!=std::string::npos,
            "contended insert must skip rather than create a duplicate key");
    require(cache.find("atomicCompSwap")!=std::string::npos
            &&cache.find("atomicExchange")!=std::string::npos,
            "cache updates must publish records atomically");
    require(cache.find("memoryBarrierBuffer")!=std::string::npos,
            "cache publication must include a buffer memory barrier");

    const std::string generated=compact(lumenImportedTraceShader());
    require(generated.find("radianceCacheLookup(position,normal,giSource)")!=std::string::npos,
            "Lumen GI must query world radiance before tracing");
    require(generated.find("radianceCacheUpdate(position,normal,cacheSource)")!=std::string::npos,
            "Lumen GI miss must update the nearest cache cell");
    require(generated.find("cacheSource=primitiveFallbackRadiance(giHit)")!=std::string::npos,
            "world cache updates must not store camera-dependent screen radiance");
    require(generated.find("indirect=giSource*albedo*(1.0-metallic)*0.32")!=std::string::npos,
            "cache value must remain source radiance rather than material-baked indirect");

    const std::string gpu=compact(readFile("Sources/Renderer/Gpu/RadianceCacheGpu.cpp"));
    require(gpu.find("glGetBufferSubData")==std::string::npos
            &&gpu.find("glMapBuffer")==std::string::npos
            &&gpu.find("glGetTexImage")==std::string::npos,
            "normal radiance caching must not add GPU-to-CPU readback");
    const std::string scene=compact(readFile("Sources/Renderer/Gpu/LumenImportedScene.cpp"));
    require(scene.find("radiance_cache_.updateGeneration")!=std::string::npos,
            "Lumen must advance cache generation from semantic scene revisions");
    require(scene.find("radiance_cache_.bind")!=std::string::npos,
            "Lumen trace must bind the radiance cache SSBO");
    require(scene.find("trace_radiance_generation_location_")!=std::string::npos,
            "Lumen trace must provide the generation uniform");
    require(scene.find("GL_SHADER_STORAGE_BARRIER_BIT")!=std::string::npos,
            "Lumen must publish cache writes before the next trace dispatch");

    std::cout<<"radiance_cache_gpu_contract=PASS\n";
    return 0;
}
