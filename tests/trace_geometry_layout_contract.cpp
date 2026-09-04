#include "Renderer/Gpu/DirectLightingImportedShader.hpp"
#include "Renderer/Gpu/ShadowTriangleGpu.hpp"
#include "Renderer/Gpu/ShadowTriangleShader.hpp"
#include "Renderer/Gpu/TraceGeometryShader.hpp"

#include <cassert>
#include <string>

int main()
{
    static_assert(sizeof(Renderer::Gpu::ShadowTriangleGpu)==48u,
                  "compact shadow triangle layout changed");
    static_assert(sizeof(Renderer::Gpu::ShadowTriangleGpu)<128u,
                  "shadow record must stay smaller than TriangleGpu");

    const std::string shadow=Renderer::Gpu::IMPORTED_SHADOW_TRIANGLE_GLSL;
    assert(shadow.find("vec4 p0;vec4 p1;vec4 p2")!=std::string::npos);
    assert(shadow.find("uv0") == std::string::npos);
    assert(shadow.find("n0") == std::string::npos);
    assert(shadow.find("traceRecords") == std::string::npos);

    const std::string compact=Renderer::Gpu::TRACE_GEOMETRY_COMPACT_COMPUTE;
    assert(compact.find("shadow.p0=source.p0")!=std::string::npos);
    assert(compact.find("shadow.p1=source.p1")!=std::string::npos);
    assert(compact.find("shadow.p2=source.p2")!=std::string::npos);

    const std::string direct=Renderer::Gpu::directLightingImportedShader();
    assert(direct.find("traceImportedShadowInstanceAny(ii,ro,rd,maximumDistance)")!=std::string::npos);
    assert(direct.find("bool opaque=(shadowInstance.flags&3u)==0u")!=std::string::npos);
    assert(direct.find("if(opaque&&traceImportedInstanceAny(ii,ro,rd,maximumDistance))")
           ==std::string::npos);
    assert(direct.find("traceImportedNearest")!=std::string::npos);
    return 0;
}
