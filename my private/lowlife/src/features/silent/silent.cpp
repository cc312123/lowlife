#define NOMINMAX
#include <Windows.h>
#include <thread>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <mutex>
#include <chrono>

#include <memory/memory.h>
#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <cache/cache.h>
#include <game/game.h>
#include <settings.h>
#include <check/typing_check.h>
#include "../triggerbot/triggerbot.h"
#include "silent.h"

namespace
{
	float get_magnitude(const math::vector2& a, const math::vector2& b)
	{
		float dx = a.x - b.x;
		float dy = a.y - b.y;
		return std::sqrt(dx * dx + dy * dy);
	}

	float get_effective_fov(const std::string& tool_name)
	{
		if (!settings::silent::gun_based_fov)
			return settings::silent::fov;

		std::string tool_name_lower = tool_name;
		std::transform(tool_name_lower.begin(), tool_name_lower.end(), tool_name_lower.begin(), ::tolower);

		if (tool_name_lower.find("double-barrel") != std::string::npos || 
			tool_name_lower.find("double barrel") != std::string::npos ||
			tool_name_lower.find("doublebarrel") != std::string::npos)
			return settings::silent::fov_double_barrel;
		else if (tool_name_lower.find("tacticalshotgun") != std::string::npos ||
			tool_name_lower.find("tactical shotgun") != std::string::npos)
			return settings::silent::fov_tactical_shotgun;
		else if (tool_name_lower.find("revolver") != std::string::npos)
			return settings::silent::fov_revolver;

		return settings::silent::fov;
	}

	bool is_player_knocked(const cache::entity_t& player)
	{
		if (player.is_knocked) return true;
		if (player.humanoid.address != 0) {
			try {
				float health = const_cast<cache::entity_t&>(player).humanoid.get_health();
				if (health <= 0.0f || !std::isfinite(health)) {
					return true;
				}
				
				bool platform_stand = memory->read<bool>(player.humanoid.address + Offsets::Humanoid::PlatformStand);
				bool is_sitting = memory->read<bool>(player.humanoid.address + Offsets::Humanoid::Sit);
				
				if (player.ko_address != 0) {
					if (memory->read<bool>(player.ko_address + Offsets::Misc::Value) || (platform_stand && !is_sitting)) {
						return true;
					}
				} else {
					if ((health <= 20.0f && platform_stand && !is_sitting)) {
						return true;
					}
				}
			} catch (...) {}
		}
		return false;
	}

	rbx::part_t get_target_part(const cache::entity_t& player, int aim_part, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view)
	{
		if (player.parts.empty()) return rbx::part_t{};

		if (aim_part == 0)
		{
			auto it = player.parts.find("Head");
			if (it != player.parts.end()) return it->second;
		}
		else if (aim_part == 1)
		{
			auto it = player.parts.find("UpperTorso");
			if (it != player.parts.end()) return it->second;
			it = player.parts.find("Torso");
			if (it != player.parts.end()) return it->second;
		}
		else if (aim_part == 2)
		{
			rbx::part_t closest = {};
			float min_dist = std::numeric_limits<float>::max();
			float cursor_x = static_cast<float>(cursor_pt.x);
			float cursor_y = static_cast<float>(cursor_pt.y);

			const std::unordered_set<std::string> valid_hitparts = {
				"Head",
				"Torso", "UpperTorso", "LowerTorso",
				"Left Arm", "LeftUpperArm", "LeftLowerArm", "LeftHand",
				"Right Arm", "RightUpperArm", "RightLowerArm", "RightHand",
				"Left Leg", "LeftUpperLeg", "LeftLowerLeg", "LeftFoot",
				"Right Leg", "RightUpperLeg", "RightLowerLeg", "RightFoot",
				"HumanoidRootPart"
			};

			for (const auto& pair : player.parts) {
				if (valid_hitparts.find(pair.first) == valid_hitparts.end()) continue;
				rbx::part_t part = pair.second;
				if (!part.address) continue;
				rbx::primitive_t primitive = part.get_primitive();
				if (!primitive.address) continue;
				math::vector3 world_pos = primitive.get_position();
				math::vector2 screen_pos = {};
				if (!game::visengine.world_to_screen(world_pos, screen_pos, dims, view)) continue;
				float dist = get_magnitude(screen_pos, { cursor_x, cursor_y });
				if (dist < min_dist) {
					min_dist = dist;
					closest = part;
				}
			}
			if (closest.address != 0) return closest;
		}

		if (auto it = player.parts.find("HumanoidRootPart"); it != player.parts.end()) return it->second;
		if (auto it = player.parts.find("Head"); it != player.parts.end()) return it->second;
		return rbx::part_t{};
	}

