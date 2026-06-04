#pragma once
#include <sdk/math/math.h>

namespace math
{
    struct cframe
    {
        math::matrix3 rotation;
        math::vector3 position;
        
        cframe() : rotation{}, position{0.0f, 0.0f, 0.0f} {}
        cframe(const math::vector3& pos, const math::matrix3& rot) : rotation(rot), position(pos) {}
    };
}

void fly_function(const math::cframe& cframe, const math::vector3& velocity);

namespace fly
{
    void run();
}

