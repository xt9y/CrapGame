#include "Gpu.hpp"

#include <algorithm>
#include <vector>

namespace Renderer
{
namespace Gpu
{
namespace
{

void setError (std::string *error, const std::string& message)
{
    if (error)
    {
        *error = message;
    }
}

std::string shaderLog (GLuint shader)
{
    GLint length = 0;
    GL20.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    if (length <= 1)
    {
        return {};
    }

    std::vector<char> log(static_cast<std::size_t>(length), '\0');
    GLsizei written = 0;
    GL20.glGetShaderInfoLog(
            shader,
            length,
            &written,
            log.data()
        );

    if (written <= 0)
    {
        return {};
    }

    return std::string(
            log.data(),
            static_cast<std::size_t>(written)
        );
}

std::string programLog (GLuint program)
{
    GLint length = 0;
    GL20.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

    if (length <= 1)
    {
        return {};
    }

    std::vector<char> log(static_cast<std::size_t>(length), '\0');
    GLsizei written = 0;
    GL20.glGetProgramInfoLog(
            program,
            length,
            &written,
            log.data()
        );

    if (written <= 0)
    {
        return {};
    }

    return std::string(
            log.data(),
            static_cast<std::size_t>(written)
        );
}

GLuint compileShader (
                GLenum type,
                const char *source,
                std::string *error
        )
{
    if (!source || !*source)
    {
        setError(error, "empty GLSL source");
        return 0;
    }

    const GLuint shader = GL20.glCreateShader(type);

    if (shader == 0)
    {
        setError(error, "glCreateShader failed");
        return 0;
    }

    const char *sources[] = {source};
    GL20.glShaderSource(shader, 1, sources, nullptr);
    GL20.glCompileShader(shader);

    GLint compiled = GL_FALSE;
    GL20.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (compiled == GL_TRUE)
    {
        return shader;
    }

    std::string log = shaderLog(shader);
    GL20.glDeleteShader(shader);

    if (log.empty())
    {
        log = "GLSL shader compilation failed without a log";
    }

    setError(error, log);
    return 0;
}

GLuint linkProgram (
                const GLuint *shaders,
                std::size_t shader_count,
                std::string *error
        )
{
    const GLuint program = GL20.glCreateProgram();

    if (program == 0)
    {
        setError(error, "glCreateProgram failed");
        return 0;
    }

    for (std::size_t index = 0; index < shader_count; ++index)
    {
        GL20.glAttachShader(program, shaders[index]);
    }

    GL20.glLinkProgram(program);

    GLint linked = GL_FALSE;
    GL20.glGetProgramiv(program, GL_LINK_STATUS, &linked);

    for (std::size_t index = 0; index < shader_count; ++index)
    {
        GL20.glDetachShader(program, shaders[index]);
    }

    if (linked == GL_TRUE)
    {
        return program;
    }

    std::string log = programLog(program);
    GL20.glDeleteProgram(program);

    if (log.empty())
    {
        log = "GLSL program link failed without a log";
    }

    setError(error, log);
    return 0;
}

} // namespace

bool available (std::string *reason)
{
    if (!lwcglModernGLAvailable())
    {
        const char *missing = lwcglModernGLMissingFunction();
        setError(
                reason,
                missing
                    ? std::string("missing OpenGL capability: ") + missing
                    : "OpenGL 4.3 capability table is unavailable"
            );
        return false;
    }

    if (!GL20.glCreateShader
            || !GL30.glGenFramebuffers
            || !GL31.glDrawElementsInstanced
            || !GL32.glFenceSync
            || !GL42.glBindImageTexture
            || !GL43.glDispatchCompute)
    {
        setError(reason, "incomplete lwcgl modern OpenGL function table");
        return false;
    }

    if (reason)
    {
        reason->clear();
    }

    return true;
}

GLuint createGraphicsProgram (
                const char *vertex_source,
                const char *fragment_source,
                std::string *error
        )
{
    const GLuint vertex = compileShader(
            GL_VERTEX_SHADER,
            vertex_source,
            error
        );

    if (vertex == 0)
    {
        return 0;
    }

    const GLuint fragment = compileShader(
            GL_FRAGMENT_SHADER,
            fragment_source,
            error
        );

    if (fragment == 0)
    {
        GL20.glDeleteShader(vertex);
        return 0;
    }

    const GLuint shaders[] = {vertex, fragment};
    const GLuint program = linkProgram(shaders, 2u, error);

    GL20.glDeleteShader(fragment);
    GL20.glDeleteShader(vertex);
    return program;
}

GLuint createComputeProgram (
                const char *compute_source,
                std::string *error
        )
{
    const GLuint compute = compileShader(
            GL_COMPUTE_SHADER,
            compute_source,
            error
        );

    if (compute == 0)
    {
        return 0;
    }

    const GLuint program = linkProgram(&compute, 1u, error);
    GL20.glDeleteShader(compute);
    return program;
}

void destroyProgram (GLuint *program)
{
    if (!program || *program == 0)
    {
        return;
    }

    GL20.glDeleteProgram(*program);
    *program = 0;
}

} // namespace Gpu
} // namespace Renderer
