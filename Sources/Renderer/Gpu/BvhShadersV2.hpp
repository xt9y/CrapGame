#ifndef CRAPGAME_RENDERER_GPU_BVHSHADERSV2_HPP
#define CRAPGAME_RENDERER_GPU_BVHSHADERSV2_HPP

namespace Renderer
{
namespace Gpu
{

constexpr const char *DIRECT_LIGHTING_BVH_V2_COMPUTE = R"GLSL(
#version 430 core
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(rgba16f,binding=0) readonly uniform image2D gPositionDepth;
layout(rgba16f,binding=1) readonly uniform image2D gNormalRoughness;
layout(rgba16f,binding=2) readonly uniform image2D gAlbedoMetallic;
layout(rgba16f,binding=3) readonly uniform image2D gEmissive;
layout(rgba16f,binding=4) writeonly uniform image2D oDirect;
struct LightData{vec4 positionType;vec4 directionRange;vec4 colorIntensity;vec4 coneShadow;};
struct PrimitiveData{vec4 positionType;vec4 rotation;vec4 scale;vec4 albedoMetallic;vec4 emissiveRoughness;};
struct BvhNode{vec4 boundsMinimum;vec4 boundsMaximum;ivec4 meta;};
layout(std430,binding=5) readonly buffer LightBuffer{LightData lights[];};
layout(std430,binding=6) readonly buffer PrimitiveBuffer{PrimitiveData primitives[];};
layout(std430,binding=7) readonly buffer BvhNodeBuffer{BvhNode bvhNodes[];};
uniform vec3 uCameraPosition;
uniform int uLightCount;
uniform int uPrimitiveCount;
uniform int uBvhNodeCount;
const float PI=3.14159265358979323846;
const float EPSILON=0.00001;
const float SHADOW_BIAS=0.004;
vec3 rotateX(vec3 v,float a){float s=sin(a),c=cos(a);return vec3(v.x,c*v.y-s*v.z,s*v.y+c*v.z);}
vec3 rotateY(vec3 v,float a){float s=sin(a),c=cos(a);return vec3(c*v.x+s*v.z,v.y,-s*v.x+c*v.z);}
vec3 rotateZ(vec3 v,float a){float s=sin(a),c=cos(a);return vec3(c*v.x-s*v.y,s*v.x+c*v.y,v.z);}
vec3 inverseRotate(vec3 v,vec3 d){vec3 r=radians(d);v=rotateY(v,-r.y);v=rotateX(v,-r.x);v=rotateZ(v,-r.z);return v;}
bool intersectBox(vec3 ro,vec3 rd,PrimitiveData p,float maximumDistance){vec3 s=p.scale.xyz;vec3 q=vec3(abs(s.x)>EPSILON?s.x:0.00001,abs(s.y)>EPSILON?s.y:0.00001,abs(s.z)>EPSILON?s.z:0.00001);vec3 o=inverseRotate(ro-p.positionType.xyz,p.rotation.xyz)/q,d=inverseRotate(rd,p.rotation.xyz)/q;vec3 inv=vec3(abs(d.x)>EPSILON?1.0/d.x:1e20,abs(d.y)>EPSILON?1.0/d.y:1e20,abs(d.z)>EPSILON?1.0/d.z:1e20);vec3 t0=(vec3(-0.75)-o)*inv,t1=(vec3(0.75)-o)*inv,mn=min(t0,t1),mx=max(t0,t1);float tn=max(max(mn.x,mn.y),mn.z),tf=min(min(mx.x,mx.y),mx.z);if(tf<max(tn,SHADOW_BIAS))return false;float t=tn>SHADOW_BIAS?tn:tf;return t>SHADOW_BIAS&&t<maximumDistance;}
bool intersectPlane(vec3 ro,vec3 rd,PrimitiveData p,float maximumDistance){vec3 s=p.scale.xyz;vec3 q=vec3(abs(s.x)>EPSILON?s.x:0.00001,abs(s.y)>EPSILON?s.y:0.00001,abs(s.z)>EPSILON?s.z:0.00001);vec3 o=inverseRotate(ro-p.positionType.xyz,p.rotation.xyz)/q,d=inverseRotate(rd,p.rotation.xyz)/q;if(abs(d.y)<=EPSILON)return false;float t=-o.y/d.y;if(t<=SHADOW_BIAS||t>=maximumDistance)return false;vec3 h=o+d*t;return abs(h.x)<=0.5&&abs(h.z)<=0.5;}
bool intersectAabb(vec3 ro,vec3 rd,vec3 mn,vec3 mx,float maximumDistance){vec3 inv=vec3(abs(rd.x)>EPSILON?1.0/rd.x:1e20,abs(rd.y)>EPSILON?1.0/rd.y:1e20,abs(rd.z)>EPSILON?1.0/rd.z:1e20);vec3 t0=(mn-ro)*inv,t1=(mx-ro)*inv,lo=min(t0,t1),hi=max(t0,t1);float tn=max(max(lo.x,lo.y),lo.z),tf=min(min(hi.x,hi.y),hi.z);return tf>=max(tn,0.0)&&tn<maximumDistance;}
bool primitiveHit(int index,vec3 origin,vec3 direction,float maximumDistance){PrimitiveData p=primitives[index];bool hit=int(p.positionType.w+0.5)==0?intersectBox(origin,direction,p,maximumDistance):intersectPlane(origin,direction,p,maximumDistance);return hit&&p.scale.w>0.5;}
bool shadowed(vec3 position,vec3 normal,vec3 direction,float maximumDistance){vec3 origin=position+normalize(normal)*SHADOW_BIAS*2.0;if(uBvhNodeCount<=0){for(int i=0;i<uPrimitiveCount;++i){if(primitiveHit(i,origin,direction,maximumDistance))return true;}return false;}int stack[64];int stackSize=1;stack[0]=0;while(stackSize>0){int ni=stack[--stackSize];if(ni<0||ni>=uBvhNodeCount)continue;BvhNode n=bvhNodes[ni];if(!intersectAabb(origin,direction,n.boundsMinimum.xyz,n.boundsMaximum.xyz,maximumDistance))continue;if(n.meta.w>0){for(int j=0;j<n.meta.w;++j){int pi=n.meta[j];if(pi>=0&&pi<uPrimitiveCount&&primitiveHit(pi,origin,direction,maximumDistance))return true;}}else if(stackSize<=61){stack[stackSize++]=n.meta.x;stack[stackSize++]=n.meta.y;}}return false;}
float pointAttenuation(float d,float r){if(r<=EPSILON||d>=r)return 0.0;float q=d/r,q2=q*q,f=clamp(1.0-q2*q2,0.0,1.0);return f*f/(d*d+1.0);}
float geometrySchlickGgx(float c,float r){float x=r+1.0,k=x*x/8.0;return c/(c*(1.0-k)+k+EPSILON);}
vec3 fresnelSchlick(float c,vec3 f0){float f=pow(1.0-clamp(c,0.0,1.0),5.0);return f0+(vec3(1.0)-f0)*f;}
float distributionGgx(vec3 n,vec3 h,float r){float a=r*r,a2=a*a,c=clamp(dot(n,h),0.0,1.0),c2=c*c,d=c2*(a2-1.0)+1.0;return a2/(PI*d*d+EPSILON);}
float geometrySmith(vec3 n,vec3 v,vec3 l,float r){return geometrySchlickGgx(clamp(dot(n,v),0.0,1.0),r)*geometrySchlickGgx(clamp(dot(n,l),0.0,1.0),r);}
vec3 evaluatePbr(vec3 albedo,float metallic,float roughness,vec3 normal,vec3 viewDirection,vec3 lightDirection,vec3 radiance){vec3 n=normalize(normal),v=normalize(viewDirection),l=normalize(lightDirection),h=normalize(v+l);float nl=clamp(dot(n,l),0.0,1.0),nv=clamp(dot(n,v),0.0,1.0);if(nl<=0.0||nv<=0.0)return vec3(0);vec3 f0=mix(vec3(0.04),albedo,clamp(metallic,0.0,1.0)),f=fresnelSchlick(dot(h,v),f0);float r=clamp(roughness,0.04,1.0),d=distributionGgx(n,h,r),g=geometrySmith(n,v,l,r);vec3 spec=f*d*g/(4.0*nv*nl+EPSILON),dw=(vec3(1)-f)*(1.0-clamp(metallic,0.0,1.0));return(dw*(albedo/PI)+spec)*radiance*nl;}
void main(){ivec2 pixel=ivec2(gl_GlobalInvocationID.xy),dimensions=imageSize(gPositionDepth);if(pixel.x>=dimensions.x||pixel.y>=dimensions.y)return;vec4 pd=imageLoad(gPositionDepth,pixel);if(pd.w<=0){imageStore(oDirect,pixel,vec4(0.055,0.070,0.105,1));return;}vec4 nr=imageLoad(gNormalRoughness,pixel),am=imageLoad(gAlbedoMetallic,pixel);vec3 emissive=imageLoad(gEmissive,pixel).xyz,position=pd.xyz,normal=normalize(nr.xyz),albedo=am.xyz,viewDirection=normalize(uCameraPosition-position);float roughness=nr.w,metallic=am.w;vec3 direct=emissive;for(int i=0;i<uLightCount;++i){LightData light=lights[i];int type=int(light.positionType.w+0.5);vec3 ld=vec3(0),radiance=vec3(0);float maxD=10000.0;if(type==0){ld=normalize(-light.directionRange.xyz);radiance=light.colorIntensity.xyz*light.colorIntensity.w;}else{vec3 toLight=light.positionType.xyz-position;float d=length(toLight);if(d<=EPSILON)continue;float attenuation=pointAttenuation(d,light.directionRange.w);if(attenuation<=0)continue;ld=toLight/d;maxD=max(SHADOW_BIAS,d-SHADOW_BIAS*2.0);float cone=1.0;if(type==2){vec3 fromLight=normalize(position-light.positionType.xyz);float cv=dot(normalize(light.directionRange.xyz),fromLight),inner=light.coneShadow.x,outer=light.coneShadow.y;cone=inner<=outer+EPSILON?(cv>=outer?1.0:0.0):clamp((cv-outer)/(inner-outer),0.0,1.0);}if(cone<=0)continue;radiance=light.colorIntensity.xyz*light.colorIntensity.w*attenuation*cone;}if(dot(radiance,radiance)<=0)continue;if(light.coneShadow.z>0.5&&shadowed(position,normal,ld,maxD))continue;direct+=evaluatePbr(albedo,metallic,roughness,normal,viewDirection,ld,radiance);}imageStore(oDirect,pixel,vec4(direct,1));}
)GLSL";

constexpr const char *LUMEN_TRACE_BVH_V2_COMPUTE = R"GLSL(
#version 430 core
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(binding=0) uniform sampler2D sPositionDepth;
layout(binding=1) uniform sampler2D sNormalRoughness;
layout(binding=2) uniform sampler2D sAlbedoMetallic;
layout(binding=3) uniform sampler2D sDirect;
layout(binding=4) uniform sampler2D sPreviousIndirect;
layout(binding=5) uniform sampler2D sPreviousReflection;
layout(binding=6) uniform sampler2D sPreviousPosition;
layout(rgba16f,binding=0) writeonly uniform image2D oIndirect;
layout(rgba16f,binding=1) writeonly uniform image2D oReflection;
layout(rgba16f,binding=2) writeonly uniform image2D oPositionHistory;
struct PrimitiveData{vec4 positionType;vec4 rotation;vec4 scale;vec4 albedoMetallic;vec4 emissiveRoughness;};
struct BvhNode{vec4 boundsMinimum;vec4 boundsMaximum;ivec4 meta;};
layout(std430,binding=7) readonly buffer PrimitiveBuffer{PrimitiveData primitives[];};
layout(std430,binding=6) readonly buffer BvhNodeBuffer{BvhNode bvhNodes[];};
uniform mat4 uViewProjection;
uniform vec3 uCameraPosition;
uniform int uPrimitiveCount;
uniform int uBvhNodeCount;
uniform int uFrameIndex;
uniform int uHistoryValid;
const float EPSILON=0.00001;
const float TRACE_BIAS=0.012;
vec3 rotateX(vec3 v,float a){float s=sin(a),c=cos(a);return vec3(v.x,c*v.y-s*v.z,s*v.y+c*v.z);}
vec3 rotateY(vec3 v,float a){float s=sin(a),c=cos(a);return vec3(c*v.x+s*v.z,v.y,-s*v.x+c*v.z);}
vec3 rotateZ(vec3 v,float a){float s=sin(a),c=cos(a);return vec3(c*v.x-s*v.y,s*v.x+c*v.y,v.z);}
vec3 inverseRotate(vec3 v,vec3 d){vec3 r=radians(d);v=rotateY(v,-r.y);v=rotateX(v,-r.x);v=rotateZ(v,-r.z);return v;}
vec3 forwardRotate(vec3 v,vec3 d){vec3 r=radians(d);v=rotateZ(v,r.z);v=rotateX(v,r.x);v=rotateY(v,r.y);return v;}
vec3 safeScale(vec3 v){return vec3(abs(v.x)>EPSILON?v.x:0.00001,abs(v.y)>EPSILON?v.y:0.00001,abs(v.z)>EPSILON?v.z:0.00001);}
bool intersectBox(vec3 ro,vec3 rd,PrimitiveData p,float maximumDistance,out float hitDistance,out vec3 hitNormal){vec3 s=safeScale(p.scale.xyz),o=inverseRotate(ro-p.positionType.xyz,p.rotation.xyz)/s,d=inverseRotate(rd,p.rotation.xyz)/s;vec3 inv=vec3(abs(d.x)>EPSILON?1.0/d.x:1e20,abs(d.y)>EPSILON?1.0/d.y:1e20,abs(d.z)>EPSILON?1.0/d.z:1e20);vec3 t0=(vec3(-0.75)-o)*inv,t1=(vec3(0.75)-o)*inv,mn=min(t0,t1),mx=max(t0,t1);float tn=max(max(mn.x,mn.y),mn.z),tf=min(min(mx.x,mx.y),mx.z);if(tf<max(tn,TRACE_BIAS))return false;hitDistance=tn>TRACE_BIAS?tn:tf;if(hitDistance<=TRACE_BIAS||hitDistance>=maximumDistance)return false;vec3 h=o+d*hitDistance,m=abs(h),ln;if(m.x>=m.y&&m.x>=m.z)ln=vec3(sign(h.x),0,0);else if(m.y>=m.z)ln=vec3(0,sign(h.y),0);else ln=vec3(0,0,sign(h.z));hitNormal=normalize(forwardRotate(ln/s,p.rotation.xyz));return true;}
bool intersectPlane(vec3 ro,vec3 rd,PrimitiveData p,float maximumDistance,out float hitDistance,out vec3 hitNormal){vec3 s=safeScale(p.scale.xyz),o=inverseRotate(ro-p.positionType.xyz,p.rotation.xyz)/s,d=inverseRotate(rd,p.rotation.xyz)/s;if(abs(d.y)<=EPSILON)return false;hitDistance=-o.y/d.y;if(hitDistance<=TRACE_BIAS||hitDistance>=maximumDistance)return false;vec3 h=o+d*hitDistance;if(abs(h.x)>0.5||abs(h.z)>0.5)return false;vec3 ln=d.y<0?vec3(0,1,0):vec3(0,-1,0);hitNormal=normalize(forwardRotate(ln/s,p.rotation.xyz));return true;}
bool intersectAabb(vec3 ro,vec3 rd,vec3 mn,vec3 mx,float maximumDistance){vec3 inv=vec3(abs(rd.x)>EPSILON?1.0/rd.x:1e20,abs(rd.y)>EPSILON?1.0/rd.y:1e20,abs(rd.z)>EPSILON?1.0/rd.z:1e20);vec3 t0=(mn-ro)*inv,t1=(mx-ro)*inv,lo=min(t0,t1),hi=max(t0,t1);float tn=max(max(lo.x,lo.y),lo.z),tf=min(min(hi.x,hi.y),hi.z);return tf>=max(tn,0.0)&&tn<maximumDistance;}
void testPrimitive(int index,vec3 ro,vec3 rd,inout int hitIndex,inout float hitDistance,inout vec3 hitNormal){PrimitiveData p=primitives[index];float candidate=hitDistance;vec3 normal=vec3(0);bool hit=int(p.positionType.w+0.5)==0?intersectBox(ro,rd,p,hitDistance,candidate,normal):intersectPlane(ro,rd,p,hitDistance,candidate,normal);if(hit&&p.scale.w>0.5&&candidate<hitDistance){hitIndex=index;hitDistance=candidate;hitNormal=normal;}}
bool traceScene(vec3 ro,vec3 rd,float maximumDistance,out int hitIndex,out float hitDistance,out vec3 hitNormal){hitIndex=-1;hitDistance=maximumDistance;hitNormal=vec3(0);if(uBvhNodeCount<=0){for(int i=0;i<uPrimitiveCount;++i)testPrimitive(i,ro,rd,hitIndex,hitDistance,hitNormal);return hitIndex>=0;}int stack[64];int stackSize=1;stack[0]=0;while(stackSize>0){int ni=stack[--stackSize];if(ni<0||ni>=uBvhNodeCount)continue;BvhNode n=bvhNodes[ni];if(!intersectAabb(ro,rd,n.boundsMinimum.xyz,n.boundsMaximum.xyz,hitDistance))continue;if(n.meta.w>0){for(int j=0;j<n.meta.w;++j){int pi=n.meta[j];if(pi>=0&&pi<uPrimitiveCount)testPrimitive(pi,ro,rd,hitIndex,hitDistance,hitNormal);}}else if(stackSize<=61){stack[stackSize++]=n.meta.x;stack[stackSize++]=n.meta.y;}}return hitIndex>=0;}
uint hashValue(uvec2 v){uint h=v.x*0x8da6b343u+v.y*0xd8163841u;h^=h>>13;h*=0xcb1ab31fu;h^=h>>16;return h;}
float randomValue(inout uint s){s=s*1664525u+1013904223u;return float(s&0x00ffffffu)/float(0x01000000u);}
vec3 hemisphereDirection(vec3 normal,ivec2 pixel){uint state=hashValue(uvec2(pixel))^uint(uFrameIndex*747796405);float r1=randomValue(state),r2=randomValue(state),phi=6.28318530718*r1,radius=sqrt(r2),z=sqrt(max(0.0,1.0-r2));vec3 local=vec3(cos(phi)*radius,sin(phi)*radius,z),n=normalize(normal),helper=abs(n.z)<0.999?vec3(0,0,1):vec3(1,0,0),tangent=normalize(cross(helper,n)),bitangent=cross(n,tangent);return normalize(tangent*local.x+bitangent*local.y+n*local.z);}
vec3 primitiveFallbackRadiance(int i){if(i<0)return vec3(0.018,0.022,0.032);PrimitiveData p=primitives[i];return p.emissiveRoughness.xyz+p.albedoMetallic.xyz*0.045;}
vec3 screenRadiance(vec3 position,int primitiveIndex){vec4 clip=uViewProjection*vec4(position,1);if(clip.w<=EPSILON)return primitiveFallbackRadiance(primitiveIndex);vec2 uv=clip.xy/clip.w*0.5+0.5;if(uv.x<=0||uv.x>=1||uv.y<=0||uv.y>=1)return primitiveFallbackRadiance(primitiveIndex);ivec2 dimensions=textureSize(sPositionDepth,0),samplePixel=clamp(ivec2(uv*vec2(dimensions)),ivec2(0),dimensions-ivec2(1));vec4 sp=texelFetch(sPositionDepth,samplePixel,0);if(sp.w>0&&distance(sp.xyz,position)<0.35)return texelFetch(sDirect,samplePixel,0).xyz;return primitiveFallbackRadiance(primitiveIndex);}
void main(){ivec2 tp=ivec2(gl_GlobalInvocationID.xy),td=imageSize(oIndirect);if(tp.x>=td.x||tp.y>=td.y)return;ivec2 fd=textureSize(sPositionDepth,0),pixel=min(tp*2+ivec2(1),fd-ivec2(1));vec4 pd=texelFetch(sPositionDepth,pixel,0);if(pd.w<=0){imageStore(oIndirect,tp,vec4(0));imageStore(oReflection,tp,vec4(0));imageStore(oPositionHistory,tp,vec4(0));return;}vec4 nr=texelFetch(sNormalRoughness,pixel,0),am=texelFetch(sAlbedoMetallic,pixel,0);vec3 position=pd.xyz,normal=normalize(nr.xyz),albedo=am.xyz;float roughness=clamp(nr.w,0.04,1.0),metallic=clamp(am.w,0.0,1.0);vec3 origin=position+normal*TRACE_BIAS*2.0,giDirection=hemisphereDirection(normal,pixel),indirect=vec3(0);int giHit;float giDistance;vec3 giNormal;if(traceScene(origin,giDirection,28.0,giHit,giDistance,giNormal)){vec3 hp=origin+giDirection*giDistance,source=screenRadiance(hp,giHit);indirect=source*albedo*(1.0-metallic)*0.32;}else{float sky=clamp(normal.y*0.5+0.5,0.0,1.0);indirect=albedo*(1.0-metallic)*mix(0.008,0.028,sky);}vec3 incident=normalize(position-uCameraPosition),reflectionDirection=normalize(reflect(incident,normal)),reflection=vec3(0);if(metallic>0.08||roughness<0.45){int ri;float rd;vec3 rn;if(traceScene(origin,reflectionDirection,48.0,ri,rd,rn)){vec3 hp=origin+reflectionDirection*rd,source=screenRadiance(hp,ri),f0=mix(vec3(0.04),albedo,metallic);float fresnel=pow(1.0-clamp(dot(normal,normalize(uCameraPosition-position)),0.0,1.0),5.0);vec3 weight=f0+(vec3(1.0)-f0)*fresnel;reflection=source*weight*(1.0-roughness*0.82);}}if(uHistoryValid!=0){vec4 pp=texelFetch(sPreviousPosition,tp,0);if(pp.w>0&&distance(pp.xyz,position)<0.18){indirect=mix(indirect,texelFetch(sPreviousIndirect,tp,0).xyz,0.84);reflection=mix(reflection,texelFetch(sPreviousReflection,tp,0).xyz,0.72);}}imageStore(oIndirect,tp,vec4(max(indirect,vec3(0)),1));imageStore(oReflection,tp,vec4(max(reflection,vec3(0)),1));imageStore(oPositionHistory,tp,vec4(position,1));}
)GLSL";

} // namespace Gpu
} // namespace Renderer

#endif