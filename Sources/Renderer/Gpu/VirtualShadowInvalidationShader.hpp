#ifndef CRAPGAME_RENDERER_GPU_VIRTUALSHADOWINVALIDATIONSHADER_HPP
#define CRAPGAME_RENDERER_GPU_VIRTUALSHADOWINVALIDATIONSHADER_HPP

namespace Renderer
{
namespace Gpu
{

constexpr const char *VIRTUAL_SHADOW_INVALIDATION_COMPUTE=R"GLSL(
#version 430 core
layout(local_size_x=64,local_size_y=1,local_size_z=1) in;
#define MAX_PHYSICAL_PAGES 2048u
#define PAGE_VALID 1u
#define PAGE_DIRTY 2u
#define PAGE_QUEUED 8u
struct ShadowPhysicalPage
{
    ivec4 key;
    uvec4 state;
};
struct ShadowInvalidationRegion
{
    vec4 minimum;
    vec4 maximum;
};
layout(std430,binding=0) buffer ShadowPageMetadata
{
    ShadowPhysicalPage shadowPages[];
};
layout(std430,binding=1) readonly buffer ShadowInvalidationRegions
{
    ShadowInvalidationRegion shadowInvalidationRegions[];
};
uniform int uInvalidationRegionCount;
uniform int uInvalidationLightIndex;
uniform int uInvalidateLight;
uniform vec3 uShadowRight;
uniform vec3 uShadowUp;

bool pageIntersectsRegion(ShadowPhysicalPage page,ShadowInvalidationRegion region)
{
    int level=page.key.y;
    float extent=exp2(float(level));
    float pageWorldSize=max(extent*2.0/128.0,1e-6);
    vec3 center=(region.minimum.xyz+region.maximum.xyz)*0.5;
    vec3 radius=max((region.maximum.xyz-region.minimum.xyz)*0.5,vec3(0.0));
    float centerX=dot(center,uShadowRight);
    float centerY=dot(center,uShadowUp);
    float radiusX=dot(radius,abs(uShadowRight));
    float radiusY=dot(radius,abs(uShadowUp));
    int minimumX=int(floor((centerX-radiusX)/pageWorldSize));
    int maximumX=int(floor((centerX+radiusX)/pageWorldSize));
    int minimumY=int(floor((centerY-radiusY)/pageWorldSize));
    int maximumY=int(floor((centerY+radiusY)/pageWorldSize));
    return page.key.z>=minimumX&&page.key.z<=maximumX&&
           page.key.w>=minimumY&&page.key.w<=maximumY;
}

void main()
{
    uint physical=gl_GlobalInvocationID.x;
    if(physical>=MAX_PHYSICAL_PAGES)return;
    ShadowPhysicalPage page=shadowPages[physical];
    if((page.state.z&PAGE_VALID)==0u||page.key.x!=uInvalidationLightIndex)return;

    bool dirty=uInvalidateLight!=0;
    if(!dirty)
    {
        for(int regionIndex=0;regionIndex<uInvalidationRegionCount;++regionIndex)
        {
            if(pageIntersectsRegion(page,shadowInvalidationRegions[regionIndex]))
            {
                dirty=true;
                break;
            }
        }
    }
    if(!dirty)return;

    atomicOr(shadowPages[physical].state.z,PAGE_DIRTY);
    atomicAnd(shadowPages[physical].state.z,~PAGE_QUEUED);
}
)GLSL";

} // namespace Gpu
} // namespace Renderer

#endif
