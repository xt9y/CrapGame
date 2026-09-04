#include "Renderer/Mesh/Tangent.hpp"

#include "Models/Texture.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <vector>

namespace Renderer
{
namespace Mesh
{
namespace
{
constexpr float EPSILON = 1.0e-8f;

Math::Vec3 fallbackTangent(const Math::Vec3& normal)
{
    const Math::Vec3 n = Math::normalize(normal);
    const Math::Vec3 axis = std::fabs(n.x) < std::fabs(n.y)
        ? (std::fabs(n.x) < std::fabs(n.z) ? Math::Vec3{1,0,0} : Math::Vec3{0,0,1})
        : (std::fabs(n.y) < std::fabs(n.z) ? Math::Vec3{0,1,0} : Math::Vec3{0,0,1});
    Math::Vec3 t = Math::normalize(Math::cross(axis,n));
    if (Math::lengthSquared(t) <= EPSILON) t = {1,0,0};
    return t;
}

float wrap(float v)
{
    v = v - std::floor(v);
    return v < 0.0f ? v + 1.0f : v;
}

float channelAt(const Models::TextureAsset& asset, int x, int y, char channel)
{
    x = std::max(0,std::min(asset.image.width-1,x));
    y = std::max(0,std::min(asset.image.height-1,y));
    const std::size_t i=(static_cast<std::size_t>(y)*asset.image.width+x)*4u;
    const char c=static_cast<char>(std::tolower(static_cast<unsigned char>(channel)));
    if(c=='g') return asset.image.rgba[i+1]/255.0f;
    if(c=='b') return asset.image.rgba[i+2]/255.0f;
    if(c=='a') return asset.image.rgba[i+3]/255.0f;
    if(c=='m') return (asset.image.rgba[i]+asset.image.rgba[i+1]+asset.image.rgba[i+2])/(3.0f*255.0f);
    return asset.image.rgba[i]/255.0f;
}

float sample(const Models::TextureAsset& asset, Math::Vec2 uv, const Material::TextureBinding& b)
{
    float u=uv.x*b.scale.x+b.offset.x+b.turbulence.x;
    float v=uv.y*b.scale.y+b.offset.y+b.turbulence.y;
    if(b.clamp){u=std::max(0.0f,std::min(1.0f,u));v=std::max(0.0f,std::min(1.0f,v));}
    else {u=wrap(u);v=wrap(v);}
    const float fx=u*static_cast<float>(std::max(1,asset.image.width-1));
    const float fy=v*static_cast<float>(std::max(1,asset.image.height-1));
    const int x0=static_cast<int>(std::floor(fx)), y0=static_cast<int>(std::floor(fy));
    const int x1=std::min(asset.image.width-1,x0+1), y1=std::min(asset.image.height-1,y0+1);
    const float tx=fx-x0, ty=fy-y0;
    const float a=channelAt(asset,x0,y0,b.channel)*(1.0f-tx)+channelAt(asset,x1,y0,b.channel)*tx;
    const float d=channelAt(asset,x0,y1,b.channel)*(1.0f-tx)+channelAt(asset,x1,y1,b.channel)*tx;
    return a*(1.0f-ty)+d*ty;
}

void recomputeNormals(MeshData *mesh)
{
    std::vector<Math::Vec3> sums(mesh->vertices.size(),{0,0,0});
    for(std::size_t i=0;i+2<mesh->indices.size();i+=3){
        const auto a=mesh->indices[i],b=mesh->indices[i+1],c=mesh->indices[i+2];
        if(a>=mesh->vertices.size()||b>=mesh->vertices.size()||c>=mesh->vertices.size()) continue;
        const Math::Vec3 e1=Math::subtract(mesh->vertices[b].position,mesh->vertices[a].position);
        const Math::Vec3 e2=Math::subtract(mesh->vertices[c].position,mesh->vertices[a].position);
        const Math::Vec3 n=Math::cross(e1,e2);
        sums[a]=Math::add(sums[a],n); sums[b]=Math::add(sums[b],n); sums[c]=Math::add(sums[c],n);
    }
    for(std::size_t i=0;i<mesh->vertices.size();++i){const auto n=Math::normalize(sums[i]); if(Math::lengthSquared(n)>EPSILON) mesh->vertices[i].normal=n;}
}

void recomputeBounds(MeshData *mesh)
{
    if(mesh->vertices.empty()){mesh->bounds={{0,0,0},{0,0,0}};return;}
    const float m=std::numeric_limits<float>::max(); mesh->bounds={{m,m,m},{-m,-m,-m}};
    for(const auto& v:mesh->vertices){mesh->bounds.minimum.x=std::min(mesh->bounds.minimum.x,v.position.x);mesh->bounds.minimum.y=std::min(mesh->bounds.minimum.y,v.position.y);mesh->bounds.minimum.z=std::min(mesh->bounds.minimum.z,v.position.z);mesh->bounds.maximum.x=std::max(mesh->bounds.maximum.x,v.position.x);mesh->bounds.maximum.y=std::max(mesh->bounds.maximum.y,v.position.y);mesh->bounds.maximum.z=std::max(mesh->bounds.maximum.z,v.position.z);}
}
}

void generateTangents(MeshData *mesh)
{
    if(!mesh) return;
    std::vector<Math::Vec3> tan(mesh->vertices.size(),{0,0,0}), bitan(mesh->vertices.size(),{0,0,0});
    for(std::size_t i=0;i+2<mesh->indices.size();i+=3){
        const auto ia=mesh->indices[i],ib=mesh->indices[i+1],ic=mesh->indices[i+2]; if(ia>=mesh->vertices.size()||ib>=mesh->vertices.size()||ic>=mesh->vertices.size()) continue;
        const auto& a=mesh->vertices[ia]; const auto& b=mesh->vertices[ib]; const auto& c=mesh->vertices[ic];
        const Math::Vec3 e1=Math::subtract(b.position,a.position), e2=Math::subtract(c.position,a.position);
        const float du1=b.uv.x-a.uv.x,dv1=b.uv.y-a.uv.y,du2=c.uv.x-a.uv.x,dv2=c.uv.y-a.uv.y;
        const float det=du1*dv2-du2*dv1; Math::Vec3 t{},bt{};
        if(std::fabs(det)>EPSILON){const float r=1.0f/det;t=Math::multiply(Math::subtract(Math::multiply(e1,dv2),Math::multiply(e2,dv1)),r);bt=Math::multiply(Math::subtract(Math::multiply(e2,du1),Math::multiply(e1,du2)),r);} else {t=fallbackTangent(a.normal);bt=Math::cross(a.normal,t);}
        for(auto idx:{ia,ib,ic}){tan[idx]=Math::add(tan[idx],t);bitan[idx]=Math::add(bitan[idx],bt);}
    }
    for(std::size_t i=0;i<mesh->vertices.size();++i){const Math::Vec3 n=Math::normalize(mesh->vertices[i].normal);Math::Vec3 t=Math::subtract(tan[i],Math::multiply(n,Math::dot(n,tan[i])));t=Math::normalize(t);if(Math::lengthSquared(t)<=EPSILON)t=fallbackTangent(n);const float sign=Math::dot(Math::cross(n,t),bitan[i])<0.0f?-1.0f:1.0f;mesh->vertices[i].tangent={t.x,t.y,t.z,sign};}
}

bool applyDisplacement(MeshData *mesh,const Material::TextureBinding& binding,float strength,std::string *error)
{
    if (error) error->clear();
    if (!mesh)
    {
        if (error) *error = "null displacement mesh";
        return false;
    }
    const Models::TextureAsset *asset = Models::texture(binding.texture);
    if (!asset || asset->image.width <= 0 || asset->image.height <= 0)
    {
        if (error) *error = "invalid displacement texture";
        return false;
    }
    const float amount=strength*binding.multiplier;
    for(auto& v:mesh->vertices){const float h=sample(*asset,v.uv,binding);v.position=Math::add(v.position,Math::multiply(Math::normalize(v.normal),h*amount));}
    recomputeNormals(mesh); generateTangents(mesh); recomputeBounds(mesh); return true;
}

} // namespace Mesh
} // namespace Renderer
