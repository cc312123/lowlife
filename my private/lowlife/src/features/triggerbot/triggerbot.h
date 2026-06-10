#pragma once
#include <sdk/math/math.h>

namespace botter
{
	void run();
	bool is_occluded(const math::vector3& start, const math::vector3& end);
}
