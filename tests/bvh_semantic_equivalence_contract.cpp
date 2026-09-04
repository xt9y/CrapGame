#include "Renderer/Gpu/Bvh.hpp"
#include "Renderer/Gpu/ImportedBvhTraversalPatch.hpp"

#include <cassert>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

using namespace Renderer::Gpu;

namespace
{
struct Vec3{float x,y,z;};

BvhBoundsInput box(float x0,float y0,float z0,float x1,float y1,float z1,std::uint32_t id)
{
    BvhBoundsInput b={};
    b.minimum[0]=x0;b.minimum[1]=y0;b.minimum[2]=z0;
    b.maximum[0]=x1;b.maximum[1]=y1;b.maximum[2]=z1;
    b.primitive_index=id;
    return b;
}

bool hit(const float mn[3],const float mx[3],Vec3 ro,Vec3 rd,float limit,float *entry)
{
    float near_t=-FLT_MAX,far_t=FLT_MAX;
    const float origin[3]={ro.x,ro.y,ro.z};
    const float direction[3]={rd.x,rd.y,rd.z};
    for(int axis=0;axis<3;++axis)
    {
        if(std::fabs(direction[axis])<1.0e-8f)
        {
            if(origin[axis]<mn[axis]||origin[axis]>mx[axis])return false;
            continue;
        }
        float t0=(mn[axis]-origin[axis])/direction[axis];
        float t1=(mx[axis]-origin[axis])/direction[axis];
        if(t0>t1){const float swap=t0;t0=t1;t1=swap;}
        near_t=std::max(near_t,t0);far_t=std::min(far_t,t1);
    }
    if(far_t<0.0f||near_t>far_t||near_t>=limit)return false;
    *entry=std::max(near_t,0.0f);
    return true;
}

float brute(const std::vector<BvhBoundsInput>& bounds,Vec3 ro,Vec3 rd)
{
    float best=1.0e30f;
    for(const BvhBoundsInput& item:bounds)
    {
        float entry=0.0f;
        if(hit(item.minimum,item.maximum,ro,rd,best,&entry)&&entry<best)best=entry;
    }
    return best;
}

float traverse(const BvhBuild& build,const std::vector<BvhBoundsInput>& bounds,Vec3 ro,Vec3 rd)
{
    if(build.nodes.empty())return 1.0e30f;
    int stack[128];int stack_size=1;stack[0]=0;float best=1.0e30f;
    while(stack_size>0)
    {
        const int node_index=stack[--stack_size];
        const BvhNodeGpu& node=build.nodes[static_cast<std::size_t>(node_index)];
        float node_entry=0.0f;
        if(!hit(node.bounds_minimum,node.bounds_maximum,ro,rd,best,&node_entry))continue;
        if(node.meta[3]>0)
        {
            for(int slot=0;slot<node.meta[3];++slot)
            {
                const BvhBoundsInput& item=bounds[static_cast<std::size_t>(node.meta[slot])];
                float entry=0.0f;
                if(hit(item.minimum,item.maximum,ro,rd,best,&entry)&&entry<best)best=entry;
            }
            continue;
        }
        const int left=node.meta[0],right=node.meta[1];
        float left_entry=0.0f,right_entry=0.0f;
        const bool left_hit=hit(build.nodes[static_cast<std::size_t>(left)].bounds_minimum,
                                build.nodes[static_cast<std::size_t>(left)].bounds_maximum,
                                ro,rd,best,&left_entry);
        const bool right_hit=hit(build.nodes[static_cast<std::size_t>(right)].bounds_minimum,
                                 build.nodes[static_cast<std::size_t>(right)].bounds_maximum,
                                 ro,rd,best,&right_entry);
        if(left_hit&&right_hit)
        {
            if(left_entry<=right_entry){stack[stack_size++]=right;stack[stack_size++]=left;}
            else{stack[stack_size++]=left;stack[stack_size++]=right;}
        }
        else if(left_hit)stack[stack_size++]=left;
        else if(right_hit)stack[stack_size++]=right;
    }
    return best;
}

void verifyTraversalPatch()
{
    const char *aabb=
        "bool traceAabb(vec3 ro,vec3 rd,vec3 mn,vec3 mx,float maximumDistance){"
        "vec3 inv=vec3(abs(rd.x)>1e-9?1.0/rd.x:1e30,abs(rd.y)>1e-9?1.0/rd.y:1e30,abs(rd.z)>1e-9?1.0/rd.z:1e30);"
        "vec3 a=(mn-ro)*inv,b=(mx-ro)*inv,lo=min(a,b),hi=max(a,b);"
        "float nearT=max(max(lo.x,lo.y),lo.z),farT=min(min(hi.x,hi.y),hi.z);"
        "return farT>=max(nearT,0.0)&&nearT<maximumDistance;}";
    const char *push="}else if(stackSize<=61){stack[stackSize++]=node.meta.x;stack[stackSize++]=node.meta.y;}";
    std::string shader=aabb;
    shader+="bool traceImportedInstanceAny(){if(x){x();"+std::string(push)+"}}";
    shader+="bool traceImportedInstance(){if(x){x();"+std::string(push)+"}}";
    shader+="bool traceImportedOpaqueAny(){if(x){x();"+std::string(push)+"}}";
    shader+="bool traceImportedNearest(){if(x){x();"+std::string(push)+"}}";
    patchImportedBvhTraversal(&shader);
    assert(shader.find("traceAabbEntry")!=std::string::npos);
    assert(shader.find("leftEntry<=rightEntry")!=std::string::npos);
    assert(shader.find(push)==std::string::npos);
}
} // namespace

int main()
{
    std::vector<BvhBoundsInput> bounds;
    for(std::uint32_t i=0;i<31u;++i)
    {
        const float x=static_cast<float>((i*7u)%17u)-8.0f;
        const float y=static_cast<float>((i*5u)%11u)-5.0f;
        const float z=static_cast<float>((i*3u)%13u)-6.0f;
        bounds.push_back(box(x,y,z,x+0.35f+static_cast<float>(i%3u)*0.1f,y+0.4f,z+0.3f,i));
    }

    const BvhBuild build=buildBvhSah(bounds,3u,16u);
    const Vec3 origins[]={{0,0,-20},{-15,2,0},{12,-4,8},{0,15,0},{2,1,20}};
    const Vec3 directions[]={{0,0,1},{1,-0.02f,0.03f},{-1,0.1f,-0.2f},{0,-1,0},{-0.1f,0,-1}};
    for(int i=0;i<5;++i)
    {
        const float expected=brute(bounds,origins[i],directions[i]);
        const float actual=traverse(build,bounds,origins[i],directions[i]);
        assert((expected>1.0e20f&&actual>1.0e20f)||std::fabs(expected-actual)<1.0e-5f);
    }

    std::vector<BvhBoundsInput> moved=bounds;
    for(BvhBoundsInput& item:moved){item.minimum[1]+=0.25f;item.maximum[1]+=0.25f;}
    BvhBuild refitted=build;
    assert(refitBvh(&refitted.nodes,moved));
    for(int i=0;i<5;++i)
    {
        const float expected=brute(moved,origins[i],directions[i]);
        const float actual=traverse(refitted,moved,origins[i],directions[i]);
        assert((expected>1.0e20f&&actual>1.0e20f)||std::fabs(expected-actual)<1.0e-5f);
    }

    verifyTraversalPatch();
    return 0;
}
