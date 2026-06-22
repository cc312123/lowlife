#pragma once
#include <memory>
#include <cstdint>
#include <vector>
#include <string>
#include <mutex>
#include <sdk/sdk.h>
#include <sdk/math/math.h>
#include <cache/cache.h>

namespace rbx
{
	namespace new_silent
	{
		void run();
		void initialize();
	}
}

// Global target variables for ESP and HUD integration
inline bool g_silent_aim_locked{ false };
inline cache::entity_t g_silent_cached_target{};
inline math::vector2 g_silent_partpos{};
inline std::mutex g_silent_aim_mutex{};