	bool is_target_valid(const cache::entity_t& player, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view, const std::string& tool_name, bool skip_fov_check = false)
	{
		if (player.instance.address == 0) return false;

		auto rel_it = settings::player_relations::relations.find(player.name);
		if (rel_it != settings::player_relations::relations.end() && rel_it->second == 1) {
			return false;
		}

		if (settings::silent::knocked_check && is_player_knocked(player)) {
			return false;
		}

		if (settings::silent::fov_check && !skip_fov_check) {
			rbx::part_t target_part = get_target_part(player, settings::silent::aim_part, cursor_pt, dims, view);
			if (!target_part.address) return false;

			rbx::primitive_t primitive = target_part.get_primitive();
			if (!primitive.address) return false;
			math::vector3 world_pos = primitive.get_position();
			math::vector2 screen_pos = {};

			if (!game::visengine.world_to_screen(world_pos, screen_pos, dims, view)) return false;

			float dist = get_magnitude(screen_pos, { static_cast<float>(cursor_pt.x), static_cast<float>(cursor_pt.y) });
			if (dist > get_effective_fov(tool_name)) return false;
		}

		if (settings::silent::wall_check && !settings::silent::magic_bullet) {
			rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
			if (camera_inst.address != 0) {
				rbx::camera_t camera{ camera_inst.address };
				math::vector3 camera_pos = camera.get_position();
				
				bool any_part_visible = false;
				const std::vector<std::string> parts_to_check = { "Head", "UpperTorso", "HumanoidRootPart" };
				for (const auto& part_name : parts_to_check) {
					auto it = player.parts.find(part_name);
					if (it != player.parts.end() && it->second.address != 0) {
						rbx::part_t part = it->second;
						rbx::primitive_t primitive = part.get_primitive();
						if (primitive.address != 0) {
							math::vector3 world_pos = primitive.get_position();
							if (!botter::is_occluded(camera_pos, world_pos)) {
								any_part_visible = true;
								break;
							}
						}
					}
				}
				if (!any_part_visible) return false;
			}
		}

		return true;
	}

	void write_mouse_position(std::uint64_t address, float x, float y)
	{
		if (address == 0 || address == 0xFFFFFFFFFFFFFFFF)
			return;

		math::vector2 new_position = { x, y };

		memory->write<math::vector2>(address + Offsets::MouseService::MousePosition, new_position);

		std::uint64_t input_obj_1 = memory->read<std::uint64_t>(address + Offsets::MouseService::InputObject);
		std::uint64_t input_obj_2 = memory->read<std::uint64_t>(address + Offsets::MouseService::InputObject2);

		if (input_obj_1 != 0 && input_obj_1 != 0xFFFFFFFFFFFFFFFF)
		{
			memory->write<math::vector2>(input_obj_1 + Offsets::MouseService::MousePosition, new_position);
		}

		if (input_obj_2 != 0 && input_obj_2 != 0xFFFFFFFFFFFFFFFF)
		{
			memory->write<math::vector2>(input_obj_2 + Offsets::MouseService::MousePosition, new_position);
		}
	}
}

