#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

static void require (bool value, const char *message)
{
    if (!value)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

int main ()
{
    std::ifstream input("Sources/Renderer/Gpu/Presenter.cpp");
    require(static_cast<bool>(input), "Presenter.cpp must be readable");

    const std::string source(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    );

    const std::size_t helper = source.find("void clearPresentationBackbuffer ()");
    const std::size_t call = source.find("clearPresentationBackbuffer();");
    const std::size_t draw = source.find("glDrawArrays(GL_TRIANGLES, 0, 3);");

    require(helper != std::string::npos,
            "presenter must own an explicit backbuffer-clear helper");
    require(call != std::string::npos,
            "presenter must clear before fullscreen presentation");
    require(draw != std::string::npos && call < draw,
            "backbuffer clear must occur before fullscreen draw");
    require(source.find("glDisable(GL_SCISSOR_TEST);") != std::string::npos,
            "backbuffer clear must not inherit a stale scissor rectangle");
    require(source.find("glClear(GL_COLOR_BUFFER_BIT);") != std::string::npos,
            "backbuffer color must be explicitly cleared");

    std::cout << "presenter_backbuffer_clear_contract=PASS\n";
    return 0;
}
