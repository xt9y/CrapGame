#include "Presenter.hpp"

#include "Gpu.hpp"
#include "PresenterPolicy.hpp"

#include <lwcgl/glmodern.h>

#include <cstddef>
#include <cstdlib>

namespace Renderer
{
namespace Gpu
{
namespace
{

const char *PRESENT_VERTEX_SHADER = R"GLSL(
#version 430 core

out vec2 v_uv;

void main()
{
    vec2 uv = vec2(
        float((gl_VertexID << 1) & 2),
        float(gl_VertexID & 2)
    );

    v_uv = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

const char *PRESENT_FRAGMENT_SHADER = R"GLSL(
#version 430 core

in vec2 v_uv;
out vec4 output_color;

uniform sampler2D u_color;
uniform int u_flip_y;

void main()
{
    vec2 uv = v_uv;

    if (u_flip_y != 0)
    {
        uv.y = 1.0 - uv.y;
    }

    output_color = texture(u_color, uv);
}
)GLSL";

void setError (std::string *error, const char *message)
{
    if (error)
    {
        *error = message ? message : "unknown GPU presenter error";
    }
}

void clearPresentationBackbuffer ()
{
    GL30.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.055f, 0.070f, 0.105f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

} // namespace

bool Presenter::init (std::string *error)
{
    if (ready())
    {
        return true;
    }

    if (!Gpu::available(error))
    {
        return false;
    }

    validate_gl_ = glValidationEnabled(std::getenv("CRAPGAME_GL_VALIDATE"));

    program_ = createGraphicsProgram(
            PRESENT_VERTEX_SHADER,
            PRESENT_FRAGMENT_SHADER,
            error
        );

    if (program_ == 0)
    {
        return false;
    }

    texture_location_ = GL20.glGetUniformLocation(
            program_,
            "u_color"
        );
    flip_y_location_ = GL20.glGetUniformLocation(
            program_,
            "u_flip_y"
        );

    texture_ = glGenTextures();

    if (texture_ == 0)
    {
        setError(error, "failed to allocate presenter texture");
        shutdown();
        return false;
    }

    GL30.glGenVertexArrays(1, &vao_);

    if (vao_ == 0)
    {
        setError(error, "failed to allocate presenter VAO");
        shutdown();
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    GL20.glUseProgram(program_);

    if (texture_location_ >= 0)
    {
        GL20.glUniform1i(texture_location_, 0);
    }

    if (flip_y_location_ >= 0)
    {
        GL20.glUniform1i(flip_y_location_, 0);
    }

    GL20.glUseProgram(0);
    invalidateGpuState();
    return true;
}

bool Presenter::resize (
                int width,
                int height,
                std::string *error
        )
{
    const int new_width = width > 0 ? width : 1,
              new_height = height > 0 ? height : 1;

    if (new_width == width_
            && new_height == height_)
    {
        return true;
    }

    if (!ready() && !init(error))
    {
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, texture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGB,
            new_width,
            new_height,
            0,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            nullptr
        );
    glBindTexture(GL_TEXTURE_2D, 0);
    invalidateGpuState();

    const GLenum gl_error = glGetError();

    if (gl_error != GL_NO_ERROR)
    {
        setError(error, "failed to allocate presenter texture storage");
        return false;
    }

    width_ = new_width;
    height_ = new_height;
    return true;
}

bool Presenter::drawTexture (
                GLuint texture,
                bool flip_y,
                bool conservative_cleanup,
                std::string *error
        )
{
    if (!ready()
            || width_ <= 0
            || height_ <= 0
            || texture == 0)
    {
        setError(error, "invalid presenter texture frame");
        return false;
    }

    const bool bind_state = presenterStateNeedsBind(
            gpu_state_valid_,
            static_cast<std::uint32_t>(texture),
            static_cast<std::uint32_t>(cached_texture_),
            flip_y,
            cached_flip_y_
        );

    if (bind_state)
    {
        GL30.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width_, height_);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        GLModern.glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        GL20.glUseProgram(program_);

        if (flip_y_location_ >= 0)
        {
            GL20.glUniform1i(flip_y_location_, flip_y ? 1 : 0);
        }

        GL30.glBindVertexArray(vao_);
        cached_texture_ = texture;
        cached_flip_y_ = flip_y;
        gpu_state_valid_ = true;
    }

    clearPresentationBackbuffer();
    glDrawArrays(GL_TRIANGLES, 0, 3);

    if (conservative_cleanup)
    {
        GL30.glBindVertexArray(0);
        GL20.glUseProgram(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        invalidateGpuState();
    }

    if (validate_gl_)
    {
        const GLenum gl_error = glGetError();

        if (gl_error != GL_NO_ERROR)
        {
            setError(error, "GPU presentation failed");
            return false;
        }
    }

    return true;
}

bool Presenter::present (
                const std::vector<std::uint8_t>& rgb,
                std::string *error
        )
{
    const std::size_t expected =
        static_cast<std::size_t>(width_) *
        static_cast<std::size_t>(height_) * 3u;

    if (!ready()
            || width_ <= 0
            || height_ <= 0
            || rgb.size() != expected)
    {
        setError(error, "invalid presenter frame");
        return false;
    }

    GLModern.glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            width_,
            height_,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            rgb.data()
        );
    glBindTexture(GL_TEXTURE_2D, 0);
    invalidateGpuState();

    return drawTexture(texture_, true, true, error);
}

bool Presenter::presentTexture (
                GLuint texture,
                std::string *error
        )
{
    /* Rendering publishes explicit invalidation whenever geometry, direct
     * lighting, Lumen, resize, or CPU upload mutates presentation state. A
     * stable present-only frame can therefore reuse the known program/VAO/
     * texture/framebuffer bindings and issue only the fullscreen draw. */
    return drawTexture(texture, false, false, error);
}

void Presenter::invalidateGpuState ()
{
    gpu_state_valid_ = false;
}

void Presenter::shutdown ()
{
    if (vao_ != 0)
    {
        GL30.glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }

    if (texture_ != 0)
    {
        glDeleteTextures(texture_);
        texture_ = 0;
    }

    destroyProgram(&program_);

    cached_texture_ = 0;
    texture_location_ = -1;
    flip_y_location_ = -1;
    gpu_state_valid_ = false;
    cached_flip_y_ = false;
    validate_gl_ = false;
    width_ = 0;
    height_ = 0;
}

bool Presenter::ready () const
{
    return program_ != 0
        && texture_ != 0
        && vao_ != 0;
}

} // namespace Gpu
} // namespace Renderer