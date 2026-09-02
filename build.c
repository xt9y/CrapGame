#include <cbuild.h>

static void configurePlatform (C_Target *target) 
{
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

static void configureApp (C_Target *target) 
{
    c_warnings_strict(target);

    c_include(target, "Sources");
    c_include(target, "/usr/local/include");

    c_link_system(target, "lwcgl");

    configurePlatform(target);
}

void build (C_Build *b) 
{
    C_Target *app = c_executable(b, "crapgame");

    c_sources(app, "Sources/*.cpp");
    c_sources(app, "Sources/Ecs/*.cpp");
    c_sources(app, "Sources/Renderer/*.cpp");
    c_sources(app, "Sources/Renderer/Math/*.cpp");
    c_sources(app, "Sources/Renderer/Mesh/*.cpp");
    c_sources(app, "Sources/Renderer/Shader/*.cpp");
    c_sources(app, "Sources/Renderer/GBuffer/*.cpp");
    c_sources(app, "Sources/Renderer/Lighting/*.cpp");
    c_sources(app, "Sources/Renderer/Shadows/*.cpp");
    c_sources(app, "Sources/Renderer/Temporal/*.cpp");
    c_sources(app, "Sources/Renderer/Lumen/*.cpp");
    c_sources(app, "Sources/Renderer/Gpu/*.cpp");
    c_sources(app, "Sources/Renderer/Test/*.cpp");

    c_flag(app, "-std=c++17");

    configureApp(app);

    // c uses $(CC) for linking, so an explicit stdlib link is needed for C++.
    c_link_system(app, "stdc++");

    c_default_target(b, app);
}
