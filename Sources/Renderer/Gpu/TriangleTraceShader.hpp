#ifndef CRAPGAME_RENDERER_GPU_TRIANGLETRACESHADER_HPP
#define CRAPGAME_RENDERER_GPU_TRIANGLETRACESHADER_HPP

namespace Renderer { namespace Gpu {

/* Shared imported-mesh ray representation. Binding indices intentionally fit
 * the OpenGL 4.3 minimum of eight SSBO bindings. Direct lighting uses 0..7;
 * Lumen uses the same imported slots and reuses the remaining slots for its
 * analytic primitive buffer. */
constexpr const char *IMPORTED_TRIANGLE_TRACE_GLSL = R"GLSL(
struct ImportedTriangle {
    vec4 p0; vec4 p1; vec4 p2;
    vec4 uv0Uv1; vec4 uv2Material;
    vec4 n0; vec4 n1; vec4 n2;
};
struct ImportedMesh { uint triangleOffset; uint triangleCount; uint nodeOffset; uint nodeCount; };
struct ImportedInstance { mat4 inverseModel; uint meshIndex; uint materialHandle; uint flags; uint padding; };
struct TraceBvhNode { vec4 boundsMinimum; vec4 boundsMaximum; ivec4 meta; };
struct TraceRecord {
    vec4 baseMetallic;
    vec4 emissiveRoughness;
    vec4 specularIor;
    vec4 advanced;
    vec4 transmission;
    vec4 extra;
    ivec4 textureIndices[4];
};
layout(std430,binding=0) readonly buffer ImportedTriangleBuffer { ImportedTriangle importedTriangles[]; };
layout(std430,binding=1) readonly buffer ImportedMeshBuffer { ImportedMesh importedMeshes[]; };
layout(std430,binding=2) readonly buffer ImportedInstanceBuffer { ImportedInstance importedInstances[]; };
layout(std430,binding=3) readonly buffer ImportedBlasBuffer { TraceBvhNode importedBlasNodes[]; };
layout(std430,binding=4) readonly buffer ImportedTlasBuffer { TraceBvhNode importedTlasNodes[]; };
layout(std430,binding=7) readonly buffer TraceRecordBuffer { TraceRecord traceRecords[]; };
layout(binding=4) uniform sampler2DArray sTraceColorAtlas;
layout(binding=5) uniform sampler2DArray sTraceDataAtlas;
uniform int uImportedInstanceCount;
uniform int uImportedTlasNodeCount;
uniform int uTraceMaterialCount;

const int TRACE_SLOT_BASE_COLOR=0;
const int TRACE_SLOT_SPECULAR=2;
const int TRACE_SLOT_EMISSIVE=3;
const int TRACE_SLOT_METALLIC=4;
const int TRACE_SLOT_ROUGHNESS=5;
const int TRACE_SLOT_OPACITY=6;
const int TRACE_SLOT_REFLECTION=9;
const int TRACE_SLOT_TRANSMISSION=10;

int traceTextureIndex(TraceRecord material,int slot){
    ivec4 q=material.textureIndices[slot>>2];
    int c=slot&3;
    return c==0?q.x:(c==1?q.y:(c==2?q.z:q.w));
}
float traceChannel(vec4 value,int channel){
    if(channel==1)return value.g;
    if(channel==2)return value.b;
    if(channel==3)return value.a;
    if(channel==4)return dot(value.rgb,vec3(1.0/3.0));
    return value.r;
}
vec4 traceTexture(int descriptorIndex,vec2 uv){
    if(descriptorIndex<0)return vec4(1.0);
    TraceRecord d=traceRecords[descriptorIndex];
    vec2 tuv=uv*d.emissiveRoughness.xy+d.emissiveRoughness.zw;
    ivec4 meta=d.textureIndices[0];
    tuv=meta.w!=0?clamp(tuv,vec2(0.0),vec2(1.0)):fract(tuv);
    vec2 atlasUv=d.baseMetallic.xy+tuv*d.baseMetallic.zw;
    return meta.y==0
        ? texture(sTraceColorAtlas,vec3(atlasUv,float(meta.x)))
        : texture(sTraceDataAtlas,vec3(atlasUv,float(meta.x)));
}
float traceScalar(TraceRecord material,int slot,vec2 uv,float fallbackValue){
    int descriptor=traceTextureIndex(material,slot);
    if(descriptor<0)return fallbackValue;
    TraceRecord d=traceRecords[descriptor];
    return traceChannel(traceTexture(descriptor,uv),d.textureIndices[0].z);
}
vec3 traceColor(TraceRecord material,int slot,vec2 uv,vec3 fallbackValue){
    int descriptor=traceTextureIndex(material,slot);
    return descriptor<0?fallbackValue:fallbackValue*traceTexture(descriptor,uv).rgb;
}

bool traceAabb(vec3 ro,vec3 rd,vec3 mn,vec3 mx,float maximumDistance){
    vec3 inv=vec3(abs(rd.x)>1e-9?1.0/rd.x:1e30,abs(rd.y)>1e-9?1.0/rd.y:1e30,abs(rd.z)>1e-9?1.0/rd.z:1e30);
    vec3 a=(mn-ro)*inv,b=(mx-ro)*inv,lo=min(a,b),hi=max(a,b);
    float nearT=max(max(lo.x,lo.y),lo.z),farT=min(min(hi.x,hi.y),hi.z);
    return farT>=max(nearT,0.0)&&nearT<maximumDistance;
}
bool traceTriangleAny(ImportedTriangle triangle,vec3 ro,vec3 rd,float maximumDistance){
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
bool traceTriangleRaw(ImportedTriangle triangle,vec3 ro,vec3 rd,float maximumDistance,
                      out float distanceValue,out vec2 uv,out vec3 localNormal){
    vec3 e1=triangle.p1.xyz-triangle.p0.xyz;
    vec3 e2=triangle.p2.xyz-triangle.p0.xyz;
    vec3 p=cross(rd,e2);float det=dot(e1,p);
    if(abs(det)<1e-8)return false;
    float invDet=1.0/det;vec3 s=ro-triangle.p0.xyz;
    float u=dot(s,p)*invDet;if(u<0.0||u>1.0)return false;
    vec3 q=cross(s,e1);float v=dot(rd,q)*invDet;if(v<0.0||u+v>1.0)return false;
    float t=dot(e2,q)*invDet;if(t<=0.00002||t>=maximumDistance)return false;
    float w=1.0-u-v;
    vec2 uv0=triangle.uv0Uv1.xy,uv1=triangle.uv0Uv1.zw,uv2=triangle.uv2Material.xy;
    uv=uv0*w+uv1*u+uv2*v;
    localNormal=normalize(triangle.n0.xyz*w+triangle.n1.xyz*u+triangle.n2.xyz*v);
    distanceValue=t;return true;
}

bool traceImportedInstanceAny(int instanceIndex,vec3 worldOrigin,vec3 worldDirection,float maximumDistance){
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
                if(traceTriangleAny(importedTriangles[int(mesh.triangleOffset)+localTriangle],ro,rd,maximumDistance))return true;
            }
        }else if(stackSize<=61){stack[stackSize++]=node.meta.x;stack[stackSize++]=node.meta.y;}
    }
    return false;
}

