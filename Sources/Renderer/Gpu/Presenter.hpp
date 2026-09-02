#ifndef CRAPGAME_RENDERER_GPU_PRESENTER_HPP
#define CRAPGAME_RENDERER_GPU_PRESENTER_HPP

#include <lwcgl/lwcgl.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Renderer
{
namespace Gpu
{

class Presenter
{
public:
    bool init (std::string *error = nullptr);
    bool resize (int width, int height, std::string *error = nullptr);
    bool present (const std::vector<std::uint8_t>& rgb, std::string *error = nullptr);
    bool presentTexture (GLuint texture, std::string *error = nullptr);
    void shutdown ();

    bool ready () const;

private:
    bool drawTexture (GLuint texture, bool flip_y, std::string *error);

    GLuint program_ = 0;
    GLuint texture_ = 0;
    GLuint vao_ = 0;
    GLint texture_location_ = -1;
    GLint flip_y_location_ = -1;

    int width_ = 0;
    int height_ = 0;
};

} // namespace Gpu
} // namespace Renderer

#endif
