#pragma once
#include <sdk/math/math.h>

namespace math
{
    struct cframe
    {
        math::vector3 position;
        math::matrix3 rotation;
        
        cframe() : position{0.0f, 0.0f, 0.0f}, rotation{} {}
        cframe(const math::vector3& pos, const math::matrix3& rot) : position(pos), rotation(rot) {}
    };
}

void fly_function(const math::cframe& cframe, const math::vector3& velocity);

namespace fly
{
    void run();
}