void rbx::silent::run()
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	std::uint64_t last_locked_address = 0;
	int aim_frame_check_counter = 0;

	for (;;)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(8));

		if (!settings::silent::enabled || !game::datamodel.address || !game::visengine.address)
		{
			{
				std::lock_guard<std::mutex> lock(g_silent_aim_mutex);
				if (!g_silent_aim_manual_locked) {
					g_silent_aim_locked = false;
					g_silent_cached_target = {};
				}
				g_silent_data_ready = false;
			}
			last_locked_address = 0;
			continue;
		}

		if (!g_mouseservice || g_mouseservice->address == 0)
		{
			std::uint64_t mouse_svc_addr = game::datamodel.find_first_child_by_class("MouseService").address;
			if (mouse_svc_addr != 0)
			{
				g_mouseservice = std::make_unique<rbx::instance_t>(mouse_svc_addr);
			}
		}

		if (!g_mouseservice || g_mouseservice->address == 0)
		{
			continue;
		}

		if (aim_frame_check_counter++ % 250 == 0)
		{
			if (game::local_player.address != 0)
			{
				rbx::instance_t player_gui = game::local_player.find_first_child("PlayerGui");
				if (player_gui.address != 0)
				{
					rbx::instance_t aim_frame = player_gui.find_first_child("Aim");
					if (aim_frame.address != 0)
					{
						g_silent_aim_instance = aim_frame;
					}
					else
					{
						rbx::instance_t main_gui = player_gui.find_first_child("Main");
						if (main_gui.address != 0)
						{
							rbx::instance_t main_aim = main_gui.find_first_child("Aim");
							if (main_aim.address != 0)
							{
								g_silent_aim_instance = main_aim;
							}
						}
					}
				}
			}
		}

		POINT cursor_point;
		HWND roblox_window = FindWindowA(nullptr, "Roblox");
		if (!roblox_window || !GetCursorPos(&cursor_point) || !ScreenToClient(roblox_window, &cursor_point))
		{
			continue;
		}

		math::vector2 dims = game::visengine.get_dimensions();
		math::matrix4 view = game::visengine.get_viewmatrix();

		bool should_active = false;
		if (!check::textchatopen)
		{
			if (settings::silent::keybind == 0)
			{
				should_active = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
			}
			else
			{
				bool key_down = (GetAsyncKeyState(settings::silent::keybind) & 0x8000) != 0;
				if (settings::silent::keybind_mode == 0)
				{
					should_active = key_down;
				}
				else if (settings::silent::keybind_mode == 1)
				{
					static bool was_pressed = false;
					static bool toggle_active = false;
					if (key_down && !was_pressed)
					{
						toggle_active = !toggle_active;
					}
					was_pressed = key_down;
					should_active = toggle_active;
				}
				else if (settings::silent::keybind_mode == 2)
				{
					should_active = true;
				}
			}
		}

		cache::entity_t current_target = {};
		bool target_acquired = false;

		bool manual_lock_active = false;
		{
			std::lock_guard<std::mutex> lock(g_silent_aim_mutex);
			if (g_silent_aim_manual_locked && g_silent_cached_target.instance.address != 0)
			{
				manual_lock_active = true;
			}
		}

		if (manual_lock_active)
		{
			cache::entity_t manual_target = {};
			bool found_manual = false;
			{
				std::lock_guard<std::mutex> cache_lock(cache::mtx);
				if (cache::cached_players)
				{
					for (const auto& player : *cache::cached_players)
					{
						std::lock_guard<std::mutex> lock(g_silent_aim_mutex);
						if (player.instance.address == g_silent_cached_target.instance.address)
						{
							manual_target = player;
							found_manual = true;
							break;
						}
					}
				}
			}

			if (found_manual && is_target_valid(manual_target, cursor_point, dims, view, "", true))
			{
				current_target = manual_target;
				target_acquired = true;
			}
			else
			{
				std::lock_guard<std::mutex> lock(g_silent_aim_mutex);
				g_silent_aim_locked = false;
				g_silent_aim_manual_locked = false;
				g_silent_cached_target = {};
			}
		}

		if (!target_acquired && settings::silent::sticky_aim && last_locked_address != 0)
		{
			std::lock_guard<std::mutex> cache_lock(cache::mtx);
			std::string local_tool = cache::cached_local_player.tool_name;
			if (cache::cached_players)
			{
				for (const auto& player : *cache::cached_players)
				{
					if (player.instance.address == last_locked_address)
					{
						if (is_target_valid(player, cursor_point, dims, view, local_tool, false))
						{
							current_target = player;
							target_acquired = true;
						}
						break;
					}
				}
			}
		}

		if (!target_acquired)
		{
			float closest_dist = std::numeric_limits<float>::max();
			std::lock_guard<std::mutex> cache_lock(cache::mtx);
			std::string local_tool = cache::cached_local_player.tool_name;
			if (cache::cached_players)
			{
				for (const auto& player : *cache::cached_players)
				{
					if (cache::is_local_player(player)) continue;

					if (is_target_valid(player, cursor_point, dims, view, local_tool, false))
					{
						rbx::part_t target_part = get_target_part(player, settings::silent::aim_part, cursor_point, dims, view);
						if (target_part.address != 0)
						{
							rbx::primitive_t primitive = target_part.get_primitive();
							if (primitive.address != 0)
							{
								math::vector3 world_pos = primitive.get_position();
								math::vector2 screen_pos = {};
								if (game::visengine.world_to_screen(world_pos, screen_pos, dims, view))
								{
									float dist = get_magnitude(screen_pos, { static_cast<float>(cursor_point.x), static_cast<float>(cursor_point.y) });
									if (dist < closest_dist)
									{
										closest_dist = dist;
										current_target = player;
										target_acquired = true;
									}
								}
							}
						}
					}
				}
			}
		}

		if (target_acquired && should_active)
		{
			rbx::part_t target_part = get_target_part(current_target, settings::silent::aim_part, cursor_point, dims, view);
			if (target_part.address != 0)
			{
				rbx::primitive_t prim = target_part.get_primitive();
				if (prim.address != 0)
				{
					math::vector3 part_3d = prim.get_position();
					math::vector2 screen_pos = {};
					if (game::visengine.world_to_screen(part_3d, screen_pos, dims, view))
					{
						write_mouse_position(g_mouseservice->address, screen_pos.x, screen_pos.y);

						{
							std::lock_guard<std::mutex> lock(g_silent_aim_mutex);
							g_silent_cached_target = current_target;
							g_silent_aim_locked = true;
							g_silent_partpos = screen_pos;
							g_silent_data_ready = true;
						}
						last_locked_address = current_target.instance.address;
						continue;
					}
				}
			}
		}

		{
			std::lock_guard<std::mutex> lock(g_silent_aim_mutex);
			if (!g_silent_aim_manual_locked)
			{
				g_silent_aim_locked = false;
				g_silent_cached_target = {};
			}
			g_silent_data_ready = false;
		}
		last_locked_address = 0;
	}
}

void rbx::silent::initialize()
{
	std::thread(run).detach();
}