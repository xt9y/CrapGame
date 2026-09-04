#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

static void require(bool value,const char *message)
{
    if(value)return;
    std::fprintf(stderr,"FAIL: %s\n",message);
    std::exit(1);
}

int main()
{
    std::ifstream input("Sources/Renderer/Gpu/Profiler.cpp");
    require(input.good(),"Profiler.cpp must be readable");
    const std::string source((std::istreambuf_iterator<char>(input)),{});
    require(source.find("SAMPLE_INTERVAL")==std::string::npos,
            "explicit profiling must not skip low-FPS frames");
    require(source.find("PRINT_INTERVAL_NS = 1000000000ull")!=std::string::npos,
            "profile output must be bounded to at most once per second");
    require(source.find("GL_QUERY_RESULT_AVAILABLE")!=std::string::npos,
            "profiling must poll queries asynchronously");
    require(source.find("radiance-hit")!=std::string::npos
            &&source.find("reflection-hit")!=std::string::npos,
            "cache hit telemetry must be exposed");

    std::puts("gpu_profile_low_fps_contract=PASS");
    return 0;
}
