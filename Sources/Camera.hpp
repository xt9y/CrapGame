#ifndef CRAPGAME_CAMERA_HPP
#define CRAPGAME_CAMERA_HPP

#include "Ecs/Ecs.hpp"

class Camera
{
public:
    void update (Ecs::World& world, float delta_seconds);

private:
    bool initialized_ = false;
    bool grabbed_ = true;
    bool tab_down_ = false;
};

#endif