bool traceImportedInstance(int instanceIndex,vec3 worldOrigin,vec3 worldDirection,float maximumDistance,
                           out float hitDistance,out vec2 hitUv,out vec3 hitNormal,out int materialHandle){
    if(instanceIndex<0||instanceIndex>=uImportedInstanceCount)return false;
    ImportedInstance instance=importedInstances[instanceIndex];
    ImportedMesh mesh=importedMeshes[instance.meshIndex];
    vec3 ro=(instance.inverseModel*vec4(worldOrigin,1.0)).xyz;
    vec3 rd=(instance.inverseModel*vec4(worldDirection,0.0)).xyz;
    if(mesh.nodeCount==0u)return false;
    float best=maximumDistance;vec2 bestUv=vec2(0.0);vec3 bestNormal=vec3(0.0);bool found=false;
    int stack[64];int stackSize=1;stack[0]=int(mesh.nodeOffset);
    while(stackSize>0){
        int nodeIndex=stack[--stackSize];
        if(nodeIndex<int(mesh.nodeOffset)||nodeIndex>=int(mesh.nodeOffset+mesh.nodeCount))continue;
        TraceBvhNode node=importedBlasNodes[nodeIndex];
        if(!traceAabb(ro,rd,node.boundsMinimum.xyz,node.boundsMaximum.xyz,best))continue;
        if(node.meta.w>0){
            for(int j=0;j<node.meta.w;++j){
                int localTriangle=j==0?node.meta.x:(j==1?node.meta.y:node.meta.z);
                if(localTriangle<0||localTriangle>=int(mesh.triangleCount))continue;
                ImportedTriangle triangle=importedTriangles[int(mesh.triangleOffset)+localTriangle];
                float t;vec2 uv;vec3 localNormal;
                if(traceTriangleRaw(triangle,ro,rd,best,t,uv,localNormal)){
                    found=true;best=t;bestUv=uv;
                    bestNormal=normalize(transpose(mat3(instance.inverseModel))*localNormal);
                }
            }
        }else if(stackSize<=61){stack[stackSize++]=node.meta.x;stack[stackSize++]=node.meta.y;}
    }
    if(!found)return false;
    hitDistance=best;hitUv=bestUv;hitNormal=bestNormal;materialHandle=int(instance.materialHandle);return true;
}

