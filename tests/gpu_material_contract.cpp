#include "Renderer/Gpu/MaterialGpu.hpp"
#include <cassert>

static int generated=0, deleted=0, images=0, mipmaps=0;
GLuint lwcgl_glGenTexture(){return static_cast<GLuint>(++generated);}
void glBindTexture(GLenum,GLuint){}
void glTexParameteri(GLenum,GLenum,GLint){}
void glTexImage2D(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*){++images;}
void lwcgl_glDeleteTexture(GLuint){++deleted;}
static void genMip(GLenum){++mipmaps;}
static void active(GLenum){}
GL30API GL30{};
GLModernAPI GLModern{};

namespace Models {
static TextureAsset asset{"x",{2,2,{128,128,255,255,128,128,255,255,128,128,255,255,128,128,255,255},false}};
const TextureAsset* texture(TextureHandle h){return h==1?&asset:nullptr;}
bool normalMapUsesNegativeY(const std::string&){return false;}
}
namespace Renderer { namespace Material {
static Resource res;
const Resource* get(MaterialHandle h){return h==0?&res:nullptr;}
} }

int main(){
    GL30.glGenerateMipmap=genMip;
    GLModern.glActiveTexture=active;
    using namespace Renderer;
    auto &r=Material::res;
    r.textures[Material::slotIndex(Material::Slot::BaseColor)].texture=1;
    r.textures[Material::slotIndex(Material::Slot::BaseColor)].color_space=Material::ColorSpace::Srgb;
    r.textures[Material::slotIndex(Material::Slot::Normal)].texture=1;
    r.textures[Material::slotIndex(Material::Slot::Normal)].color_space=Material::ColorSpace::Linear;
    Gpu::MaterialGpu gpu;
    std::string error;
    assert(gpu.init(&error));
    assert(gpu.ensure(0,&error));
    assert(generated==2);
    const int before=generated;
    assert(gpu.ensure(0,&error));
    assert(generated==before);
    assert((gpu.textureMask(0)&1u)!=0u);
    assert(gpu.bind(0,0,&error));
    assert(generated==before);
    assert(mipmaps>=1);
    assert(images>=3);
    gpu.shutdown();
    assert(deleted==2);
}
