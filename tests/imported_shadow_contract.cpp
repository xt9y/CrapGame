#include "Renderer/Gpu/DirectLightingImportedShader.hpp"
#include <cassert>
#include <string>
int main(){
 const std::string shader=Renderer::Gpu::directLightingImportedShader();
 assert(shader.find("traceImportedNearest")!=std::string::npos);
 assert(shader.find("importedShadowVisibility")!=std::string::npos);
 assert(shader.find("traceMaterialRejectsHit")!=std::string::npos);
 assert(shader.find("shadowed(position,normal,ld,maxD)")!=std::string::npos);
 assert(shader.find("radiance*=importedVisibility")!=std::string::npos);
 assert(shader.find("renderClass==1&&traceResolvedOpacity")!=std::string::npos);
 return 0;
}
