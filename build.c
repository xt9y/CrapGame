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
                          C_Dependency *renderercheck) {
    c_warnings_strict(target);
    c_use(target, renderercheck);
    // Use system-installed lwcgl from `make && sudo make install` in the lwcgl repo
    // (expected at /usr/local/lib/liblwcgl.a and /usr/local/include/lwcgl/lwcgl.h).
    // /usr/local/include and /usr/local/lib are default search paths on most
    // systems, so only the -llwcgl link is required; cbuild emits -L flags
    // after -l flags, so we rely on the default search path instead of
    // adding an explicit -L that would be ordered incorrectly.
    c_include(target, "/usr/local/include");
    c_link_system(target, "lwcgl");
    configure_platform(target);
}

void build(C_Build *b) {
    C_Dependency *renderercheck = c_git(
        b,
        "renderercheck",
        "https://github.com/xt9y/RendererCheck.git",
        "main"
    );
    c_dep_header_only(renderercheck);
    c_dep_include(renderercheck, "include");

    C_Target *app = c_executable(b, "crapgame");
    c_sources(app, "main.cpp");
    c_flag(app, "-std=c++17");
    configure_app(app, renderercheck);
    // c uses $(CC) for linking, so an explicit stdlib link is needed for C++.
    c_link_system(app, "stdc++");
    c_default_target(b, app);
}
