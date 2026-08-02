#pragma once
#include <sdk/math/math.h>
#include <cache/cache.h>
#include <mutex>

namespace botter
{
	void run();
	bool is_occluded(const math::vector3& start, const math::vector3& end);
}

namespace shot_detect
{
	extern cache::entity_t target_player;
	extern bool has_target;
	extern int last_ammo_val;
	inline std::mutex g_shot_detect_mutex;

	void run();
	int get_target_ammo(const cache::entity_t& target, std::uint64_t* out_tool_addr = nullptr);
}

