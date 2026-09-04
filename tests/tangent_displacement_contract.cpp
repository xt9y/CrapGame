#include "Models/Material.hpp"
#include "Models/Texture.hpp"
#include "Renderer/Material/Material.hpp"
#include "Renderer/Mesh/Tangent.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
bool near(float a,float b,float e=0.002f){return std::fabs(a-b)<e;}
bool finite4(const Renderer::Math::Vec4& v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.w);}
void writeGrayTga(const std::filesystem::path& p,int w,int h,const std::vector<std::uint8_t>& px){
 std::vector<std::uint8_t> b(18u,0u); b[2]=3u; b[12]=static_cast<std::uint8_t>(w); b[14]=static_cast<std::uint8_t>(h); b[16]=8u; b[17]=0x20u; b.insert(b.end(),px.begin(),px.end()); std::ofstream o(p,std::ios::binary); o.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
}
}

int main(){
 using namespace Renderer;
 Mesh::MeshData quad;
 quad.vertices={
   {{0,0,0},{0,0,1},{0,0},{0,0,0,1}},
   {{1,0,0},{0,0,1},{1,0},{0,0,0,1}},
   {{1,1,0},{0,0,1},{1,1},{0,0,0,1}},
   {{0,1,0},{0,0,1},{0,1},{0,0,0,1}},
 };
 quad.indices={0,1,2,0,2,3};
 Mesh::generateTangents(&quad);
 for(const auto& v:quad.vertices){assert(near(v.tangent.x,1));assert(near(v.tangent.y,0));assert(near(v.tangent.z,0));assert(near(v.tangent.w,1));}

 Mesh::MeshData deg;
 deg.vertices={
   {{0,0,0},{0,0,1},{0,0},{0,0,0,1}},
   {{1,0,0},{0,0,1},{0,0},{0,0,0,1}},
   {{0,1,0},{0,0,1},{0,0},{0,0,0,1}},
 };
 deg.indices={0,1,2}; Mesh::generateTangents(&deg);
 for(const auto& v:deg.vertices){assert(finite4(v.tangent)); assert(near(std::sqrt(v.tangent.x*v.tangent.x+v.tangent.y*v.tangent.y+v.tangent.z*v.tangent.z),1));}

 const std::filesystem::path dir="/tmp/crapgame-tangent-contract"; std::filesystem::remove_all(dir); std::filesystem::create_directories(dir);
 writeGrayTga(dir/"height.tga",2,2,{0,255,128,255});
 Models::clearTextureCache(); std::string error;
 const auto th=Models::loadTexture((dir/"height.tga").string(),&error); assert(th!=Models::INVALID_TEXTURE);
 Material::TextureBinding bind; bind.texture=th; bind.scale={1,1,1}; bind.offset={0,0,0}; bind.channel='r'; bind.multiplier=2.0f; bind.clamp=true;
 Mesh::MeshData disp=quad; for(auto& v:disp.vertices){v.position.z=0;v.normal={0,0,1};}
 assert(Mesh::applyDisplacement(&disp,bind,0.5f,&error));
 assert(near(disp.vertices[0].position.z,0.0f));
 assert(disp.vertices[1].position.z>0.95f);
 for(const auto& v:disp.vertices){assert(finite4(v.tangent));assert(std::isfinite(v.normal.x)&&std::isfinite(v.normal.y)&&std::isfinite(v.normal.z));}

 writeGrayTga(dir/"wall_ddn.tga",1,1,{128});
 Models::MaterialData m; m.displacement_texture.path=(dir/"wall_ddn.tga").string();
 std::vector<std::string> warnings; auto r=Models::resolveMaterial(m,false,&warnings);
 assert(r.textures[Material::slotIndex(Material::Slot::Normal)].texture!=Models::INVALID_TEXTURE);
 assert(r.textures[Material::slotIndex(Material::Slot::Displacement)].texture==Models::INVALID_TEXTURE);
 return 0;
}
