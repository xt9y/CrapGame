#include "Models/Obj.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Models { namespace Obj { namespace {

struct Ref { int position=-1, uv=-1, normal=-1; };
struct Triangle { std::array<Ref,3> corners; int smoothing=0; };
struct RawPart { std::string object_name, group_name, material_name; std::vector<Triangle> triangles; };

std::string trim(const std::string& value)
{
    std::size_t first=0; while(first<value.size() && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
    std::size_t last=value.size(); while(last>first && std::isspace(static_cast<unsigned char>(value[last-1]))) --last;
    return value.substr(first,last-first);
}

std::string lower(std::string value)
{
    for(char& c:value) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

std::vector<std::string> tokens(const std::string& line)
{
    std::vector<std::string> result; std::string current; char quote='\0';
    for(char c:line)
    {
        if(quote!='\0')
        {
            if(c==quote) quote='\0'; else current.push_back(c);
        }
        else if(c=='"' || c=='\'') quote=c;
        else if(std::isspace(static_cast<unsigned char>(c)))
        {
            if(!current.empty()){result.push_back(current);current.clear();}
        }
        else current.push_back(c);
    }
    if(!current.empty()) result.push_back(current);
    return result;
}

bool parseFloat(const std::string& text,float* value)
{
    if(!value) return false;
    char* end=nullptr;
    const float parsed=std::strtof(text.c_str(),&end);
    if(end==text.c_str() || *end!='\0' || !std::isfinite(parsed)) return false;
    *value=parsed;
    return true;
}

bool parseInt(const std::string& text,int* value)
{
    if(!value) return false;
    char* end=nullptr;
    const long parsed=std::strtol(text.c_str(),&end,10);
    if(end==text.c_str() || *end!='\0' || parsed<std::numeric_limits<int>::min() || parsed>std::numeric_limits<int>::max()) return false;
    *value=static_cast<int>(parsed);
    return true;
}

float clamp01(float v){return std::max(0.0f,std::min(1.0f,v));}
Renderer::Math::Vec3 add(const Renderer::Math::Vec3&a,const Renderer::Math::Vec3&b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Renderer::Math::Vec3 sub(const Renderer::Math::Vec3&a,const Renderer::Math::Vec3&b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Renderer::Math::Vec3 cross(const Renderer::Math::Vec3&a,const Renderer::Math::Vec3&b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Renderer::Math::Vec3 normalize(const Renderer::Math::Vec3&v){float l=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);return l>1e-20f?Renderer::Math::Vec3{v.x/l,v.y/l,v.z/l}:Renderer::Math::Vec3{0,1,0};}

bool vec3From(const std::vector<std::string>& t,std::size_t start,Renderer::Math::Vec3* out)
{
    return out && t.size()>start+2 && parseFloat(t[start],&out->x) && parseFloat(t[start+1],&out->y) && parseFloat(t[start+2],&out->z);
}

int resolveIndex(int raw,std::size_t count)
{
    if(raw>0){long long idx=static_cast<long long>(raw)-1;return idx>=0 && idx<static_cast<long long>(count)?static_cast<int>(idx):-1;}
    if(raw<0){long long idx=static_cast<long long>(count)+raw;return idx>=0 && idx<static_cast<long long>(count)?static_cast<int>(idx):-1;}
    return -1;
}

bool parseRef(const std::string& text,std::size_t pc,std::size_t tc,std::size_t nc,Ref* out)
{
    if(!out) return false;
    std::array<std::string,3> f;
    std::size_t field=0;
    for(char c:text){if(c=='/' && field<2){++field;}else f[field].push_back(c);}
    int raw=0; if(f[0].empty() || !parseInt(f[0],&raw)) return false; out->position=resolveIndex(raw,pc); if(out->position<0) return false;
    if(!f[1].empty()){if(!parseInt(f[1],&raw)) return false;out->uv=resolveIndex(raw,tc);if(out->uv<0)return false;}
    if(!f[2].empty()){if(!parseInt(f[2],&raw)) return false;out->normal=resolveIndex(raw,nc);if(out->normal<0)return false;}
    return true;
}

std::filesystem::path resolvedPath(const std::filesystem::path& base,const std::string& value)
{
    std::filesystem::path p(value); if(p.is_relative()) p=base/p; return p.lexically_normal();
}

void parseVecOption(const std::vector<std::string>& t,std::size_t* index,Renderer::Math::Vec3* value)
{
    Renderer::Math::Vec3 parsed=*value; float* dst[3]={&parsed.x,&parsed.y,&parsed.z}; std::size_t n=0;
    while(*index<t.size() && n<3){float x=0; if(!parseFloat(t[*index],&x)) break; *dst[n++]=x; ++*index;}
    *value=parsed;
}

TextureRef textureRef(const std::vector<std::string>& t,const std::filesystem::path& base)
{
    TextureRef result; std::size_t i=1;
    while(i<t.size() && !t[i].empty() && t[i][0]=='-')
    {
        const std::string option=lower(t[i++]);
        if(option=="-o") parseVecOption(t,&i,&result.offset);
        else if(option=="-s") parseVecOption(t,&i,&result.scale);
        else if(option=="-t") parseVecOption(t,&i,&result.turbulence);
        else if(option=="-bm" && i<t.size()){float v=0;if(parseFloat(t[i],&v))result.bump_multiplier=v;++i;}
        else if(option=="-clamp" && i<t.size()){result.clamp=lower(t[i])=="on" || t[i]=="1";++i;}
        else if(option=="-imfchan" && i<t.size()){result.channel=t[i].empty()?'\0':t[i][0];++i;}
        else if((option=="-blendu"||option=="-blendv"||option=="-cc"||option=="-texres"||option=="-type") && i<t.size()) ++i;
        else if(option=="-mm"){if(i<t.size())++i;if(i<t.size())++i;}
    }
    if(i<t.size())
    {
        std::string path=t[i++]; while(i<t.size()){path.push_back(' ');path+=t[i++];}
        result.path=resolvedPath(base,path).string();
    }
    return result;
}

void setTexture(MaterialData* m,const std::string& key,const TextureRef& r)
{
    if(key=="map_kd")m->base_color_texture=r; else if(key=="map_ka")m->ambient_texture=r;
    else if(key=="map_ks")m->specular_texture=r; else if(key=="map_ke")m->emissive_texture=r;
    else if(key=="map_pm")m->metallic_texture=r; else if(key=="map_pr")m->roughness_texture=r;
    else if(key=="map_ns")m->shininess_texture=r; else if(key=="map_d")m->opacity_texture=r;
    else if(key=="norm"||key=="map_normal"||key=="map_norm")m->normal_texture=r;
    else if(key=="bump"||key=="map_bump")m->bump_texture=r;
    else if(key=="disp"||key=="map_disp")m->displacement_texture=r;
    else if(key=="refl"||key=="map_refl")m->reflection_texture=r;
    else if(key=="map_kt"||key=="map_pt")m->transmission_texture=r;
    else if(key=="map_pc")m->clearcoat_texture=r; else if(key=="map_pcr")m->clearcoat_roughness_texture=r;
    else if(key=="map_ps")m->sheen_texture=r; else if(key=="map_aniso")m->anisotropy_texture=r;
}

bool parseMtl(const std::filesystem::path& path,Document* doc,std::unordered_map<std::string,std::uint32_t>* indices)
{
    std::ifstream in(path); if(!in){doc->warnings.push_back("missing material library: "+path.string());return true;}
    const auto base=path.parent_path(); MaterialData* current=nullptr; std::string line;
    while(std::getline(in,line))
    {
        line=trim(line); if(line.empty()||line[0]=='#')continue; auto t=tokens(line); if(t.empty())continue; const std::string key=lower(t[0]);
        if(key=="newmtl")
        {
            std::string name=t.size()>1?t[1]:std::string{}; doc->materials.push_back({}); current=&doc->materials.back(); current->name=name;
            (*indices)[name]=static_cast<std::uint32_t>(doc->materials.size()-1u); continue;
        }
        if(!current) continue;
        if(key=="ka") vec3From(t,1,&current->ambient);
        else if(key=="kd") vec3From(t,1,&current->base_color);
        else if(key=="ks") vec3From(t,1,&current->specular);
        else if(key=="ke") vec3From(t,1,&current->emissive);
        else if(key=="tf"||key=="kt") vec3From(t,1,&current->transmission_color);
        else if(t.size()>1)
        {
            float v=0; int iv=0;
            if(key=="ns" && parseFloat(t[1],&v)){current->shininess=v;current->roughness=std::sqrt(2.0f/std::max(2.0f,v+2.0f));}
            else if(key=="ni" && parseFloat(t[1],&v))current->ior=v;
            else if(key=="d" && parseFloat(t[1],&v)){current->opacity=clamp01(v);current->transparency=1.0f-current->opacity;}
            else if(key=="tr" && parseFloat(t[1],&v)){current->transparency=clamp01(v);current->opacity=1.0f-current->transparency;}
            else if(key=="illum" && parseInt(t[1],&iv))current->illumination_model=iv;
            else if(key=="pr" && parseFloat(t[1],&v))current->roughness=clamp01(v);
            else if(key=="pm" && parseFloat(t[1],&v))current->metallic=clamp01(v);
            else if(key=="ps" && parseFloat(t[1],&v))current->sheen=clamp01(v);
            else if(key=="pc" && parseFloat(t[1],&v))current->clearcoat=clamp01(v);
            else if(key=="pcr" && parseFloat(t[1],&v))current->clearcoat_roughness=clamp01(v);
            else if((key=="aniso"||key=="anisotropy") && parseFloat(t[1],&v))current->anisotropy=v;
            else if((key=="pt"||key=="transmission"||key=="translucency") && parseFloat(t[1],&v))current->transmission=clamp01(v);
            else if((key=="kr"||key=="reflectivity") && parseFloat(t[1],&v))current->reflectivity=clamp01(v);
            else if((key=="specular"||key=="specular_strength") && parseFloat(t[1],&v))current->specular_strength=v;
            else if(key.rfind("map_",0)==0 || key=="bump"||key=="norm"||key=="disp"||key=="refl") setTexture(current,key,textureRef(t,base));
        }
    }
    return true;
}

struct SmoothKey { int position; int smoothing; bool operator==(const SmoothKey&o)const{return position==o.position&&smoothing==o.smoothing;} };
struct SmoothHash { std::size_t operator()(const SmoothKey&k)const{return (static_cast<std::size_t>(static_cast<unsigned>(k.position))*1315423911u)^static_cast<unsigned>(k.smoothing);} };
struct VertexKey { int p,t,n,s; std::size_t flat; bool operator==(const VertexKey&o)const{return p==o.p&&t==o.t&&n==o.n&&s==o.s&&flat==o.flat;} };
struct VertexHash { std::size_t operator()(const VertexKey&k)const{std::size_t h=static_cast<unsigned>(k.p);h=h*16777619u^static_cast<unsigned>(k.t+1);h=h*16777619u^static_cast<unsigned>(k.n+1);h=h*16777619u^static_cast<unsigned>(k.s);return h*16777619u^k.flat;} };

Renderer::Mesh::Bounds boundsFor(const std::vector<Renderer::Mesh::Vertex>& vertices)
{
    if(vertices.empty()) return {{0,0,0},{0,0,0}};
    const float m=std::numeric_limits<float>::max();
    Renderer::Mesh::Bounds b={{m,m,m},{-m,-m,-m}};
    for(const auto&v:vertices){b.minimum.x=std::min(b.minimum.x,v.position.x);b.minimum.y=std::min(b.minimum.y,v.position.y);b.minimum.z=std::min(b.minimum.z,v.position.z);b.maximum.x=std::max(b.maximum.x,v.position.x);b.maximum.y=std::max(b.maximum.y,v.position.y);b.maximum.z=std::max(b.maximum.z,v.position.z);} return b;
}

Submesh build(const RawPart& raw,const std::vector<Renderer::Math::Vec3>& positions,const std::vector<Renderer::Math::Vec2>& uvs,const std::vector<Renderer::Math::Vec3>& normals)
{
    Submesh out;out.object_name=raw.object_name;out.group_name=raw.group_name;out.material_name=raw.material_name;
    std::vector<Renderer::Math::Vec3> faceNormals(raw.triangles.size()); std::unordered_map<SmoothKey,Renderer::Math::Vec3,SmoothHash> smooth;
    for(std::size_t ti=0;ti<raw.triangles.size();++ti){const Triangle&t=raw.triangles[ti];auto n=normalize(cross(sub(positions[t.corners[1].position],positions[t.corners[0].position]),sub(positions[t.corners[2].position],positions[t.corners[0].position])));faceNormals[ti]=n;if(t.smoothing>0)for(const Ref&r:t.corners)if(r.normal<0)smooth[{r.position,t.smoothing}]=add(smooth[{r.position,t.smoothing}],n);}
    std::unordered_map<VertexKey,std::uint32_t,VertexHash> unique;
    for(std::size_t ti=0;ti<raw.triangles.size();++ti){const Triangle&t=raw.triangles[ti];for(const Ref&r:t.corners){VertexKey key={r.position,r.uv,r.normal,r.normal<0?t.smoothing:0,r.normal<0&&t.smoothing==0?ti+1u:0u};auto it=unique.find(key);if(it==unique.end()){Renderer::Math::Vec3 n=r.normal>=0?normals[r.normal]:(t.smoothing>0?normalize(smooth[{r.position,t.smoothing}]):faceNormals[ti]);Renderer::Math::Vec2 uv=r.uv>=0?uvs[r.uv]:Renderer::Math::Vec2{0,0};std::uint32_t idx=static_cast<std::uint32_t>(out.mesh.vertices.size());out.mesh.vertices.push_back({positions[r.position],n,uv});unique.emplace(key,idx);out.mesh.indices.push_back(idx);}else out.mesh.indices.push_back(it->second);}}
    out.mesh.bounds=boundsFor(out.mesh.vertices);return out;
}

} // namespace

bool load(const std::string& path,Document* document,std::string* error)
{
    if(error) error->clear();
    if(!document)
    {
        if(error) *error="null OBJ destination";
        return false;
    }
    *document={};
    std::ifstream in(path);
    if(!in)
    {
        if(error) *error="cannot open OBJ: "+path;
        return false;
    }
    std::vector<Renderer::Math::Vec3> positions,normals; std::vector<Renderer::Math::Vec2> uvs; std::vector<RawPart> rawParts; std::vector<std::filesystem::path> mtllibs;
    std::string objectName,groupName,materialName; int smoothing=0; std::string line; std::size_t lineNumber=0;
    auto part=[&]()->RawPart&{if(rawParts.empty()||rawParts.back().object_name!=objectName||rawParts.back().group_name!=groupName||rawParts.back().material_name!=materialName)rawParts.push_back({objectName,groupName,materialName,{}});return rawParts.back();};
    while(std::getline(in,line))
    {
        ++lineNumber; line=trim(line); if(line.empty()||line[0]=='#')continue; auto t=tokens(line); if(t.empty())continue; const std::string key=lower(t[0]);
        if(key=="v"){Renderer::Math::Vec3 v{};if(!vec3From(t,1,&v)){if(error)*error="invalid vertex at OBJ line "+std::to_string(lineNumber);return false;}positions.push_back(v);}
        else if(key=="vn"){Renderer::Math::Vec3 v{};if(!vec3From(t,1,&v)){if(error)*error="invalid normal at OBJ line "+std::to_string(lineNumber);return false;}normals.push_back(normalize(v));}
        else if(key=="vt"){Renderer::Math::Vec2 v{};if(t.size()<3||!parseFloat(t[1],&v.x)||!parseFloat(t[2],&v.y)){if(error)*error="invalid texcoord at OBJ line "+std::to_string(lineNumber);return false;}uvs.push_back(v);}
        else if(key=="o") objectName=t.size()>1?t[1]:std::string{};
        else if(key=="g") groupName=t.size()>1?t[1]:std::string{};
        else if(key=="usemtl") materialName=t.size()>1?t[1]:std::string{};
        else if(key=="s"){if(t.size()<2||lower(t[1])=="off"||t[1]=="0")smoothing=0;else if(!parseInt(t[1],&smoothing))smoothing=1;}
        else if(key=="mtllib"){for(std::size_t i=1;i<t.size();++i)mtllibs.push_back(resolvedPath(std::filesystem::path(path).parent_path(),t[i]));}
        else if(key=="f")
        {
            if(t.size()<4){if(error)*error="face has fewer than three vertices at OBJ line "+std::to_string(lineNumber);return false;} std::vector<Ref> refs;refs.reserve(t.size()-1);
            for(std::size_t i=1;i<t.size();++i){Ref r;if(!parseRef(t[i],positions.size(),uvs.size(),normals.size(),&r)){if(error)*error="invalid face index at OBJ line "+std::to_string(lineNumber);return false;}refs.push_back(r);}
            RawPart&p=part();for(std::size_t i=1;i+1<refs.size();++i)p.triangles.push_back({{refs[0],refs[i],refs[i+1]},smoothing});
        }
    }
    std::unordered_map<std::string,std::uint32_t> materialIndices; for(const auto&m:mtllibs)parseMtl(m,document,&materialIndices);
    for(const RawPart&r:rawParts){if(r.triangles.empty())continue;Submesh s=build(r,positions,uvs,normals);auto it=materialIndices.find(s.material_name);if(it!=materialIndices.end())s.material_index=it->second;else if(!s.material_name.empty())document->warnings.push_back("material not found: "+s.material_name);document->submeshes.push_back(std::move(s));}
    if(document->submeshes.empty()){if(error)*error="OBJ contains no renderable faces: "+path;return false;} return true;
}

} }
