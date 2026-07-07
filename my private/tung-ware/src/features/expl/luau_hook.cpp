#include "luau_hook.h"
#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <memory/memory.h>
#include <game/game.h>
#include <vector>
#include <thread>
#include <chrono>

std::uint64_t luau::find_lua_state()
{
	if (game::datamodel.address == 0) return 0;

	std::uint64_t script_context = game::datamodel.find_first_child_by_class("ScriptContext").address;
	if (script_context == 0)
	{
		script_context = memory->read<std::uint64_t>(game::datamodel.address + Offsets::DataModel::ScriptContext);
	}
	if (script_context == 0) return 0;

	// Widened scan: 0x8 to 0x800 (was 0x400) to handle offset shifts after Roblox updates
	for (std::uint64_t offset = 0x8; offset < 0x800; offset += 8)
	{
		std::uint64_t L = memory->read<std::uint64_t>(script_context + offset);
		if (L == 0 || (L % 8) != 0 || L < 0x100000 || L > 0x7FFFFFFFFFFF)
			continue;

		// Widened: 0x10 to 0x60 (was 0x40)
		for (std::uint64_t offset_global = 0x10; offset_global <= 0x60; offset_global += 8)
		{
			std::uint64_t global_state = memory->read<std::uint64_t>(L + offset_global);
			if (global_state == 0 || (global_state % 8) != 0 || global_state < 0x100000 || global_state > 0x7FFFFFFFFFFF)
				continue;

			// Widened: 0x10 to 0x120 (was 0xE0)
			for (std::uint64_t offset_mainthread = 0x10; offset_mainthread <= 0x120; offset_mainthread += 8)
			{
				std::uint64_t main = memory->read<std::uint64_t>(global_state + offset_mainthread);
				if (main == L || (main != 0 && (main % 8) == 0 && main >= 0x100000 && main <= 0x7FFFFFFFFFFF && memory->read<std::uint64_t>(main + offset_global) == global_state))
				{
					return L;
				}
			}
		}
	}

	return 0;
}


std::uint64_t luau::find_rngstate_offset(std::uint64_t lua_state, std::uint64_t& out_global_state)
{
	out_global_state = 0;
	if (lua_state == 0) return 0;

	
	std::uint64_t global_state = 0;
	for (std::uint64_t offset_global = 0x10; offset_global <= 0x40; offset_global += 8)
	{
		std::uint64_t g = memory->read<std::uint64_t>(lua_state + offset_global);
		if (g == 0 || (g % 8) != 0 || g < 0x100000 || g > 0x7FFFFFFFFFFF)
			continue;

		for (std::uint64_t offset_mainthread = 0x10; offset_mainthread <= 0xE0; offset_mainthread += 8)
		{
			std::uint64_t main = memory->read<std::uint64_t>(g + offset_mainthread);
			if (main == lua_state || (main != 0 && (main % 8) == 0 && main >= 0x100000 && main <= 0x7FFFFFFFFFFF && memory->read<std::uint64_t>(main + offset_global) == g))
			{
				global_state = g;
				break;
			}
		}
		if (global_state != 0) break;
	}

	if (global_state == 0) return 0;
	out_global_state = global_state;

	const std::uint64_t multiplier = 6364136223846793005ULL;
	const std::uint64_t increment = 1442695040888963407ULL;

	
	std::vector<std::uint64_t> snapshot1(256, 0); 
	std::vector<std::uint64_t> snapshot2(256, 0);

	
	for (int attempt = 0; attempt < 50; ++attempt)
	{
		Luck_ReadVirtualMemory(memory->get_process_handle(), reinterpret_cast<void*>(global_state), snapshot1.data(), 2048, nullptr);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		Luck_ReadVirtualMemory(memory->get_process_handle(), reinterpret_cast<void*>(global_state), snapshot2.data(), 2048, nullptr);

		for (size_t i = 0; i < 255; ++i)
		{
			std::uint64_t val1 = snapshot1[i];
			std::uint64_t val2 = snapshot2[i];

			if (val1 == val2 || val1 == 0 || val2 == 0) continue;

			
			std::uint64_t dynamic_inc = snapshot1[i + 1];
			if ((dynamic_inc & 1) != 0 && dynamic_inc != 0)
			{
				
				if (val2 == (val1 * multiplier + dynamic_inc))
				{
					return i * 8;
				}

				
				std::uint64_t t1 = val1 * multiplier + dynamic_inc;
				if (val2 == (t1 * multiplier + dynamic_inc))
				{
					return i * 8;
				}

				
				std::uint64_t t2 = t1 * multiplier + dynamic_inc;
				if (val2 == (t2 * multiplier + dynamic_inc))
				{
					return i * 8;
				}
			}

			
			if (val2 == (val1 * multiplier + increment))
			{
				return i * 8;
			}
			std::uint64_t t1 = val1 * multiplier + increment;
			if (val2 == (t1 * multiplier + increment))
			{
				return i * 8;
			}
			std::uint64_t t2 = t1 * multiplier + increment;
			if (val2 == (t2 * multiplier + increment))
			{
				return i * 8;
			}
		}
	}

	return 0;
}
