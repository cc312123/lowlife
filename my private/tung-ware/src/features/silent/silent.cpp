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
#include <random>

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

	bool is_player_visible(const cache::entity_t& player)
	{
		if (game::workspace.address == 0) return true;
		rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
		if (camera_inst.address != 0) {
			rbx::camera_t camera{ camera_inst.address };
			math::vector3 camera_pos = camera.get_position();
			
			const std::unordered_set<std::string> target_parts = {
				"Head", "UpperTorso", "LowerTorso", "HumanoidRootPart"
			};

			for (const auto& pair : player.parts) {
				if (target_parts.find(pair.first) == target_parts.end()) continue;
				rbx::part_t part = pair.second;
				if (!part.address) continue;
				rbx::primitive_t primitive = part.get_primitive();
				if (!primitive.address) continue;
				math::vector3 world_pos = primitive.get_position();
				if (!botter::is_occluded(camera_pos, world_pos)) {
					return true;
				}
			}
			return false;
		}
		return true;
	}

	bool is_on_same_team(const cache::entity_t& player, const std::string& local_crew_id)
	{
		if (local_crew_id.empty() || player.crew_id.empty()) return false;
		if (local_crew_id == "0" || player.crew_id == "0") return false;
		return local_crew_id == player.crew_id;
	}

	rbx::part_t get_target_part(const cache::entity_t& player, int aim_part, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view)
	{
		if (player.parts.empty()) return rbx::part_t{};

		
		if (aim_part == 4) 
		{
			if (game::workspace.address != 0)
			{
				rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
				if (camera_inst.address != 0)
				{
					rbx::camera_t camera{ camera_inst.address };
					math::vector3 camera_pos = camera.get_position();
					
					auto head_it = player.parts.find("Head");
					if (head_it != player.parts.end() && head_it->second.address != 0)
					{
						rbx::part_t head_part = head_it->second;
						rbx::primitive_t head_prim = head_part.get_primitive();
						if (head_prim.address != 0 && !botter::is_occluded(camera_pos, head_prim.get_position()))
						{
							return head_it->second;
						}
					}
					
					auto torso_it = player.parts.find("UpperTorso");
					if (torso_it != player.parts.end()) return torso_it->second;
					torso_it = player.parts.find("Torso");
					if (torso_it != player.parts.end()) return torso_it->second;
				}
			}
			
			auto head_it = player.parts.find("Head");
			if (head_it != player.parts.end()) return head_it->second;
		}
		
		else if (aim_part == 5) 
		{
			std::vector<std::string> parts_list = { "Head", "UpperTorso", "LowerTorso", "HumanoidRootPart" };
			std::random_device rd;
			std::mt19937 g(rd());
			std::shuffle(parts_list.begin(), parts_list.end(), g);
			
			for (const auto& name : parts_list)
			{
				auto it = player.parts.find(name);
				if (it != player.parts.end() && it->second.address != 0)
				{
					return it->second;
				}
			}
		}
		else if (aim_part == 0) 
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
			auto it = player.parts.find("LowerTorso");
			if (it != player.parts.end()) return it->second;
			it = player.parts.find("Torso");
			if (it != player.parts.end()) return it->second;
		}
		else if (aim_part == 3) 
		{
			auto it = player.parts.find("HumanoidRootPart");
			if (it != player.parts.end()) return it->second;
		}

		
		auto it = player.parts.find("Head");
		if (it != player.parts.end()) return it->second;
		
		return rbx::part_t{};
	}

	bool is_target_valid(const cache::entity_t& player, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view, bool skip_fov_check, bool skip_wall_check = false)
	{
		if (player.instance.address == 0) return false;

		
		bool relation_invalid = false;
		{
			std::lock_guard<std::mutex> lock(settings::player_relations::relations_mutex);
			auto rel_it = settings::player_relations::relations.find(player.name);
			if (rel_it != settings::player_relations::relations.end() && rel_it->second == 1) {
				relation_invalid = true;
			}
		}
		if (relation_invalid) return false;

		
		if (settings::new_silent::team_check)
		{
			if (is_on_same_team(player, cache::cached_local_player.crew_id)) return false;
		}

		
		if (settings::new_silent::knocked_check && is_player_knocked(player))
		{
			return false;
		}

		
		if (settings::new_silent::wall_check && !skip_wall_check && !is_player_visible(player))
		{
			return false;
		}

		
		if (settings::new_silent::fov_check && !skip_fov_check)
		{
			rbx::part_t target_part = get_target_part(player, settings::new_silent::aim_part, cursor_pt, dims, view);
			if (target_part.address == 0) return false;

			rbx::primitive_t primitive = target_part.get_primitive();
			if (primitive.address == 0) return false;

			math::vector3 world_pos = primitive.get_position();
			math::vector2 screen_pos = {};

			if (!game::visengine.world_to_client(world_pos, screen_pos, dims, view)) return false;

			float cursor_x = static_cast<float>(cursor_pt.x);
			float cursor_y = static_cast<float>(cursor_pt.y);
			float dist = get_magnitude(screen_pos, { cursor_x, cursor_y });
			if (dist > settings::new_silent::fov) return false;
		}

		return true;
	}

	math::vector3 apply_prediction(rbx::primitive_t primitive)
	{
		math::vector3 pos = primitive.get_position();
		math::vector3 vel = primitive.get_velocity();

		constexpr float PREDICTION_SCALE = 0.016f;
		constexpr float MAX_VELOCITY = 1000.0f;

		if (!std::isfinite(vel.x) || !std::isfinite(vel.y) || !std::isfinite(vel.z) ||
			std::abs(vel.x) > MAX_VELOCITY || std::abs(vel.y) > MAX_VELOCITY || std::abs(vel.z) > MAX_VELOCITY)
			return pos;

		float px = settings::new_silent::prediction_scale_x;
		float py = settings::new_silent::prediction_scale_y;

		if (settings::new_silent::auto_prediction)
		{
			
			
			float ping = settings::aimbot::latency_ms;
			if (ping <= 0.0f) ping = 50.0f;
			
			
			float auto_scale = ping * 0.02f; 
			px = auto_scale;
			py = auto_scale;
		}

		pos.x += vel.x * PREDICTION_SCALE * px;
		pos.y += vel.y * PREDICTION_SCALE * py;
		pos.z += vel.z * PREDICTION_SCALE * px;
		return pos;
	}

	void write_mouse_position(std::uint64_t address, float x, float y)
	{
		if (address == 0 || address == 0xFFFFFFFFFFFFFFFF)
			return;

		math::vector2 new_position = { x, y };

		memory->write<math::vector2>(address + Offsets::MouseService::MousePosition, new_position);
	}
}

