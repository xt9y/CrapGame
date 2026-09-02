#ifndef CRAPGAME_RENDER_HPP
#define CRAPGAME_RENDER_HPP

#include "ECS/ecs.hpp"

namespace render {

class Renderer {
public:
    bool init();
    void resize(int width, int height);
    void render(const ecs::World& world);
    void shutdown();

private:
    void applyCamera(const ecs::TransformComponent& transform,
                     const ecs::CameraComponent& camera) const;
    void drawRenderable(const ecs::TransformComponent& transform,
                        const ecs::RenderableComponent& renderable) const;
    void drawCube() const;
    void drawPlane() const;

    int width_ = 1;
    int height_ = 1;
};

} // namespace render

#endif
