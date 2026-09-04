#ifndef CRAPGAME_RENDERER_GPU_TRACEGEOMETRYSHADER_HPP
#define CRAPGAME_RENDERER_GPU_TRACEGEOMETRYSHADER_HPP

namespace Renderer
{
namespace Gpu
{

constexpr const char *TRACE_GEOMETRY_COMPACT_COMPUTE=R"GLSL(
#version 430 core
layout(local_size_x=64,local_size_y=1,local_size_z=1) in;
struct SourceTriangle{
    vec4 p0;vec4 p1;vec4 p2;
    vec4 uv0Uv1;vec4 uv2Material;
    vec4 n0;vec4 n1;vec4 n2;
};
struct ShadowTriangle{vec4 p0;vec4 p1;vec4 p2;};
layout(std430,binding=0) readonly buffer SourceTriangleBuffer{SourceTriangle sourceTriangles[];};
layout(std430,binding=1) writeonly buffer ShadowTriangleBuffer{ShadowTriangle shadowTriangles[];};
uniform int uTriangleCount;
void main(){
    uint index=gl_GlobalInvocationID.x;
    if(index>=uint(max(uTriangleCount,0)))return;
    SourceTriangle source=sourceTriangles[index];
    ShadowTriangle shadow;
    shadow.p0=source.p0;
    shadow.p1=source.p1;
    shadow.p2=source.p2;
    shadowTriangles[index]=shadow;
}
)GLSL";

} // namespace Gpu
} // namespace Renderer

#endif
