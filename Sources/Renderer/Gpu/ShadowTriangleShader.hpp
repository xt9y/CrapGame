#ifndef CRAPGAME_RENDERER_GPU_SHADOWTRIANGLESHADER_HPP
#define CRAPGAME_RENDERER_GPU_SHADOWTRIANGLESHADER_HPP

namespace Renderer
{
namespace Gpu
{

constexpr const char *IMPORTED_SHADOW_TRIANGLE_GLSL=R"GLSL(
struct ImportedShadowTriangle{vec4 p0;vec4 p1;vec4 p2;};
layout(std430,binding=8) readonly buffer ImportedShadowTriangleBuffer{
    ImportedShadowTriangle importedShadowTriangles[];
};

bool traceShadowTriangleAny(ImportedShadowTriangle triangle,vec3 ro,vec3 rd,float maximumDistance){
    vec3 e1=triangle.p1.xyz-triangle.p0.xyz;
    vec3 e2=triangle.p2.xyz-triangle.p0.xyz;
    vec3 p=cross(rd,e2);float det=dot(e1,p);
    if(abs(det)<1e-8)return false;
    float invDet=1.0/det;vec3 s=ro-triangle.p0.xyz;
    float u=dot(s,p)*invDet;if(u<0.0||u>1.0)return false;
    vec3 q=cross(s,e1);float v=dot(rd,q)*invDet;if(v<0.0||u+v>1.0)return false;
    float t=dot(e2,q)*invDet;
    return t>0.00002&&t<maximumDistance;
}

bool traceImportedShadowInstanceAny(int instanceIndex,vec3 worldOrigin,
                                    vec3 worldDirection,float maximumDistance){
    if(instanceIndex<0||instanceIndex>=uImportedInstanceCount)return false;
    ImportedInstance instance=importedInstances[instanceIndex];
    ImportedMesh mesh=importedMeshes[instance.meshIndex];
    if(mesh.nodeCount==0u)return false;
    vec3 ro=(instance.inverseModel*vec4(worldOrigin,1.0)).xyz;
    vec3 rd=(instance.inverseModel*vec4(worldDirection,0.0)).xyz;
    int stack[64];int stackSize=1;stack[0]=int(mesh.nodeOffset);
    while(stackSize>0){
        int nodeIndex=stack[--stackSize];
        if(nodeIndex<int(mesh.nodeOffset)||nodeIndex>=int(mesh.nodeOffset+mesh.nodeCount))continue;
        TraceBvhNode node=importedBlasNodes[nodeIndex];
        if(!traceAabb(ro,rd,node.boundsMinimum.xyz,node.boundsMaximum.xyz,maximumDistance))continue;
        if(node.meta.w>0){
            for(int j=0;j<node.meta.w;++j){
                int localTriangle=j==0?node.meta.x:(j==1?node.meta.y:node.meta.z);
                if(localTriangle<0||localTriangle>=int(mesh.triangleCount))continue;
                if(traceShadowTriangleAny(
                    importedShadowTriangles[int(mesh.triangleOffset)+localTriangle],
                    ro,rd,maximumDistance))return true;
            }
        }else if(stackSize<=61){stack[stackSize++]=node.meta.x;stack[stackSize++]=node.meta.y;}
    }
    return false;
}
)GLSL";

} // namespace Gpu
} // namespace Renderer

#endif
