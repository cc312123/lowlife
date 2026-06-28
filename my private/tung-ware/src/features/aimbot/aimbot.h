#pragma once
#include <memory>
#include <cstdint>
#include <mutex>
#include <sdk/sdk.h>
#include <sdk/math/math.h>
#include <cache/cache.h>
#include <settings.h>

namespace rbx
{
	namespace aimbot
	{
		void initialize();
		void run();
		void render();  

		
		extern bool g_aimbot_manual_locked;
		extern cache::entity_t g_aimbot_manual_target;
		inline std::mutex g_aimbot_mutex;

		void lock_target(const cache::entity_t& target);
		void unlock_target();
	}
}


