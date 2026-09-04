#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

int main()
{
    std::ifstream in("Sources/Renderer/Gpu/GBufferGpu.cpp");
    std::string source((std::istreambuf_iterator<char>(in)), {});
    assert(source.find("aTangent") != std::string::npos);
    assert(source.find("uBaseColorTex") != std::string::npos);
    assert(source.find("uNormalTex") != std::string::npos);
    assert(source.find("uOpacityTex") != std::string::npos);
    assert(source.find("discard") != std::string::npos);
    assert(source.find("GL_COLOR_ATTACHMENT6") != std::string::npos);
    assert(source.find("GL_COLOR_ATTACHMENT7") == std::string::npos);
    assert(source.find("glDrawBuffers(7") != std::string::npos);
    assert(source.find("renderer_material") != std::string::npos);
    assert(source.find("RenderClass::Transparent") != std::string::npos);
    assert(source.find("MaterialGpu::LIVE_TEXTURE_COUNT") != std::string::npos);
}
