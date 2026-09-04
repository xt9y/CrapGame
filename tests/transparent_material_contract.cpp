#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
static std::string read(const char*p){std::ifstream in(p);return std::string((std::istreambuf_iterator<char>(in)),{});}
int main(){auto s=read("Sources/Renderer/Gpu/TransparentGpu.cpp");assert(s.find("RenderClass::Transparent")!=std::string::npos);assert(s.find("RenderClass::Transmissive")!=std::string::npos);assert(s.find("stable_sort")!=std::string::npos);assert(s.find("a.depth>b.depth")!=std::string::npos);assert(s.find("GL_ONE_MINUS_SRC_ALPHA")!=std::string::npos);assert(s.find("sOpaque")!=std::string::npos);assert(s.find("iorF0")!=std::string::npos);assert(s.find("GL_READ_FRAMEBUFFER")!=std::string::npos);}
