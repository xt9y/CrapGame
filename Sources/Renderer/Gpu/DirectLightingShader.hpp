#ifndef CRAPGAME_RENDERER_GPU_DIRECTLIGHTINGSHADER_HPP
#define CRAPGAME_RENDERER_GPU_DIRECTLIGHTINGSHADER_HPP

namespace Renderer { namespace Gpu {

constexpr const char *DIRECT_LIGHTING_COMPUTE = R"GLSL(
#version 430 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rgba16f,binding=0) readonly uniform image2D gPositionDepth;
layout(rgba16f,binding=1) readonly uniform image2D gNormalRoughness;
layout(rgba8,binding=2) readonly uniform image2D gAlbedoMetallic;
layout(rgba16f,binding=3) readonly uniform image2D gEmissive;
layout(rgba16f,binding=4) writeonly uniform image2D oDirect;
layout(binding=0) uniform sampler2D sSpecularIor;
layout(binding=1) uniform sampler2D sAdvancedMaterial;
layout(binding=2) uniform sampler2D sAmbientTransmission;
layout(binding=3) uniform sampler2D sTangentAnisotropy;
struct LightData{vec4 positionType;vec4 directionRange;vec4 colorIntensity;vec4 coneShadow;};
struct PrimitiveData{vec4 positionType;vec4 rotation;vec4 scale;vec4 albedoMetallic;vec4 emissiveRoughness;};
layout(std430,binding=5) readonly buffer LightBuffer{LightData lights[];};
layout(std430,binding=6) readonly buffer PrimitiveBuffer{PrimitiveData primitives[];};
uniform vec3 uCameraPosition;
uniform int uLightCount;
uniform int uPrimitiveCount;
const float PI=3.14159265358979323846;
const float EPSILON=0.00001;
const float SHADOW_BIAS=0.004;
vec3 rotateX(vec3 v,float a){float s=sin(a),c=cos(a);return vec3(v.x,c*v.y-s*v.z,s*v.y+c*v.z);}
vec3 rotateY(vec3 v,float a){float s=sin(a),c=cos(a);return vec3(c*v.x+s*v.z,v.y,-s*v.x+c*v.z);}
vec3 rotateZ(vec3 v,float a){float s=sin(a),c=cos(a);return vec3(c*v.x-s*v.y,s*v.x+c*v.y,v.z);}
vec3 inverseRotate(vec3 v,vec3 d){vec3 r=radians(d);v=rotateY(v,-r.y);v=rotateX(v,-r.x);v=rotateZ(v,-r.z);return v;}
bool intersectBox(vec3 ro,vec3 rd,PrimitiveData p,float maximumDistance){vec3 s=p.scale.xyz;vec3 q=vec3(abs(s.x)>EPSILON?s.x:0.00001,abs(s.y)>EPSILON?s.y:0.00001,abs(s.z)>EPSILON?s.z:0.00001);vec3 o=inverseRotate(ro-p.positionType.xyz,p.rotation.xyz)/q,d=inverseRotate(rd,p.rotation.xyz)/q;vec3 inv=vec3(abs(d.x)>EPSILON?1.0/d.x:1e20,abs(d.y)>EPSILON?1.0/d.y:1e20,abs(d.z)>EPSILON?1.0/d.z:1e20);vec3 t0=(vec3(-0.75)-o)*inv,t1=(vec3(0.75)-o)*inv,mn=min(t0,t1),mx=max(t0,t1);float tn=max(max(mn.x,mn.y),mn.z),tf=min(min(mx.x,mx.y),mx.z);if(tf<max(tn,SHADOW_BIAS))return false;float t=tn>SHADOW_BIAS?tn:tf;return t>SHADOW_BIAS&&t<maximumDistance;}
bool intersectPlane(vec3 ro,vec3 rd,PrimitiveData p,float maximumDistance){vec3 s=p.scale.xyz;vec3 q=vec3(abs(s.x)>EPSILON?s.x:0.00001,abs(s.y)>EPSILON?s.y:0.00001,abs(s.z)>EPSILON?s.z:0.00001);vec3 o=inverseRotate(ro-p.positionType.xyz,p.rotation.xyz)/q,d=inverseRotate(rd,p.rotation.xyz)/q;if(abs(d.y)<=EPSILON)return false;float t=-o.y/d.y;if(t<=SHADOW_BIAS||t>=maximumDistance)return false;vec3 h=o+d*t;return abs(h.x)<=0.5&&abs(h.z)<=0.5;}
bool shadowed(vec3 position,vec3 normal,vec3 direction,float maximumDistance){vec3 origin=position+normalize(normal)*SHADOW_BIAS*2.0;for(int i=0;i<uPrimitiveCount;++i){PrimitiveData p=primitives[i];bool hit=int(p.positionType.w+0.5)==0?intersectBox(origin,direction,p,maximumDistance):intersectPlane(origin,direction,p,maximumDistance);if(hit)return true;}return false;}
float pointAttenuation(float d,float r){if(r<=EPSILON||d>=r)return 0.0;float q=d/r,q2=q*q,f=clamp(1.0-q2*q2,0.0,1.0);return f*f/(d*d+1.0);}
float geometrySchlickGgx(float c,float r){float x=r+1.0,k=x*x/8.0;return c/(c*(1.0-k)+k+EPSILON);}
float geometrySmith(vec3 n,vec3 v,vec3 l,float r){return geometrySchlickGgx(clamp(dot(n,v),0.0,1.0),r)*geometrySchlickGgx(clamp(dot(n,l),0.0,1.0),r);}
vec3 fresnelSchlick(float c,vec3 f0){float f=pow(1.0-clamp(c,0.0,1.0),5.0);return f0+(vec3(1.0)-f0)*f;}
float distributionGgx(vec3 n,vec3 h,float r){float a=r*r,a2=a*a,c=clamp(dot(n,h),0.0,1.0),c2=c*c,d=c2*(a2-1.0)+1.0;return a2/(PI*d*d+EPSILON);}
float distributionAnisotropic(vec3 n,vec3 t,vec3 b,vec3 h,float roughness,float anisotropy){float a=max(0.001,roughness*roughness);float aspect=sqrt(max(0.1,1.0-0.9*abs(anisotropy)));float at=anisotropy>=0.0?a/aspect:a*aspect;float ab=anisotropy>=0.0?a*aspect:a/aspect;float hx=dot(h,t),hy=dot(h,b),hz=max(EPSILON,dot(h,n));float d=hx*hx/(at*at)+hy*hy/(ab*ab)+hz*hz;return 1.0/(PI*at*ab*d*d+EPSILON);}
float iorToF0(float ior){float n=max(1.0001,ior),r=(n-1.0)/(n+1.0);return r*r;}
vec3 evaluateMaterialPbr(vec3 albedo,float metallic,float roughness,vec3 explicitSpecular,float ior,float clearcoat,float clearcoatRoughness,float sheen,float anisotropy,vec3 tangent,vec3 normal,vec3 viewDirection,vec3 lightDirection,vec3 radiance){vec3 n=normalize(normal),v=normalize(viewDirection),l=normalize(lightDirection),h=normalize(v+l);float nl=clamp(dot(n,l),0.0,1.0),nv=clamp(dot(n,v),0.0,1.0);if(nl<=0.0||nv<=0.0)return vec3(0);vec3 t=normalize(tangent-n*dot(n,tangent));vec3 b=normalize(cross(n,t));vec3 dielectricF0=dot(explicitSpecular,explicitSpecular)>EPSILON?clamp(explicitSpecular,vec3(0),vec3(1)):vec3(iorToF0(ior));vec3 f0=mix(dielectricF0,albedo,clamp(metallic,0.0,1.0));vec3 f=fresnelSchlick(dot(h,v),f0);float r=clamp(roughness,0.04,1.0);float d=abs(anisotropy)>0.001?distributionAnisotropic(n,t,b,h,r,anisotropy):distributionGgx(n,h,r);float g=geometrySmith(n,v,l,r);vec3 spec=f*d*g/(4.0*nv*nl+EPSILON);vec3 diffuseWeight=(vec3(1)-f)*(1.0-clamp(metallic,0.0,1.0));vec3 base=(diffuseWeight*(albedo/PI)+spec);float cc=clamp(clearcoat,0.0,1.0);if(cc>0.0){float cr=clamp(clearcoatRoughness,0.04,1.0);vec3 cf=fresnelSchlick(dot(h,v),vec3(0.04));float cd=distributionGgx(n,h,cr),cg=geometrySmith(n,v,l,cr);vec3 coat=cf*cd*cg/(4.0*nv*nl+EPSILON);base=base*(vec3(1)-cf*cc)+coat*cc;}float sheenFactor=clamp(sheen,0.0,1.0)*(1.0-clamp(metallic,0.0,1.0))*pow(1.0-clamp(dot(h,v),0.0,1.0),5.0);base+=mix(vec3(1),albedo,0.5)*sheenFactor;return base*radiance*nl;}
void main(){ivec2 pixel=ivec2(gl_GlobalInvocationID.xy),dimensions=imageSize(gPositionDepth);if(pixel.x>=dimensions.x||pixel.y>=dimensions.y)return;vec4 pd=imageLoad(gPositionDepth,pixel);if(pd.w<=0){imageStore(oDirect,pixel,vec4(0.055,0.070,0.105,1));return;}vec4 nr=imageLoad(gNormalRoughness,pixel),am=imageLoad(gAlbedoMetallic,pixel),eo=imageLoad(gEmissive,pixel);vec4 si=texelFetch(sSpecularIor,pixel,0),adv=texelFetch(sAdvancedMaterial,pixel,0),ta=texelFetch(sTangentAnisotropy,pixel,0);vec3 position=pd.xyz,normal=normalize(nr.xyz),albedo=am.xyz,viewDirection=normalize(uCameraPosition-position),emissive=eo.xyz;float roughness=nr.w,metallic=am.w;vec3 direct=emissive;for(int i=0;i<uLightCount;++i){LightData light=lights[i];int type=int(light.positionType.w+0.5);vec3 ld=vec3(0),radiance=vec3(0);float maxD=10000.0;if(type==0){ld=normalize(-light.directionRange.xyz);radiance=light.colorIntensity.xyz*light.colorIntensity.w;}else{vec3 toLight=light.positionType.xyz-position;float dist=length(toLight);if(dist<=EPSILON)continue;float attenuation=pointAttenuation(dist,light.directionRange.w);if(attenuation<=0)continue;ld=toLight/dist;maxD=max(SHADOW_BIAS,dist-SHADOW_BIAS*2.0);float cone=1.0;if(type==2){vec3 fromLight=normalize(position-light.positionType.xyz);float cv=dot(normalize(light.directionRange.xyz),fromLight),inner=light.coneShadow.x,outer=light.coneShadow.y;cone=inner<=outer+EPSILON?(cv>=outer?1.0:0.0):clamp((cv-outer)/(inner-outer),0.0,1.0);}if(cone<=0)continue;radiance=light.colorIntensity.xyz*light.colorIntensity.w*attenuation*cone;}if(dot(radiance,radiance)<=0)continue;if(light.coneShadow.z>0.5&&shadowed(position,normal,ld,maxD))continue;direct+=evaluateMaterialPbr(albedo,metallic,roughness,si.rgb,si.a,adv.x,adv.y,adv.z,ta.a,ta.xyz,normal,viewDirection,ld,radiance);}imageStore(oDirect,pixel,vec4(direct,1));}
)GLSL";

} }
#endif
