#pragma once
#include <cstdint>

namespace luau
{
	std::uint64_t find_lua_state();
	std::uint64_t find_rngstate_offset(std::uint64_t lua_state, std::uint64_t& out_global_state);
}
