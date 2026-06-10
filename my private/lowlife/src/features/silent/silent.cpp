#define NOMINMAX
#include <Windows.h>
#include <thread>
#include <vector>
#include <algorithm>
#include <immintrin.h>
#include <cmath>
#include <limits>
#include <unordered_set>

#include <memory/memory.h>
#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <cache/cache.h>
#include <game/game.h>
#include <settings.h>
#include <check/typing_check.h>
#include "../triggerbot/triggerbot.h"
#include "silent.h"

std::uint64_t c_silent_helper::cached_input_object = 0;

static math::vector2 world_to_screen(math::vector3 world, math::vector2 dimensions, math::matrix4 viewmatrix)
{
	float clipX = world.x * viewmatrix.m[0][0] + world.y * viewmatrix.m[0][1] + world.z * viewmatrix.m[0][2] + viewmatrix.m[0][3];
	float clipY = world.x * viewmatrix.m[1][0] + world.y * viewmatrix.m[1][1] + world.z * viewmatrix.m[1][2] + viewmatrix.m[1][3];
	float clipZ = world.x * viewmatrix.m[2][0] + world.y * viewmatrix.m[2][1] + world.z * viewmatrix.m[2][2] + viewmatrix.m[2][3];
	float clipW = world.x * viewmatrix.m[3][0] + world.y * viewmatrix.m[3][1] + world.z * viewmatrix.m[3][2] + viewmatrix.m[3][3];

	if (clipW <= 1e-6f)
		return { -1.0f, -1.0f };

	float inv_w = 1.0f / clipW;
	float ndcX = clipX * inv_w;
	float ndcY = clipY * inv_w;

	return {
		(dimensions.x / 2.0f) * (ndcX + 1.0f),
		(dimensions.y / 2.0f) * (1.0f - ndcY)
	};
}

static float get_magnitude(const math::vector2& a, const math::vector2& b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return std::sqrt(dx * dx + dy * dy);
}

