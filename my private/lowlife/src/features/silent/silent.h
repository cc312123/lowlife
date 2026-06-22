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
	namespace silent
	{
		void run();
		void initialize();
	}
}

inline std::unique_ptr<rbx::instance_t> g_mouseservice{};

inline bool g_silent_data_ready{ false };
inline bool g_silent_found_target{ false };
inline bool g_silent_target_needs_reset{ false };
inline math::vector2 g_silent_partpos{};
inline std::uint64_t g_silent_cached_position_x{ 0 };
inline std::uint64_t g_silent_cached_position_y{ 0 };
inline cache::entity_t g_silent_cached_target{};
inline cache::entity_t g_silent_cached_last_target{};
inline std::string g_silent_locked_part_name{};
inline rbx::instance_t g_silent_aim_instance{};

inline bool g_silent_aim_enabled{ true };
inline bool g_silent_sticky_aim{ false };
inline int g_silent_aim_keybind{ 0 };
inline bool g_silent_aim_locked{ false };
inline bool g_silent_aim_manual_locked{ false };
inline bool g_silent_aim_key_was_pressed{ false };

inline std::mutex g_silent_aim_mutex{};