bool traceImportedOpaqueAny(vec3 ro,vec3 rd,float maximumDistance){
    if(uImportedTlasNodeCount<=0||uImportedInstanceCount<=0)return false;
    int stack[64];int stackSize=1;stack[0]=0;
    while(stackSize>0){
        int nodeIndex=stack[--stackSize];if(nodeIndex<0||nodeIndex>=uImportedTlasNodeCount)continue;
        TraceBvhNode node=importedTlasNodes[nodeIndex];
        if(!traceAabb(ro,rd,node.boundsMinimum.xyz,node.boundsMaximum.xyz,maximumDistance))continue;
        if(node.meta.w>0){
            for(int j=0;j<node.meta.w;++j){
                int ii=j==0?node.meta.x:(j==1?node.meta.y:node.meta.z);
                if(ii<0||ii>=uImportedInstanceCount)continue;
                int material=int(importedInstances[ii].materialHandle);
                bool opaque=material<0||material>=uTraceMaterialCount
                    || int(traceRecords[material].extra.w+0.5)==0;
                if(opaque&&traceImportedInstanceAny(ii,ro,rd,maximumDistance))return true;
            }
        }else if(stackSize<=61){stack[stackSize++]=node.meta.x;stack[stackSize++]=node.meta.y;}
    }
    return false;
}

bool traceImportedNearest(vec3 ro,vec3 rd,float maximumDistance,
                          out float hitDistance,out vec2 hitUv,out vec3 hitNormal,
                          out int materialHandle,out int instanceIndex){
    hitDistance=maximumDistance;hitUv=vec2(0.0);hitNormal=vec3(0.0);materialHandle=-1;instanceIndex=-1;
    if(uImportedTlasNodeCount<=0||uImportedInstanceCount<=0)return false;
    int stack[64];int stackSize=1;stack[0]=0;
    while(stackSize>0){
        int nodeIndex=stack[--stackSize];if(nodeIndex<0||nodeIndex>=uImportedTlasNodeCount)continue;
        TraceBvhNode node=importedTlasNodes[nodeIndex];
        if(!traceAabb(ro,rd,node.boundsMinimum.xyz,node.boundsMaximum.xyz,hitDistance))continue;
        if(node.meta.w>0){
            for(int j=0;j<node.meta.w;++j){
                int ii=j==0?node.meta.x:(j==1?node.meta.y:node.meta.z);
                float t;vec2 uv;vec3 n;int material;
                if(traceImportedInstance(ii,ro,rd,hitDistance,t,uv,n,material)){
                    hitDistance=t;hitUv=uv;hitNormal=n;materialHandle=material;instanceIndex=ii;
                }
            }
        }else if(stackSize<=61){stack[stackSize++]=node.meta.x;stack[stackSize++]=node.meta.y;}
    }
    return instanceIndex>=0;
}

float traceResolvedOpacity(int materialHandle,vec2 uv){
    if(materialHandle<0||materialHandle>=uTraceMaterialCount)return 1.0;
    TraceRecord material=traceRecords[materialHandle];
    return clamp(material.advanced.x*traceScalar(material,TRACE_SLOT_OPACITY,uv,1.0),0.0,1.0);
}
float traceResolvedTransmission(int materialHandle,vec2 uv){
    if(materialHandle<0||materialHandle>=uTraceMaterialCount)return 0.0;
    TraceRecord material=traceRecords[materialHandle];
    return clamp(material.transmission.a*traceScalar(material,TRACE_SLOT_TRANSMISSION,uv,1.0),0.0,1.0);
}
bool traceMaterialRejectsHit(int materialHandle,vec2 uv){
    if(materialHandle<0||materialHandle>=uTraceMaterialCount)return false;
    TraceRecord material=traceRecords[materialHandle];
    int renderClass=int(material.extra.w+0.5);
    return renderClass==1&&traceResolvedOpacity(materialHandle,uv)<material.advanced.w;
}

/* Opaque imported geometry can terminate visibility with a true any-hit walk;
 * only masked/transmissive layers need the substantially more expensive
 * nearest-hit continuation path below. */
float importedShadowVisibility(vec3 ro,vec3 rd,float maximumDistance){
    if(traceImportedOpaqueAny(ro,rd,maximumDistance))return 0.0;
    float visibility=1.0;vec3 origin=ro;float remaining=maximumDistance;
    for(int layer=0;layer<24&&remaining>0.0001;++layer){
        float t;vec2 uv;vec3 n;int material,instanceIndex;
        if(!traceImportedNearest(origin,rd,remaining,t,uv,n,material,instanceIndex))break;
        if(traceMaterialRejectsHit(material,uv)){
            float step=t+0.00008;origin+=rd*step;remaining-=step;continue;
        }
        int renderClass=(material>=0&&material<uTraceMaterialCount)?int(traceRecords[material].extra.w+0.5):0;
        float opacity=traceResolvedOpacity(material,uv);
        float transmission=traceResolvedTransmission(material,uv);
        if(renderClass<=1&&opacity>=((material>=0&&material<uTraceMaterialCount)?traceRecords[material].advanced.w:0.5))return 0.0;
        visibility*=clamp(1.0-opacity*(1.0-transmission),0.0,1.0);
        if(visibility<=0.015)return 0.0;
        float step=t+0.00008;origin+=rd*step;remaining-=step;
    }
    return visibility;
}
)GLSL";

} }
#endif