void rbx::new_silent::run()
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	std::uint64_t last_locked_address = 0;
	bool silent_needs_key_release = false;
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(1, 100);

	for (;;)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(8));

		if (!settings::new_silent::enabled || !game::datamodel.address || !game::visengine.address || !game::workspace.address)
		{
			{
				std::lock_guard<std::mutex> lock(g_silent_aim_mutex);
				g_silent_aim_locked = false;
				g_silent_cached_target = {};
			}
			last_locked_address = 0;
			silent_needs_key_release = false;
			continue;
		}

		std::uint64_t mouse_svc_addr = game::datamodel.find_first_child_by_class("MouseService").address;
		if (mouse_svc_addr == 0)
		{
			continue;
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
			if (settings::new_silent::keybind == 0)
			{
				should_active = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
			}
			else
			{
				bool key_down = (GetAsyncKeyState(settings::new_silent::keybind) & 0x8000) != 0;
				if (settings::new_silent::keybind_mode == 0) 
				{
					should_active = key_down;
				}
				else if (settings::new_silent::keybind_mode == 1) 
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
				else if (settings::new_silent::keybind_mode == 2) 
				{
					should_active = true;
				}
			}
		}

		if (!should_active)
		{
			silent_needs_key_release = false;
			last_locked_address = 0;
			{
				std::lock_guard<std::mutex> lock(g_silent_aim_mutex);
				g_silent_aim_locked = false;
				g_silent_cached_target = {};
			}
			continue;
		}

		if (silent_needs_key_release)
		{
			continue;
		}

		// 1. Check if the previously locked target died/got knocked/left
		bool locked_target_still_exists = false;
		cache::entity_t updated_locked_target = {};
		if (last_locked_address != 0)
		{
			std::lock_guard<std::mutex> cache_lock(cache::mtx);
			if (cache::cached_players)
			{
				for (const auto& player : *cache::cached_players)
				{
					if (player.instance.address == last_locked_address)
					{
						updated_locked_target = player;
						locked_target_still_exists = true;
						break;
					}
				}
			}
		}

		bool locked_target_died = false;
		if (last_locked_address != 0)
		{
			if (!locked_target_still_exists)
			{
				locked_target_died = true;
			}
			else
			{
				// Check health
				if (updated_locked_target.humanoid.address != 0)
				{
					try {
						float health = const_cast<cache::entity_t&>(updated_locked_target).humanoid.get_health();
						if (health <= 0.0f || !std::isfinite(health))
						{
							locked_target_died = true;
						}
					} catch (...) {}
				}
				// Check if knocked (if knocked_check is enabled)
				if (!locked_target_died && settings::new_silent::knocked_check && is_player_knocked(updated_locked_target))
				{
					locked_target_died = true;
				}
			}
		}

		if (locked_target_died)
		{
			last_locked_address = 0;
			{
				std::lock_guard<std::mutex> lock(g_silent_aim_mutex);
				g_silent_aim_locked = false;
				g_silent_cached_target = {};
			}
			silent_needs_key_release = true;
			continue;
		}

		cache::entity_t current_target = {};
		bool target_acquired = false;
		bool has_sticky_lock = false;

		// 2. Try to maintain sticky aim lock first
		if (settings::new_silent::sticky_aim && last_locked_address != 0)
		{
			// Check if target is valid ignoring FOV and Wall Check to maintain sticky lock
			if (is_target_valid(updated_locked_target, cursor_point, dims, view, true, true))
			{
				has_sticky_lock = true;
				last_locked_address = updated_locked_target.instance.address;

				// Only aim if visible (or if wall check is disabled)
				bool visible = true;
				if (settings::new_silent::wall_check && !is_player_visible(updated_locked_target))
				{
					visible = false;
				}
				if (visible)
				{
					current_target = updated_locked_target;
					target_acquired = true;
				}
			}
		}

		// 3. Acquire new target if we don't have a maintained sticky lock
		if (!target_acquired && !has_sticky_lock)
		{
			float closest_crosshair_dist = std::numeric_limits<float>::max();
			float closest_world_dist = std::numeric_limits<float>::max();
			float lowest_health_val = std::numeric_limits<float>::max();

			std::lock_guard<std::mutex> cache_lock(cache::mtx);
			if (cache::cached_players)
			{
				for (const auto& player : *cache::cached_players)
				{
					if (cache::is_local_player(player)) continue;

					if (is_target_valid(player, cursor_point, dims, view, false, false))
					{
						rbx::part_t target_part = get_target_part(player, settings::new_silent::aim_part, cursor_point, dims, view);
						if (target_part.address != 0)
						{
							rbx::primitive_t primitive = target_part.get_primitive();
							if (primitive.address != 0)
							{
								math::vector3 world_pos = primitive.get_position();
								math::vector2 screen_pos = {};
								if (game::visengine.world_to_client(world_pos, screen_pos, dims, view))
								{
									
									if (settings::new_silent::target_mode == 0)
									{
										float dist = get_magnitude(screen_pos, { static_cast<float>(cursor_point.x), static_cast<float>(cursor_point.y) });
										if (dist < closest_crosshair_dist)
										{
											closest_crosshair_dist = dist;
											current_target = player;
											target_acquired = true;
										}
									}
									
									else if (settings::new_silent::target_mode == 1)
									{
										if (game::workspace.address != 0)
										{
											rbx::instance_t local_camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
											if (local_camera_inst.address != 0)
											{
												rbx::camera_t local_camera{ local_camera_inst.address };
												math::vector3 cam_pos = local_camera.get_position();
												float dx = world_pos.x - cam_pos.x;
												float dy = world_pos.y - cam_pos.y;
												float dz = world_pos.z - cam_pos.z;
												float dist_3d = std::sqrt(dx * dx + dy * dy + dz * dz);
												if (dist_3d < closest_world_dist)
												{
													closest_world_dist = dist_3d;
													current_target = player;
													target_acquired = true;
												}
											}
										}
									}
									
									else if (settings::new_silent::target_mode == 2)
									{
										if (player.humanoid.address != 0)
										{
											try {
												float health = const_cast<cache::entity_t&>(player).humanoid.get_health();
												if (health < lowest_health_val)
												{
													lowest_health_val = health;
													current_target = player;
													target_acquired = true;
												}
											} catch (...) {}
										}
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
			
			bool hit_roll_success = true;
			if (settings::new_silent::hit_chance < 100)
			{
				int roll = dis(gen);
				if (roll > settings::new_silent::hit_chance)
				{
					hit_roll_success = false;
				}
			}

			if (hit_roll_success)
			{
				rbx::part_t target_part = get_target_part(current_target, settings::new_silent::aim_part, cursor_point, dims, view);
				if (target_part.address != 0)
				{
					rbx::primitive_t prim = target_part.get_primitive();
					if (prim.address != 0)
					{
						
						math::vector3 target_pos_3d = settings::new_silent::prediction_enabled ? apply_prediction(prim) : prim.get_position();
						math::vector2 screen_pos = {};
						if (game::visengine.world_to_client(target_pos_3d, screen_pos, dims, view))
						{
							write_mouse_position(mouse_svc_addr, screen_pos.x, screen_pos.y);

							{
								std::lock_guard<std::mutex> lock(g_silent_aim_mutex);
								g_silent_cached_target = current_target;
								g_silent_aim_locked = true;
								g_silent_partpos = screen_pos;
							}
							last_locked_address = current_target.instance.address;
							continue;
						}
					}
				}
			}
		}

		{
			std::lock_guard<std::mutex> lock(g_silent_aim_mutex);
			g_silent_aim_locked = false;
			g_silent_cached_target = {};
		}
		last_locked_address = 0;
	}
}

void rbx::new_silent::initialize()
{
	std::thread(run).detach();
}