static float get_effective_fov()
{
	if (!settings::silent::gun_based_fov)
		return settings::silent::fov;

	std::string tool_name;
	{
		std::lock_guard<std::mutex> lock(cache::mtx);
		tool_name = cache::cached_local_player.tool_name;
	}

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

static rbx::part_t get_target_part(cache::entity_t& player, int aim_part)
{
	rbx::part_t target_part{};
	
	if (aim_part == 0)
	{
		auto head_it = player.parts.find("Head");
		if (head_it != player.parts.end())
			target_part = head_it->second;
	}
	else if (aim_part == 1)
	{
		auto torso_it = player.parts.find("UpperTorso");
		if (torso_it != player.parts.end())
			target_part = torso_it->second;
		else
		{
			auto torso_r6 = player.parts.find("Torso");
			if (torso_r6 != player.parts.end())
				target_part = torso_r6->second;
		}
	}
	else if (aim_part == 2)
	{
		POINT cursor_point;
		HWND rblxWnd = FindWindowA(nullptr, "Roblox");
		if (rblxWnd && GetCursorPos(&cursor_point) && ScreenToClient(rblxWnd, &cursor_point))
		{
			math::vector2 cursor = { static_cast<float>(cursor_point.x), static_cast<float>(cursor_point.y) };
			math::vector2 dimensions = game::visengine.get_dimensions();
			math::matrix4 viewmatrix = game::visengine.get_viewmatrix();
			
			float shortest_distance = std::numeric_limits<float>::max();

			const std::unordered_set<std::string> valid_hitparts = {
				"Head",
				"Torso", "UpperTorso", "LowerTorso",
				"Left Arm", "LeftUpperArm", "LeftLowerArm", "LeftHand",
				"Right Arm", "RightUpperArm", "RightLowerArm", "RightHand",
				"Left Leg", "LeftUpperLeg", "LeftLowerLeg", "LeftFoot",
				"Right Leg", "RightUpperLeg", "RightLowerLeg", "RightFoot",
				"HumanoidRootPart"
			};
			
			for (auto& part_pair : player.parts)
			{
				if (valid_hitparts.find(part_pair.first) == valid_hitparts.end()) continue;
				rbx::primitive_t prim = part_pair.second.get_primitive();
				if (!prim.address)
					continue;
					
				math::vector3 part_position = prim.get_position();
				math::vector2 part_screen = world_to_screen(part_position, dimensions, viewmatrix);
				
				if (part_screen.x < 0 || part_screen.y < 0)
					continue;
					
				float distance = get_magnitude(part_screen, cursor);
				if (distance < shortest_distance)
				{
					shortest_distance = distance;
					target_part = part_pair.second;
				}
			}
		}
	}
	
	return target_part;
}

static bool is_player_knocked(cache::entity_t& player)
{
	if (player.is_knocked) return true;
	if (player.humanoid.address != 0) {
		try {
			float health = const_cast<cache::entity_t&>(player).humanoid.get_health();
			if (health <= 0.0f || !std::isfinite(health)) {
				return true;
			}
		} catch (...) {}
	}
	
	if (player.ko_address != 0) {
		try {
			return memory->read<bool>(player.ko_address + Offsets::Misc::Value);
		} catch (...) {
			return false;
		}
	}
	return false;
}

static bool is_target_within_fov(cache::entity_t& player)
{
	if (player.instance.address == 0)
		return false;

	POINT cursor_point;
	HWND rblxWnd = FindWindowA(nullptr, "Roblox");
	if (!rblxWnd)
		return false;

	if (!GetCursorPos(&cursor_point))
		return false;

	if (!ScreenToClient(rblxWnd, &cursor_point))
		return false;

	math::vector2 cursor = { static_cast<float>(cursor_point.x), static_cast<float>(cursor_point.y) };

	rbx::part_t target_part = get_target_part(player, settings::silent::aim_part);
	if (!target_part.address)
		return false;

	rbx::primitive_t prim = target_part.get_primitive();
	if (!prim.address)
		return false;

	math::vector2 dimensions = game::visengine.get_dimensions();
	math::matrix4 viewmatrix = game::visengine.get_viewmatrix();

	math::vector3 part_position = prim.get_position();
	math::vector2 part_screen = world_to_screen(part_position, dimensions, viewmatrix);

	if (part_screen.x < 0 || part_screen.y < 0)
		return false;

	float distance_from_cursor = get_magnitude(part_screen, cursor);
	return distance_from_cursor <= get_effective_fov();
}

static cache::entity_t get_closest_player_from_cursor()
{
	POINT cursor_point;
	HWND rblxWnd = FindWindowA(nullptr, "Roblox");
	if (!rblxWnd)
		return {};

	if (!GetCursorPos(&cursor_point))
		return {};

	if (!ScreenToClient(rblxWnd, &cursor_point))
		return {};

	math::vector2 cursor = { static_cast<float>(cursor_point.x), static_cast<float>(cursor_point.y) };

	std::shared_ptr<std::vector<cache::entity_t>> players_snapshot;
	{
		std::lock_guard<std::mutex> lock(cache::mtx);
		players_snapshot = cache::cached_players;
	}

	if (!players_snapshot || players_snapshot->empty())
		return {};

	cache::entity_t closest_player{};
	float shortest_distance = std::numeric_limits<float>::max();

	math::vector2 dimensions = game::visengine.get_dimensions();
	math::matrix4 viewmatrix = game::visengine.get_viewmatrix();

	for (auto& player : *players_snapshot)
	{
		if (player.instance.address == 0)
			continue;

		if (player.instance.address == game::local_player.address)
			continue;

		
		auto rel_it = settings::player_relations::relations.find(player.name);
		if (rel_it != settings::player_relations::relations.end() && rel_it->second == 1) {
			continue;
		}

		rbx::part_t target_part = get_target_part(player, settings::silent::aim_part);

		if (!target_part.address)
			continue;

		rbx::primitive_t prim = target_part.get_primitive();
		if (!prim.address)
			continue;

		math::vector3 part_position = prim.get_position();
		math::vector2 part_screen = world_to_screen(part_position, dimensions, viewmatrix);

		if (part_screen.x < 0 || part_screen.y < 0)
			continue;

		float distance_from_cursor = get_magnitude(part_screen, cursor);

		if (settings::silent::fov_check && distance_from_cursor > get_effective_fov())
			continue;

		if (settings::silent::knocked_check && is_player_knocked(player))
			continue;

		if (settings::silent::wall_check && !settings::silent::magic_bullet)
		{
			rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
			if (camera_inst.address != 0)
			{
				rbx::camera_t camera{ camera_inst.address };
				math::vector3 camera_pos = camera.get_position();
				
				bool any_part_visible = false;
				const std::unordered_set<std::string> target_parts_to_check = {
					"Head", "Torso", "UpperTorso", "LowerTorso",
					"Left Arm", "LeftUpperArm", "LeftLowerArm", "LeftHand",
					"Right Arm", "RightUpperArm", "RightLowerArm", "RightHand",
					"Left Leg", "LeftUpperLeg", "LeftLowerLeg", "LeftFoot",
					"Right Leg", "RightUpperLeg", "RightLowerLeg", "RightFoot",
					"HumanoidRootPart"
				};
				
				for (const auto& pair : player.parts)
				{
					if (target_parts_to_check.find(pair.first) == target_parts_to_check.end()) continue;
					rbx::part_t part = pair.second;
					if (!part.address) continue;
					rbx::primitive_t primitive = part.get_primitive();
					if (!primitive.address) continue;
					math::vector3 world_pos = primitive.get_position();
					if (!botter::is_occluded(camera_pos, world_pos))
					{
						any_part_visible = true;
						break;
					}
				}
				
				if (!any_part_visible)
				{
					continue;
				}
			}
		}

		if (distance_from_cursor < shortest_distance)
		{
			shortest_distance = distance_from_cursor;
			closest_player = player;
		}
	}

	return closest_player;
}

static std::uint64_t get_current_input_object(std::uint64_t base_address)
{
	if (base_address == 0 || base_address == 0xFFFFFFFFFFFFFFFF)
		return 0;
	return memory->read<std::uint64_t>(base_address + Offsets::MouseService::InputObject2);
}

void c_silent_helper::set_frame_pos_x(std::uint64_t position)
{
	
}

void c_silent_helper::set_frame_pos_y(std::uint64_t position)
{
	
}

void c_silent_helper::initialize_mouse_service(std::uint64_t address)
{
	cached_input_object = get_current_input_object(address);
}

void c_silent_helper::write_mouse_position(std::uint64_t address, float x, float y) 
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

static bool should_silent_aim_be_active()
{
	if (!settings::silent::enabled)
		return false;

	if (g_silent_aim_locked || g_silent_aim_manual_locked || (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
		return true;

	return false;
}

static void update_silent_aim_key_state()
{
	if (settings::silent::keybind == 0)
		return;

	static bool was_disabled_by_typing = false;

	if (check::textchatopen)
	{
		was_disabled_by_typing = true;
		if (!g_silent_aim_manual_locked)
		{
			g_silent_aim_locked = false;
			g_silent_cached_target = {};
			g_silent_found_target = false;
			g_silent_data_ready = false;
			g_silent_locked_part_name = "";
		}
		return;
	}

	if (was_disabled_by_typing && !check::textchatopen)
	{
		if (!g_silent_aim_manual_locked)
			g_silent_aim_locked = false;
		was_disabled_by_typing = false;
	}

	bool key_currently_pressed = (GetAsyncKeyState(settings::silent::keybind) & 0x8000) != 0;

	if (settings::silent::keybind_mode == 0)
	{
		if (key_currently_pressed && !g_silent_aim_locked)
		{
			g_silent_aim_locked = true;
		}
		else if (!key_currently_pressed && g_silent_aim_locked && !g_silent_aim_manual_locked)
		{
			g_silent_aim_locked = false;
			g_silent_cached_target = {};
			g_silent_found_target = false;
			g_silent_data_ready = false;
			g_silent_locked_part_name = "";
		}
	}
	else
	{
		if (key_currently_pressed && !g_silent_aim_key_was_pressed)
		{
			if (!g_silent_aim_locked)
			{
				g_silent_aim_locked = true;
			}
			else if (!g_silent_aim_manual_locked)
			{
				g_silent_aim_locked = false;
				g_silent_cached_target = {};
				g_silent_found_target = false;
				g_silent_data_ready = false;
				g_silent_locked_part_name = "";
			}
		}
	}

	g_silent_aim_key_was_pressed = key_currently_pressed;
}

void rbx::silent::silent_aim_1()
{
	cache::entity_t target{};

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	HWND roblox_window = FindWindowA(0, "Roblox");

	for (;;)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		if (g_mouseservice == nullptr || g_mouseservice->address == 0)
		{
			g_mouseservice = std::make_unique<rbx::instance_t>(game::datamodel.find_first_child_by_class("MouseService"));
		}

		if (!g_mouseservice || g_mouseservice->address == 0 || !game::datamodel.address || !game::visengine.address)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		update_silent_aim_key_state();

		
		if (g_silent_aim_instance.address && g_silent_has_original_sizes)
		{
			if (settings::silent::enabled)
			{
				memory->write<math::vector2>(g_silent_aim_instance.address + Offsets::GuiObject::Size, { 0, 0 });
				std::vector<rbx::instance_t> aim_children = g_silent_aim_instance.get_children();
				for (rbx::instance_t& child : aim_children)
				{
					if (child.address)
					{
						memory->write<math::vector2>(child.address + Offsets::GuiObject::Size, { 0, 0 });
					}
				}
			}
			else
			{
				memory->write<math::vector2>(g_silent_aim_instance.address + Offsets::GuiObject::Size, g_silent_original_size);
				for (const auto& [child_addr, original_size] : g_silent_original_children_sizes)
				{
					memory->write<math::vector2>(child_addr + Offsets::GuiObject::Size, original_size);
				}
			}
		}

		if (!settings::silent::enabled)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			g_silent_data_ready = false;
			if (g_silent_cached_target.instance.address != 0 && !g_silent_aim_manual_locked)
			{
				g_silent_cached_target = {};
				g_silent_locked_part_name = "";
			}
			target = {};
			g_silent_found_target = false;
			g_silent_target_needs_reset = false;
			continue;
		}

		if (!game::datamodel.address || game::players.address == 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		rbx::instance_t local_player = game::local_player;

		if (local_player.address == 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		static int aim_instance_check_counter = 0;
		if (aim_instance_check_counter++ % 10 == 0)
		{
			try
			{
				rbx::instance_t player_gui = local_player.find_first_child("PlayerGui");
				if (player_gui.address != 0)
				{
					rbx::instance_t aim_frame{};
					std::vector<rbx::instance_t> children = player_gui.get_children();

					for (rbx::instance_t& child : children)
					{
						if (!child.address)
							continue;

						std::string child_name = child.get_name();
						if (child_name == "Aim")
						{
							aim_frame = child;
							break;
						}

						std::string child_class = child.get_class_name();
						if (child_class == "Frame" || child_class == "ScreenGui" || child_class == "GuiObject")
						{
							std::string child_lower = child_name;
							std::transform(child_lower.begin(), child_lower.end(), child_lower.begin(), ::tolower);

							if (child_lower.find("main") != std::string::npos)
							{
								std::vector<rbx::instance_t> grandchildren = child.get_children();
								for (rbx::instance_t& grandchild : grandchildren)
								{
									if (grandchild.address)
									{
										std::string grandchild_name = grandchild.get_name();
										if (grandchild_name == "Aim")
										{
											aim_frame = grandchild;
											break;
										}
									}
								}

								if (aim_frame.address)
									break;
							}
						}
					}

					if (aim_frame.address != g_silent_aim_instance.address)
					{
						if (g_silent_aim_instance.address && g_silent_has_original_sizes)
						{
							memory->write<math::vector2>(g_silent_aim_instance.address + Offsets::GuiObject::Size, g_silent_original_size);
							for (const auto& [child_addr, original_size] : g_silent_original_children_sizes)
							{
								memory->write<math::vector2>(child_addr + Offsets::GuiObject::Size, original_size);
							}
						}

						g_silent_aim_instance = aim_frame;
						g_silent_has_original_sizes = false;
						g_silent_original_children_sizes.clear();

						if (g_silent_aim_instance.address)
						{
							g_silent_original_size = memory->read<math::vector2>(g_silent_aim_instance.address + Offsets::GuiObject::Size);
							std::vector<rbx::instance_t> aim_children = g_silent_aim_instance.get_children();
							for (rbx::instance_t& child : aim_children)
							{
								if (child.address)
								{
									math::vector2 child_size = memory->read<math::vector2>(child.address + Offsets::GuiObject::Size);
									g_silent_original_children_sizes.push_back({ child.address, child_size });
								}
							}
							g_silent_has_original_sizes = true;
						}
					}
				}
			}
			catch (...)
			{
				g_silent_aim_instance = rbx::instance_t{};
			}
		}

		
		if (g_silent_found_target && g_silent_cached_target.instance.address != 0)
		{
			bool found = false;
			std::shared_ptr<std::vector<cache::entity_t>> players_snapshot;
			{
				std::lock_guard<std::mutex> cache_lock(cache::mtx);
				players_snapshot = cache::cached_players;
			}
			if (players_snapshot)
			{
				for (const auto& player : *players_snapshot)
				{
					if (player.instance.address == g_silent_cached_target.instance.address)
					{
						g_silent_cached_target = player;
						found = true;
						break;
					}
				}
			}
			if (!found)
			{
				g_silent_cached_target = {};
				g_silent_found_target = false;
				g_silent_locked_part_name = "";
			}
		}

		bool always_mode = (settings::silent::keybind_mode == 2);
		
		if (!g_silent_found_target || g_silent_cached_target.instance.address == 0)
		{
			target = get_closest_player_from_cursor();
			
			g_silent_cached_last_target = target;
			
			rbx::part_t target_part{};
			if (settings::silent::aim_part == 2) 
			{
				rbx::part_t closest = get_target_part(target, 2);
				if (closest.address != 0)
				{
					g_silent_locked_part_name = closest.get_name();
					target_part = closest;
				}
			}
			else
			{
				target_part = get_target_part(target, settings::silent::aim_part);
			}

			g_silent_found_target = (target_part.address != 0);
			g_silent_cached_target = target;
		}
		else
		{
			if (always_mode || !settings::silent::sticky_aim)
			{
				target = get_closest_player_from_cursor();
				
				g_silent_cached_last_target = target;
				
				rbx::part_t target_part{};
				if (settings::silent::aim_part == 2) 
				{
					rbx::part_t closest = get_target_part(target, 2);
					if (closest.address != 0)
					{
						g_silent_locked_part_name = closest.get_name();
						target_part = closest;
					}
				}
				else
				{
					target_part = get_target_part(target, settings::silent::aim_part);
				}

				g_silent_found_target = (target_part.address != 0);
				g_silent_cached_target = target;
			}
			else
			{
				target = g_silent_cached_target;
				
				if (settings::silent::fov_check && game::visengine.address)
				{
					if (!is_target_within_fov(g_silent_cached_target))
					{
						g_silent_found_target = false;
						if (!g_silent_aim_manual_locked)
						{
							g_silent_cached_target = {};
							g_silent_locked_part_name = "";
						}
						target = {};
						continue;
					}
				}
			}
		}

		if (g_silent_found_target && g_silent_cached_target.instance.address != 0 && game::visengine.address)
		{
			if (settings::silent::knocked_check && is_player_knocked(g_silent_cached_target))
			{
				g_silent_found_target = false;
				if (!g_silent_aim_manual_locked)
				{
					g_silent_cached_target = {};
					g_silent_locked_part_name = "";
				}
				continue;
			}

			if (settings::silent::wall_check && !settings::silent::magic_bullet)
			{
				rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
				if (camera_inst.address != 0)
				{
					rbx::camera_t camera{ camera_inst.address };
					math::vector3 camera_pos = camera.get_position();
					
					bool any_part_visible = false;
					const std::unordered_set<std::string> target_parts_to_check = {
						"Head", "Torso", "UpperTorso", "LowerTorso",
						"Left Arm", "LeftUpperArm", "LeftLowerArm", "LeftHand",
						"Right Arm", "RightUpperArm", "RightLowerArm", "RightHand",
						"Left Leg", "LeftUpperLeg", "LeftLowerLeg", "LeftFoot",
						"Right Leg", "RightUpperLeg", "RightLowerLeg", "RightFoot",
						"HumanoidRootPart"
					};
					
					for (const auto& pair : g_silent_cached_target.parts)
					{
						if (target_parts_to_check.find(pair.first) == target_parts_to_check.end()) continue;
						rbx::part_t part = pair.second;
						if (!part.address) continue;
						rbx::primitive_t primitive = part.get_primitive();
						if (!primitive.address) continue;
						math::vector3 world_pos = primitive.get_position();
						if (!botter::is_occluded(camera_pos, world_pos))
						{
							any_part_visible = true;
							break;
						}
					}
					
					if (!any_part_visible)
					{
						g_silent_found_target = false;
						if (!g_silent_aim_manual_locked)
						{
							g_silent_cached_target = {};
							g_silent_locked_part_name = "";
						}
						continue;
					}
				}
			}

			rbx::part_t target_part{};
			if (settings::silent::aim_part == 2) 
			{
				if (!g_silent_locked_part_name.empty() && 
					g_silent_cached_target.parts.find(g_silent_locked_part_name) != g_silent_cached_target.parts.end() &&
					g_silent_cached_target.parts[g_silent_locked_part_name].address != 0)
				{
					target_part = g_silent_cached_target.parts[g_silent_locked_part_name];
				}
				else
				{
					rbx::part_t closest = get_target_part(g_silent_cached_target, 2);
					if (closest.address != 0)
					{
						g_silent_locked_part_name = closest.get_name();
						target_part = closest;
					}
				}
			}
			else
			{
				target_part = get_target_part(g_silent_cached_target, settings::silent::aim_part);
			}
			
			if (target_part.address != 0)
			{
				rbx::primitive_t prim = target_part.get_primitive();
				if (prim.address)
				{
					math::vector3 part_3d = prim.get_position();
					math::matrix4 view = game::visengine.get_viewmatrix();
					math::vector2 dims = game::visengine.get_dimensions();

					g_silent_partpos = world_to_screen(part_3d, dims, view);
					POINT cursor_point;
					GetCursorPos(&cursor_point);
					if (roblox_window)
						ScreenToClient(roblox_window, &cursor_point);

					g_silent_cached_position_x = static_cast<std::uint64_t>(cursor_point.x);
					g_silent_cached_position_y = static_cast<std::uint64_t>(cursor_point.y);
					g_silent_data_ready = true;
				}
				else
				{
					g_silent_data_ready = false;
				}
			}
			else
			{
				g_silent_data_ready = false;
			}
		}
		else
		{
			g_silent_data_ready = false;
		}
	}
}

void rbx::silent::silent_aim_2()
{
	c_silent_helper mouse_service_instance{};
	bool mouse_service_initialized = false;

	for (;;)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1)); 

		if (!g_mouseservice)
		{
			mouse_service_initialized = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		if (!should_silent_aim_be_active())
		{
			mouse_service_initialized = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			if (g_silent_cached_target.instance.address != 0)
				g_silent_target_needs_reset = true;
			continue;
		}

		if (g_silent_cached_target.instance.address != 0 && g_silent_data_ready && g_mouseservice && g_mouseservice->address != 0)
		{
			if (g_silent_partpos.x < 0.0f || g_silent_partpos.y < 0.0f ||
				g_silent_partpos.x > 10000.0f || g_silent_partpos.y > 10000.0f)
			{
				continue;
			}

			try
			{
				if (!mouse_service_initialized)
				{
					mouse_service_instance.initialize_mouse_service(g_mouseservice->address);
					mouse_service_initialized = true;
				}

				mouse_service_instance.write_mouse_position(g_mouseservice->address, g_silent_partpos.x, g_silent_partpos.y);
			}
			catch (...)
			{
				mouse_service_initialized = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
	}
}

void rbx::silent::initialize()
{
	std::thread(silent_aim_1).detach();
	std::thread(silent_aim_2).detach();
}