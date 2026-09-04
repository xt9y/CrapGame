#ifndef CRAPGAME_RENDERER_GPU_IMPORTEDBVHTRAVERSALPATCH_HPP
#define CRAPGAME_RENDERER_GPU_IMPORTEDBVHTRAVERSALPATCH_HPP

#include <stdexcept>
#include <string>

namespace Renderer
{
namespace Gpu
{

inline std::size_t importedBvhFunctionEnd(
            const std::string& source,
            std::size_t function_at)
{
    const std::size_t open_brace=source.find('{',function_at);
    if(open_brace==std::string::npos)
        throw std::runtime_error("imported BVH function body missing");

    int depth=0;
    for(std::size_t i=open_brace;i<source.size();++i)
    {
        if(source[i]=='{')++depth;
        else if(source[i]=='}')
        {
            --depth;
            if(depth==0)return i+1u;
        }
    }
    throw std::runtime_error("imported BVH function body is unterminated");
}

inline void patchTraversalFunction(
            std::string *source,
            const char *function_token,
            const char *buffer_name,
            const char *distance_expression,
            bool required = true)
{
    if(!source||!function_token||!buffer_name||!distance_expression)
        throw std::runtime_error("invalid imported BVH traversal patch");

    const std::size_t function_at=source->find(function_token);
    if(function_at==std::string::npos)
    {
        if(required)throw std::runtime_error("imported BVH traversal function missing");
        return;
    }
    const std::size_t function_end=importedBvhFunctionEnd(*source,function_at);

    const std::string old_push=
        "}else if(stackSize<=61){stack[stackSize++]=node.meta.x;stack[stackSize++]=node.meta.y;}";
    const std::size_t push_at=source->find(old_push,function_at);
    if(push_at==std::string::npos||push_at>=function_end)
        throw std::runtime_error("imported BVH child push missing");

    const std::string replacement=
        std::string("}else if(stackSize<=61){")
        +"int leftIndex=node.meta.x,rightIndex=node.meta.y;"
        +"TraceBvhNode leftNode="+buffer_name+"[leftIndex],rightNode="+buffer_name+"[rightIndex];"
        +"float leftEntry=0.0,rightEntry=0.0;"
        +"bool leftHit=traceAabbEntry(ro,rd,leftNode.boundsMinimum.xyz,leftNode.boundsMaximum.xyz,"
        +distance_expression+",leftEntry);"
        +"bool rightHit=traceAabbEntry(ro,rd,rightNode.boundsMinimum.xyz,rightNode.boundsMaximum.xyz,"
        +distance_expression+",rightEntry);"
        +"if(leftHit&&rightHit){if(leftEntry<=rightEntry){stack[stackSize++]=rightIndex;"
        +"stack[stackSize++]=leftIndex;}else{stack[stackSize++]=leftIndex;"
        +"stack[stackSize++]=rightIndex;}}else if(leftHit){stack[stackSize++]=leftIndex;}"
        +"else if(rightHit){stack[stackSize++]=rightIndex;}}";
    source->replace(push_at,old_push.size(),replacement);
}

inline void patchImportedBvhTraversal(std::string *source)
{
    if(!source)throw std::runtime_error("null imported BVH shader");

    const std::string aabb_signature=
        "bool traceAabb(vec3 ro,vec3 rd,vec3 mn,vec3 mx,float maximumDistance)";
    const std::size_t aabb_at=source->find(aabb_signature);
    if(aabb_at==std::string::npos)
        throw std::runtime_error("imported BVH AABB patch point missing");
    const std::size_t aabb_end=importedBvhFunctionEnd(*source,aabb_at);

    const std::string new_aabb=
        "bool traceAabbEntry(vec3 ro,vec3 rd,vec3 mn,vec3 mx,float maximumDistance,out float entryDistance){"
        "vec3 inv=vec3(abs(rd.x)>1e-9?1.0/rd.x:1e30,abs(rd.y)>1e-9?1.0/rd.y:1e30,abs(rd.z)>1e-9?1.0/rd.z:1e30);"
        "vec3 a=(mn-ro)*inv,b=(mx-ro)*inv,lo=min(a,b),hi=max(a,b);"
        "float nearT=max(max(lo.x,lo.y),lo.z),farT=min(min(hi.x,hi.y),hi.z);"
        "entryDistance=max(nearT,0.0);return farT>=entryDistance&&nearT<maximumDistance;}"
        "bool traceAabb(vec3 ro,vec3 rd,vec3 mn,vec3 mx,float maximumDistance){"
        "float entryDistance=0.0;return traceAabbEntry(ro,rd,mn,mx,maximumDistance,entryDistance);}";
    source->replace(aabb_at,aabb_end-aabb_at,new_aabb);

    patchTraversalFunction(source,"bool traceImportedInstanceAny(","importedBlasNodes","maximumDistance");
    patchTraversalFunction(source,"bool traceImportedInstance(","importedBlasNodes","best");
    patchTraversalFunction(source,"bool traceImportedOpaqueAny(","importedTlasNodes","maximumDistance");
    patchTraversalFunction(source,"bool traceImportedNearest(","importedTlasNodes","hitDistance");
    patchTraversalFunction(source,"bool traceImportedShadowInstanceAny(","importedBlasNodes","maximumDistance",false);
}

} // namespace Gpu
} // namespace Renderer

#endif
