#ifndef CRAPGAME_RENDERER_GPU_RADIANCECACHESHADER_HPP
#define CRAPGAME_RENDERER_GPU_RADIANCECACHESHADER_HPP

namespace Renderer
{
namespace Gpu
{

constexpr const char *RADIANCE_CACHE_GLSL=R"GLSL(
struct RadianceCacheRecord{
    ivec4 keyGeneration;
    uvec4 meta;
    vec4 radiance;
    vec4 normal;
};
layout(std430,binding=9) coherent buffer RadianceCacheBuffer{RadianceCacheRecord radianceRecords[];};
uniform int uRadianceGeneration;
const float RADIANCE_CELL_SIZE=0.5;
const uint RADIANCE_CAPACITY=65536u;
const uint RADIANCE_ACCEPT_CONFIDENCE=16u;
const uint RADIANCE_HIGH_CONFIDENCE=48u;
const uint RADIANCE_MAX_PROBES=8u;
uint radianceHash(ivec3 cell){uvec3 v=uvec3(cell);uint h=v.x*0x8da6b343u+v.y*0xd8163841u+v.z*0xcb1ab31fu;h^=h>>13;h*=0x85ebca6bu;h^=h>>16;return h;}
bool radianceReadCell(ivec3 cell,out vec3 value,out vec3 storedNormal,out uint confidence){uint start=radianceHash(cell)&(RADIANCE_CAPACITY-1u);for(uint probe=0u;probe<RADIANCE_MAX_PROBES;++probe){uint index=(start+probe)&(RADIANCE_CAPACITY-1u);if(radianceRecords[index].meta.x!=2u)continue;memoryBarrierBuffer();ivec4 key=radianceRecords[index].keyGeneration;if(key.w!=uRadianceGeneration||any(notEqual(key.xyz,cell)))continue;uint samples=radianceRecords[index].meta.y;vec3 radiance=radianceRecords[index].radiance.xyz;vec3 n=radianceRecords[index].normal.xyz;memoryBarrierBuffer();if(radianceRecords[index].meta.x!=2u)continue;if(samples<RADIANCE_ACCEPT_CONFIDENCE)return false;value=radiance;storedNormal=n;confidence=samples;return true;}return false;}
bool radianceCacheLookup(vec3 position,vec3 normal,out vec3 result){vec3 grid=position/RADIANCE_CELL_SIZE;ivec3 base=ivec3(floor(grid));vec3 f=fract(grid),n=normalize(normal);vec3 sum=vec3(0.0);float total=0.0;for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x){ivec3 cell=base+ivec3(x,y,z);vec3 value,storedNormal;uint confidence;if(!radianceReadCell(cell,value,storedNormal,confidence))continue;vec3 corner=vec3(float(x),float(y),float(z));vec3 axisWeight=mix(vec3(1.0)-f,f,corner);float spatial=axisWeight.x*axisWeight.y*axisWeight.z;float normalLength=length(storedNormal);if(normalLength<=0.0001)continue;float agreement=max(dot(n,storedNormal/normalLength),0.0);float weight=spatial*agreement;if(weight<=0.0001)continue;sum+=value*weight;total+=weight;}if(total<=0.0001)return false;result=sum/total;return true;}
void radianceCacheUpdate(vec3 position,vec3 normal,vec3 sampleRadiance){ivec3 cell=ivec3(floor(position/RADIANCE_CELL_SIZE));uint start=radianceHash(cell)&(RADIANCE_CAPACITY-1u);for(uint probe=0u;probe<RADIANCE_MAX_PROBES;++probe){uint index=(start+probe)&(RADIANCE_CAPACITY-1u);uint state=radianceRecords[index].meta.x;if(state==1u)return;ivec4 key=radianceRecords[index].keyGeneration;bool same=state==2u&&key.w==uRadianceGeneration&&all(equal(key.xyz,cell));bool stale=state==2u&&key.w!=uRadianceGeneration;if(!same&&state!=0u&&!stale)continue;if(atomicCompSwap(radianceRecords[index].meta.x,state,1u)!=state)return;ivec4 lockedKey=radianceRecords[index].keyGeneration;bool lockedSame=state==2u&&lockedKey.w==uRadianceGeneration&&all(equal(lockedKey.xyz,cell));uint samples=lockedSame?min(radianceRecords[index].meta.y,RADIANCE_HIGH_CONFIDENCE):0u;if(samples<RADIANCE_HIGH_CONFIDENCE){float count=float(samples),next=float(samples+1u);vec3 oldRadiance=lockedSame?radianceRecords[index].radiance.xyz:vec3(0.0);vec3 oldNormal=lockedSame?radianceRecords[index].normal.xyz:vec3(0.0);vec3 averagedRadiance=(oldRadiance*count+max(sampleRadiance,vec3(0.0)))/next;vec3 accumulatedNormal=oldNormal*count+normalize(normal);float normalLength=length(accumulatedNormal);if(normalLength>0.0001)accumulatedNormal/=normalLength;radianceRecords[index].keyGeneration=ivec4(cell,uRadianceGeneration);radianceRecords[index].radiance=vec4(averagedRadiance,0.0);radianceRecords[index].normal=vec4(accumulatedNormal,0.0);radianceRecords[index].meta.y=samples+1u;radianceRecords[index].meta.zw=uvec2(0u);}memoryBarrierBuffer();atomicExchange(radianceRecords[index].meta.x,2u);return;}}
)GLSL";

} // namespace Gpu
} // namespace Renderer

#endif
