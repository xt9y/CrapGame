#include <cbuild.h>

static void configure_platform(C_Target *target) {
#ifdef __APPLE__
    c_define(target, "GL_SILENCE_DEPRECATION");
    c_include(target, "/opt/homebrew/include");
    c_include(target, "/usr/local/include");
    c_link_flag(target, "-L/opt/homebrew/lib");
    c_link_flag(target, "-L/usr/local/lib");
    c_link_system(target, "glfw");
    c_framework(target, "OpenGL");
    c_framework(target, "Cocoa");
    c_framework(target, "IOKit");
    c_framework(target, "CoreVideo");
#else
    c_link_system(target, "glfw");
    c_link_system(target, "GL");
    c_link_system(target, "GLU");
    c_link_system(target, "m");
    c_link_system(target, "dl");
    c_link_system(target, "pthread");
#endif
}

static void configure_app(C_Target *target,
                          C_Dependency *lwcgl,
                          C_Dependency *renderercheck) {
    c_warnings_strict(target);
    c_use(target, lwcgl);
    c_use(target, renderercheck);
    configure_platform(target);
}

void build(C_Build *b) {
    C_Dependency *lwcgl = c_git(
        b,
        "lwcgl",
        "https://github.com/xt9y/lwcgl.git",
        "v2.9.3"
    );
    c_dep_source(lwcgl);
    c_dep_include(lwcgl, "include");
    c_dep_sources(lwcgl, "src/*.c");
    c_dep_flag(lwcgl, "-D_POSIX_C_SOURCE=200809L");
#ifdef __APPLE__
    c_dep_flag(lwcgl, "-DGL_SILENCE_DEPRECATION");
    c_dep_flag(lwcgl, "-I/opt/homebrew/include");
    c_dep_flag(lwcgl, "-I/usr/local/include");
#endif

    C_Dependency *renderercheck = c_git(
        b,
        "renderercheck",
        "https://github.com/xt9y/RendererCheck.git",
        "main"
    );
    c_dep_header_only(renderercheck);
    c_dep_include(renderercheck, "include");

    C_Target *c_app = c_executable(b, "crapgame-c");
    c_sources(c_app, "main.c");
    c_standard(c_app, C_STANDARD_C11);
    configure_app(c_app, lwcgl, renderercheck);
    c_default_target(b, c_app);

    C_Target *cpp_app = c_executable(b, "crapgame-cpp");
    c_sources(cpp_app, "main.cpp");
    c_flag(cpp_app, "-std=c++17");
    configure_app(cpp_app, lwcgl, renderercheck);
}
