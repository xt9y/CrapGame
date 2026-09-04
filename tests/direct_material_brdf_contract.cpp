#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
static std::string read(const char* p){std::ifstream in(p);return std::string((std::istreambuf_iterator<char>(in)),{});}
int main(){
 const std::string shader=read("Sources/Renderer/Gpu/DirectLightingShader.hpp");
 const std::string scene=read("Sources/Renderer/Gpu/DirectLightingScene.cpp");
 assert(shader.find("iorToF0")!=std::string::npos);
 assert(shader.find("distributionAnisotropic")!=std::string::npos);
 assert(shader.find("clearcoat")!=std::string::npos);
 assert(shader.find("sheen")!=std::string::npos);
 assert(shader.find("sSpecularIor")!=std::string::npos);
 assert(shader.find("mix(vec3(0.04),albedo")==std::string::npos);
 assert(scene.find("specularIorTexture")!=std::string::npos);
 assert(scene.find("advancedMaterialTexture")!=std::string::npos);
 assert(scene.find("tangentAnisotropyTexture")!=std::string::npos);
}
