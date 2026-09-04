#ifndef CRAPGAME_RENDERER_GPU_GBUFFERRECONSTRUCT_HPP
#define CRAPGAME_RENDERER_GPU_GBUFFERRECONSTRUCT_HPP

#include "Renderer/Math/Math.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer
{
namespace Gpu
{

inline bool invertGBufferViewProjection(const Math::Mat4& matrix,Math::Mat4 *inverse)
{
    if(!inverse)return false;
    float augmented[4][8]={};
    for(int row=0;row<4;++row)
    {
        for(int column=0;column<4;++column)
            augmented[row][column]=matrix.value[column*4+row];
        augmented[row][row+4]=1.0f;
    }
    for(int column=0;column<4;++column)
    {
        int pivot=column;
        for(int row=column+1;row<4;++row)
            if(std::fabs(augmented[row][column])>std::fabs(augmented[pivot][column]))pivot=row;
        if(std::fabs(augmented[pivot][column])<=1.0e-8f)return false;
        if(pivot!=column)
            for(int entry=0;entry<8;++entry)std::swap(augmented[pivot][entry],augmented[column][entry]);
        const float scale=1.0f/augmented[column][column];
        for(int entry=0;entry<8;++entry)augmented[column][entry]*=scale;
        for(int row=0;row<4;++row)
        {
            if(row==column)continue;
            const float factor=augmented[row][column];
            for(int entry=0;entry<8;++entry)augmented[row][entry]-=factor*augmented[column][entry];
        }
    }
    for(int row=0;row<4;++row)
        for(int column=0;column<4;++column)
            inverse->value[column*4+row]=augmented[row][column+4];
    return true;
}

inline bool projectGBufferWorldPosition(const Math::Mat4& view_projection,
                                        const Math::Vec3& world,
                                        Math::Vec2 *uv,float *depth)
{
    if(!uv||!depth)return false;
    const Math::Vec4 clip=Math::transform(view_projection,{world.x,world.y,world.z,1.0f});
    if(std::fabs(clip.w)<=1.0e-8f)return false;
    const float inverse_w=1.0f/clip.w;
    uv->x=clip.x*inverse_w*0.5f+0.5f;
    uv->y=clip.y*inverse_w*0.5f+0.5f;
    *depth=clip.z*inverse_w*0.5f+0.5f;
    return true;
}

inline Math::Vec3 reconstructGBufferWorldPosition(const Math::Mat4& inverse_view_projection,
                                                   const Math::Vec2& uv,float depth)
{
    const Math::Vec4 world=Math::transform(inverse_view_projection,
        {uv.x*2.0f-1.0f,uv.y*2.0f-1.0f,depth*2.0f-1.0f,1.0f});
    if(std::fabs(world.w)<=1.0e-8f)return {world.x,world.y,world.z};
    const float inverse_w=1.0f/world.w;
    return {world.x*inverse_w,world.y*inverse_w,world.z*inverse_w};
}

constexpr const char *GBUFFER_RECONSTRUCT_GLSL=R"GLSL(
bool gbufferDepthValid(float depthValue){return depthValue<0.999999;}
vec3 gbufferReconstructWorld(ivec2 pixel,ivec2 dimensions,float depthValue,mat4 inverseViewProjection){
    vec2 uv=(vec2(pixel)+vec2(0.5))/vec2(dimensions);
    vec4 clip=vec4(uv*2.0-1.0,depthValue*2.0-1.0,1.0);
    vec4 world=inverseViewProjection*clip;
    return abs(world.w)>1e-8?world.xyz/world.w:world.xyz;
}
)GLSL";

} // namespace Gpu
} // namespace Renderer

#endif
