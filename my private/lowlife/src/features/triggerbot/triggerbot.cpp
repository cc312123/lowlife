#define NOMINMAX
#include <Windows.h>
#include <thread>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <chrono>
#include <random>
#include <unordered_map>
#include <unordered_set>

#include <memory/memory.h>
#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <cache/cache.h>
#include <game/game.h>
#include <settings.h>
#include <check/typing_check.h>
#include <render/notifications.h>
#include "triggerbot.h"

namespace {
	bool is_player_knocked(const cache::entity_t& player) {
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
}

namespace botter
{
	namespace
	{
		struct cached_part_t {
			math::vector3 position;
			math::matrix3 rotation;
			math::vector3 size;
			float r;
			float r_sq;
			int type; // 0 = standard Box/Part, 1 = WedgePart, 2 = CornerWedgePart
		};

		float dot(const math::vector3& a, const math::vector3& b)
		{
			return a.x * b.x + a.y * b.y + a.z * b.z;
		}

		bool get_inequality_interval(float a, float b, float t_start, float t_end, float& out_start, float& out_end)
		{
			if (std::abs(a) < 1e-5f)
			{
				if (b <= 0.0f)
				{
					out_start = t_start;
					out_end = t_end;
					return true;
				}
				return false;
			}

			if (a > 0.0f)
			{
				float limit = -b / a;
				if (t_start > limit) return false;
				out_start = t_start;
				out_end = std::min(t_end, limit);
				return true;
			}
			else
			{
				float limit = -b / a;
				if (limit > t_end) return false;
				out_start = std::max(t_start, limit);
				out_end = t_end;
				return true;
			}
		}

		bool ray_intersects_obb_get_t(
			const math::vector3& ray_origin,
			const math::vector3& ray_dir,
			float ray_length,
			const cached_part_t& box,
			float& out_t_min,
			float& out_t_max
		) {
			float t_min = 0.0f;
			float t_max = ray_length;

			math::vector3 delta = box.position - ray_origin;

			// Axis 0 (X)
			{
				float ax_x = box.rotation.m[0];
				float ax_y = box.rotation.m[3];
				float ax_z = box.rotation.m[6];
				float f = ray_dir.x * ax_x + ray_dir.y * ax_y + ray_dir.z * ax_z;
				float e = delta.x * ax_x + delta.y * ax_y + delta.z * ax_z;
				float ext = box.size.x * 0.5f;

				if (std::abs(f) > 0.0001f)
				{
					float inv_f = 1.0f / f;
					float t1 = (e - ext) * inv_f;
					float t2 = (e + ext) * inv_f;

					if (t1 > t2)
					{
						std::swap(t1, t2);
					}

					t_min = std::max(t_min, t1);
					t_max = std::min(t_max, t2);

					if (t_min > t_max)
					{
						return false;
					}
				}
				else
				{
					if (-e - ext > 0.0f || -e + ext < 0.0f)
					{
						return false;
					}
				}
			}

			// Axis 1 (Y)
			{
				float ay_x = box.rotation.m[1];
				float ay_y = box.rotation.m[4];
				float ay_z = box.rotation.m[7];
				float f = ray_dir.x * ay_x + ray_dir.y * ay_y + ray_dir.z * ay_z;
				float e = delta.x * ay_x + delta.y * ay_y + delta.z * ay_z;
				float ext = box.size.y * 0.5f;

				if (std::abs(f) > 0.0001f)
				{
					float inv_f = 1.0f / f;
					float t1 = (e - ext) * inv_f;
					float t2 = (e + ext) * inv_f;

					if (t1 > t2)
					{
						std::swap(t1, t2);
					}

					t_min = std::max(t_min, t1);
					t_max = std::min(t_max, t2);

					if (t_min > t_max)
					{
						return false;
					}
				}
				else
				{
					if (-e - ext > 0.0f || -e + ext < 0.0f)
					{
						return false;
					}
				}
			}

			// Axis 2 (Z)
			{
				float az_x = box.rotation.m[2];
				float az_y = box.rotation.m[5];
				float az_z = box.rotation.m[8];
				float f = ray_dir.x * az_x + ray_dir.y * az_y + ray_dir.z * az_z;
				float e = delta.x * az_x + delta.y * az_y + delta.z * az_z;
				float ext = box.size.z * 0.5f;

				if (std::abs(f) > 0.0001f)
				{
					float inv_f = 1.0f / f;
					float t1 = (e - ext) * inv_f;
					float t2 = (e + ext) * inv_f;

					if (t1 > t2)
					{
						std::swap(t1, t2);
					}

					t_min = std::max(t_min, t1);
					t_max = std::min(t_max, t2);

					if (t_min > t_max)
					{
						return false;
					}
				}
				else
				{
					if (-e - ext > 0.0f || -e + ext < 0.0f)
					{
						return false;
					}
				}
			}

			out_t_min = t_min;
			out_t_max = t_max;
			return true;
		}

		bool ray_intersects_part(
			const math::vector3& ray_origin,
			const math::vector3& ray_dir,
			float ray_length,
			const cached_part_t& box,
			float& intersection_distance
		) {
			float t_entry = 0.0f;
			float t_exit = ray_length;

			if (!ray_intersects_obb_get_t(ray_origin, ray_dir, ray_length, box, t_entry, t_exit))
			{
				return false;
			}

			if (box.type == 0) // Standard box
			{
				intersection_distance = t_entry;
				return true;
			}

			// Clamp segment to active ray range
			t_entry = std::max(0.0f, t_entry);
			t_exit = std::min(ray_length, t_exit);
			if (t_entry > t_exit)
			{
				return false;
			}

			math::vector3 delta_0 = ray_origin - box.position;
			math::vector3 axis_x = { box.rotation.m[0], box.rotation.m[3], box.rotation.m[6] };
			math::vector3 axis_y = { box.rotation.m[1], box.rotation.m[4], box.rotation.m[7] };
			math::vector3 axis_z = { box.rotation.m[2], box.rotation.m[5], box.rotation.m[8] };

			float x_0 = dot(delta_0, axis_x);
			float x_d = dot(ray_dir, axis_x);
			float y_0 = dot(delta_0, axis_y);
			float y_d = dot(ray_dir, axis_y);
			float z_0 = dot(delta_0, axis_z);
			float z_d = dot(ray_dir, axis_z);

			if (box.type == 1) // WedgePart
			{
				// Inequality: y <= z * (size.y / size.z)
				// y - z * (size.y / size.z) <= 0
				float scale = box.size.y / box.size.z;
				float a = y_d - z_d * scale;
				float b = y_0 - z_0 * scale;

				float out_start = 0.0f;
				float out_end = 0.0f;
				if (get_inequality_interval(a, b, t_entry, t_exit, out_start, out_end))
				{
					intersection_distance = out_start;
					return true;
				}
				return false;
			}
			else if (box.type == 2) // CornerWedgePart
			{
				// Inequality 1: y <= x * (size.y / size.x)
				// y - x * (size.y / size.x) <= 0
				float scale_x = box.size.y / box.size.x;
				float a1 = y_d - x_d * scale_x;
				float b1 = y_0 - x_0 * scale_x;

				// Inequality 2: y <= -z * (size.y / size.z)
				// y + z * (size.y / size.z) <= 0
				float scale_z = box.size.y / box.size.z;
				float a2 = y_d + z_d * scale_z;
				float b2 = y_0 + z_0 * scale_z;

				float start1 = 0.0f, end1 = 0.0f;
				float start2 = 0.0f, end2 = 0.0f;

				if (get_inequality_interval(a1, b1, t_entry, t_exit, start1, end1) &&
					get_inequality_interval(a2, b2, t_entry, t_exit, start2, end2))
				{
					float intersect_start = std::max(start1, start2);
					float intersect_end = std::min(end1, end2);
					if (intersect_start <= intersect_end)
					{
						intersection_distance = intersect_start;
						return true;
					}
				}
				return false;
			}

			return false;
		}

		bool is_point_inside_obb(const math::vector3& p, const cached_part_t& box);

		bool is_point_inside_part(const math::vector3& p, const cached_part_t& box)
		{
			if (!is_point_inside_obb(p, box))
			{
				return false;
			}

			if (box.type == 1) // WedgePart
			{
				math::vector3 delta = p - box.position;
				float local_y = delta.x * box.rotation.m[1] + delta.y * box.rotation.m[4] + delta.z * box.rotation.m[7];
				float local_z = delta.x * box.rotation.m[2] + delta.y * box.rotation.m[5] + delta.z * box.rotation.m[8];

				// Wedge slope rises from front (-Z) to back (+Z)
				float slope_y = local_z * (box.size.y / box.size.z);
				if (local_y > slope_y + 0.05f)
				{
					return false;
				}
			}
			else if (box.type == 2) // CornerWedgePart
			{
				math::vector3 delta = p - box.position;
				float local_x = delta.x * box.rotation.m[0] + delta.y * box.rotation.m[3] + delta.z * box.rotation.m[6];
				float local_y = delta.x * box.rotation.m[1] + delta.y * box.rotation.m[4] + delta.z * box.rotation.m[7];
				float local_z = delta.x * box.rotation.m[2] + delta.y * box.rotation.m[5] + delta.z * box.rotation.m[8];

				// CornerWedge peak is at (+X, +Y, -Z)
				float slope_y_x = local_x * (box.size.y / box.size.x);
				float slope_y_z = -local_z * (box.size.y / box.size.z);
				if (local_y > slope_y_x + 0.05f || local_y > slope_y_z + 0.05f)
				{
					return false;
				}
			}

			return true;
		}

		std::vector<cached_part_t> cached_map_parts;
		std::mutex map_parts_mutex;

		std::string get_class_name_fast(std::uint64_t instance_addr)
		{
			if (!instance_addr) return "unknown";
			std::uint64_t class_descriptor = memory->read<std::uint64_t>(instance_addr + Offsets::Instance::ClassDescriptor);
			if (!class_descriptor) return "unknown";

			static std::unordered_map<std::uint64_t, std::string> descriptor_cache;
			static std::uint32_t last_pid = 0;
			std::uint32_t current_pid = memory->get_process_id();
			if (current_pid != last_pid)
			{
				descriptor_cache.clear();
				last_pid = current_pid;
			}

			auto it = descriptor_cache.find(class_descriptor);
			if (it != descriptor_cache.end())
			{
				return it->second;
			}

			std::uint64_t class_name_ptr = memory->read<std::uint64_t>(class_descriptor + Offsets::Instance::ClassName);
			std::string name = "unknown";
			if (class_name_ptr)
			{
				name = memory->read_string(class_name_ptr);
			}
			descriptor_cache[class_descriptor] = name;
			return name;
		}

		bool str_contains_case_insensitive(const std::string& str, const std::string& find) {
			auto it = std::search(
				str.begin(), str.end(),
				find.begin(), find.end(),
				[](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
			);
			return it != str.end();
		}

		bool should_skip_instance(const std::string& name) {
			static const std::vector<std::string> skip_keywords = {
				"player", "debris", "effect", "visual", "bullet", "blood", "sound", 
				"tool", "dropped", "casing", "ragdoll", "particle", "marker", "gui", 
				"ui", "viewmodel", "camera", "raycast", "beam", "trail", "spell", "snow",
				"glass", "window", "fence", "grate", "foliage", "leaves", "leaf", "water", 
				"cloud", "spawn", "barrier", "trash", "prop", "vegetation", "bush", 
				"tree", "flower", "grass", "light", "lamp", "sign", "decal", "texture", 
				"mesh", "handle", "accessory", "hat", "hair", "helmet", "armor", "clothing"
			};
			
			std::string lower_name = name;
			std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
			
			for (const auto& skip : skip_keywords) {
				if (lower_name.find(skip) != std::string::npos) {
					return true;
				}
			}
			return false;
		}

		void gather_collidable_parts(rbx::instance_t parent, std::vector<cached_part_t>& parts, int depth = 0)
		{
			if (depth > 16 || !parent.address || parts.size() >= 30000) return;

			std::vector<rbx::instance_t> children;
			try {
				children = parent.get_children();
			} catch (...) {
				return;
			}

			for (auto& child : children)
			{
				if (!child.address || parts.size() >= 30000) continue;

				// 100% reliably skip local character model
				if (game::local_character.address != 0 && child.address == game::local_character.address) continue;

				std::string name = child.get_name();
				if (should_skip_instance(name)) continue;

				std::string class_name = get_class_name_fast(child.address);

				// Skip character models (players and NPCs)
				if (class_name == "Model")
				{
					bool is_player_char = false;
					{
						std::lock_guard<std::mutex> lock(cache::mtx);
						if (cache::cached_players) {
							for (const auto& player : *cache::cached_players) {
								if (player.name == name) {
									is_player_char = true;
									break;
								}
							}
						}
					}
					if (is_player_char) continue;

					// Robust player character model detection
					if (child.find_first_child("Humanoid").address != 0 ||
						child.find_first_child_by_class("Humanoid").address != 0 ||
						child.find_first_child("HumanoidRootPart").address != 0) {
						continue;
					}
				}

				// Cache physical collidable map parts
				if (class_name == "Part" || class_name == "MeshPart" || class_name == "WedgePart" ||
					class_name == "CornerWedgePart" || class_name == "TrussPart" || class_name == "SpawnLocation" ||
					class_name == "UnionOperation")
				{
					try {
						rbx::part_t part{ child.address };
						
						// Skip transparent/invisible parts (like glass windows, invisible walls, forcefields)
						float transparency = memory->read<float>(part.address + Offsets::BasePart::Transparency);
						if (transparency > 0.25f) continue;

						rbx::primitive_t prim = part.get_primitive();
						if (prim.address)
						{
							if (prim.get_can_collide())
							{
								cached_part_t cp;
								cp.position = prim.get_position();
								cp.rotation = prim.get_rotation();
								cp.size = prim.get_size();

								// Skip parts that are extremely small in all dimensions (props, pebbles, debris)
								if (cp.size.x < 0.5f && cp.size.y < 0.5f && cp.size.z < 0.5f) continue;

								if (cp.size.x > 0.01f && cp.size.y > 0.01f && cp.size.z > 0.01f)
								{
									float hx = cp.size.x * 0.5f;
									float hy = cp.size.y * 0.5f;
									float hz = cp.size.z * 0.5f;
									cp.r_sq = hx * hx + hy * hy + hz * hz;
									cp.r = std::sqrt(cp.r_sq);

									int part_type = 0;
									if (class_name == "WedgePart") {
										part_type = 1;
									} else if (class_name == "CornerWedgePart") {
										part_type = 2;
									}
									cp.type = part_type;

									parts.push_back(cp);
								}
							}
						}
					} catch (...) {}
				}

				// Only traverse children of container classes to optimize performance
				if (class_name == "Folder" || class_name == "Model" || class_name == "Workspace")
				{
					gather_collidable_parts(child, parts, depth + 1);
				}
			}
		}

		void map_cache_loop()
		{
			std::uint64_t last_workspace = 0;
			auto last_update_time = std::chrono::steady_clock::now();

			while (true)
			{
				Sleep(1000);

				bool any_wall_check = (settings::botter::autoclicker_enabled && settings::botter::wall_check) || 
				                      (settings::aimbot::enabled && settings::aimbot::wall_check) ||
				                      (settings::silent::enabled && settings::silent::wall_check);

				if (!any_wall_check)
				{
					std::lock_guard<std::mutex> lock(map_parts_mutex);
					cached_map_parts.clear();
					last_workspace = 0;
					continue;
				}

				if (!game::workspace.address) {
					std::lock_guard<std::mutex> lock(map_parts_mutex);
					cached_map_parts.clear();
					last_workspace = 0;
					continue;
				}

				auto now = std::chrono::steady_clock::now();
				bool timeout = std::chrono::duration_cast<std::chrono::seconds>(now - last_update_time).count() >= 10;

				// Periodically rescan map parts every 10 seconds to account for dynamic loading and stream updates
				if (game::workspace.address == last_workspace && !cached_map_parts.empty() && !timeout)
				{
					continue;
				}

				std::vector<cached_part_t> temp_parts;
				gather_collidable_parts(game::workspace, temp_parts);

				if (!temp_parts.empty())
				{
					std::lock_guard<std::mutex> lock(map_parts_mutex);
					cached_map_parts = std::move(temp_parts);
					last_workspace = game::workspace.address;
					last_update_time = std::chrono::steady_clock::now();
				}
			}
		}

		bool ray_intersects_obb(
			const math::vector3& ray_origin,
			const math::vector3& ray_dir,
			float ray_length,
			const cached_part_t& box,
			float& intersection_distance
		) {
			float t_min = 0.0f;
			float t_max = ray_length;

			math::vector3 delta = box.position - ray_origin;

			// Axis 0 (X)
			{
				float ax_x = box.rotation.m[0];
				float ax_y = box.rotation.m[3];
				float ax_z = box.rotation.m[6];
				float f = ray_dir.x * ax_x + ray_dir.y * ax_y + ray_dir.z * ax_z;
				float e = delta.x * ax_x + delta.y * ax_y + delta.z * ax_z;
				float ext = box.size.x * 0.5f;

				if (std::abs(f) > 0.0001f)
				{
					float inv_f = 1.0f / f;
					float t1 = (e - ext) * inv_f;
					float t2 = (e + ext) * inv_f;

					if (t1 > t2)
					{
						std::swap(t1, t2);
					}

					t_min = std::max(t_min, t1);
					t_max = std::min(t_max, t2);

					if (t_min > t_max)
					{
						return false;
					}
				}
				else
				{
					if (-e - ext > 0.0f || -e + ext < 0.0f)
					{
						return false;
					}
				}
			}

			// Axis 1 (Y)
			{
				float ay_x = box.rotation.m[1];
				float ay_y = box.rotation.m[4];
				float ay_z = box.rotation.m[7];
				float f = ray_dir.x * ay_x + ray_dir.y * ay_y + ray_dir.z * ay_z;
				float e = delta.x * ay_x + delta.y * ay_y + delta.z * ay_z;
				float ext = box.size.y * 0.5f;

				if (std::abs(f) > 0.0001f)
				{
					float inv_f = 1.0f / f;
					float t1 = (e - ext) * inv_f;
					float t2 = (e + ext) * inv_f;

					if (t1 > t2)
					{
						std::swap(t1, t2);
					}

					t_min = std::max(t_min, t1);
					t_max = std::min(t_max, t2);

					if (t_min > t_max)
					{
						return false;
					}
				}
				else
				{
					if (-e - ext > 0.0f || -e + ext < 0.0f)
					{
						return false;
					}
				}
			}

			// Axis 2 (Z)
			{
				float az_x = box.rotation.m[2];
				float az_y = box.rotation.m[5];
				float az_z = box.rotation.m[8];
				float f = ray_dir.x * az_x + ray_dir.y * az_y + ray_dir.z * az_z;
				float e = delta.x * az_x + delta.y * az_y + delta.z * az_z;
				float ext = box.size.z * 0.5f;

				if (std::abs(f) > 0.0001f)
				{
					float inv_f = 1.0f / f;
					float t1 = (e - ext) * inv_f;
					float t2 = (e + ext) * inv_f;

					if (t1 > t2)
					{
						std::swap(t1, t2);
					}

					t_min = std::max(t_min, t1);
					t_max = std::min(t_max, t2);

					if (t_min > t_max)
					{
						return false;
					}
				}
				else
				{
					if (-e - ext > 0.0f || -e + ext < 0.0f)
					{
						return false;
					}
				}
			}

			intersection_distance = t_min;
			return true;
		}

		bool is_point_inside_obb(const math::vector3& p, const cached_part_t& box)
		{
			math::vector3 delta = p - box.position;
			for (int i = 0; i < 3; ++i)
			{
				float ax_x = box.rotation.m[i];
				float ax_y = box.rotation.m[i + 3];
				float ax_z = box.rotation.m[i + 6];
				float dist = delta.x * ax_x + delta.y * ax_y + delta.z * ax_z;
				float ext = 0.0f;
				if (i == 0) ext = box.size.x * 0.5f;
				else if (i == 1) ext = box.size.y * 0.5f;
				else ext = box.size.z * 0.5f;

				if (std::abs(dist) > ext)
				{
					return false;
				}
			}
			return true;
		}

		static bool key_was_pressed = false;
		static bool toggle_state = false;

		bool get_keybind_state()
		{
			switch (settings::botter::trigger_keybind_mode)
			{
			case 0: 
				return (GetAsyncKeyState(settings::botter::trigger_keybind) & 0x8000) != 0;
			case 1: 
				{
					bool pressed = (GetAsyncKeyState(settings::botter::trigger_keybind) & 0x8000) != 0;
					if (pressed && !key_was_pressed) toggle_state = !toggle_state;
					key_was_pressed = pressed;
					return toggle_state;
				}
			case 2: 
				return true;
			default:
				return false;
			}
		}

		void trigger_immediate_click()
		{
			INPUT input = {};
			input.type = INPUT_MOUSE;
			input.mi.dx = 0;
			input.mi.dy = 0;
			input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
			SendInput(1, &input, sizeof(INPUT));

			Sleep(15);

			input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
			SendInput(1, &input, sizeof(INPUT));
		}
	}

	bool is_occluded(const math::vector3& start, const math::vector3& end)
	{
		std::lock_guard<std::mutex> lock(map_parts_mutex);

		math::vector3 dir = end - start;
		float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
		if (len < 0.001f) return false;

		math::vector3 dir_norm = { dir.x / len, dir.y / len, dir.z / len };

		float ray_min_x = std::min(start.x, end.x);
		float ray_max_x = std::max(start.x, end.x);
		float ray_min_y = std::min(start.y, end.y);
		float ray_max_y = std::max(start.y, end.y);
		float ray_min_z = std::min(start.z, end.z);
		float ray_max_z = std::max(start.z, end.z);

		float vx = dir.x;
		float vy = dir.y;
		float vz = dir.z;
		float v_sq = vx * vx + vy * vy + vz * vz;

		for (const auto& box : cached_map_parts)
		{
			if (is_point_inside_part(start, box) || is_point_inside_part(end, box))
			{
				return true;
			}
			float r = box.r;
			if (box.position.x + r < ray_min_x || box.position.x - r > ray_max_x ||
				box.position.y + r < ray_min_y || box.position.y - r > ray_max_y ||
				box.position.z + r < ray_min_z || box.position.z - r > ray_max_z)
			{
				continue;
			}

			float wx = box.position.x - start.x;
			float wy = box.position.y - start.y;
			float wz = box.position.z - start.z;
			
			float dot_val = wx * vx + wy * vy + wz * vz;
			float t = dot_val / v_sq;
			if (t < 0.0f) t = 0.0f;
			else if (t > 1.0f) t = 1.0f;
			
			float cx = start.x + t * vx;
			float cy = start.y + t * vy;
			float cz = start.z + t * vz;
			
			float dx = box.position.x - cx;
			float dy = box.position.y - cy;
			float dz = box.position.z - cz;
			
			float dist_sq = dx * dx + dy * dy + dz * dz;
			if (dist_sq > box.r_sq)
			{
				continue;
			}

			float dist = 0.0f;
			if (ray_intersects_part(start, dir_norm, len, box, dist))
			{
				return true; 
			}
		}

		return false;
	}

	void run()
	{
		std::thread(map_cache_loop).detach();

		POINT cursor_pt = {};
		static std::chrono::steady_clock::time_point last_bot_click = std::chrono::steady_clock::now();

		while (true)
		{
			Sleep(2); 

			if (!settings::botter::autoclicker_enabled || check::textchatopen || !game::workspace.address)
			{
				continue;
			}

			if (!get_keybind_state())
			{
				continue;
			}

			if (!GetCursorPos(&cursor_pt)) continue;
			HWND roblox_wnd = FindWindowA(nullptr, "Roblox");
			if (!roblox_wnd) continue;

			math::vector3 camera_pos = {};
			rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
			if (camera_inst.address != 0)
			{
				rbx::camera_t camera{ camera_inst.address };
				camera_pos = camera.get_position();
			}

			std::shared_ptr<std::vector<cache::entity_t>> players_snapshot;
			{
				std::lock_guard<std::mutex> lock(cache::mtx);
				players_snapshot = cache::cached_players;
			}

			math::vector2 dims = game::visengine.get_dimensions();
			math::matrix4 view = game::visengine.get_viewmatrix();

			bool clicked_this_tick = false;

			if (!players_snapshot) continue;

			for (auto& player : *players_snapshot)
			{
				if (player.instance.address == 0 ||
					player.instance.address == cache::cached_local_player.instance.address ||
					player.instance.address == game::local_player.address ||
					(player.name == cache::cached_local_player.name && !player.name.empty()))
				{
					continue;
				}

				if (game::local_character.address != 0 && player.model_address != 0 &&
					player.model_address == game::local_character.address)
				{
					continue;
				}

				if (settings::botter::team_check)
				{
					if (!cache::cached_local_player.crew_id.empty() && !player.crew_id.empty() &&
						cache::cached_local_player.crew_id != "0" && player.crew_id != "0" &&
						cache::cached_local_player.crew_id == player.crew_id)
					{
						continue;
					}
				}

				if (settings::botter::knocked_check && is_player_knocked(player))
				{
					continue;
				}

				// -- TARGET HRP PROJECTION BOX CHECK --
				clicked_this_tick = false;
				auto hrp_it = player.parts.find("HumanoidRootPart");
				if (hrp_it != player.parts.end())
				{
					rbx::part_t part = hrp_it->second;
					if (part.address)
					{
						rbx::primitive_t prim = part.get_primitive();
						if (prim.address)
						{
							math::vector3 pos = prim.get_position();
							math::vector3 size = { 4.0f, 6.0f, 2.0f };
							math::matrix3 rot = prim.get_rotation();

							bool valid = false;
							float left = FLT_MAX, top = FLT_MAX;
							float right = -FLT_MAX, bottom = -FLT_MAX;

							static math::vector3 local_corners[8] =
							{
								{-1, -1, -1}, {1, -1, -1}, {-1, 1, -1}, {1, 1, -1},
								{-1, -1, 1}, {1, -1, 1}, {-1, 1, 1}, {1, 1, 1}
							};

							for (auto& corner : local_corners)
							{
								math::vector3 world = pos + rot * math::vector3
								{
									corner.x * size.x * 0.5f,
									corner.y * size.y * 0.5f,
									corner.z * size.z * 0.5f
								};

								math::vector2 out{};
								if (game::visengine.world_to_screen(world, out, dims, view))
								{
									valid = true;
									left = std::min(left, out.x);
									top = std::min(top, out.y);
									right = std::max(right, out.x);
									bottom = std::max(bottom, out.y);
								}
							}

							if (valid && left < right && top < bottom)
							{
								float scale = settings::botter::hitbox_size / 100.0f;
								float width = right - left;
								float height = bottom - top;
								float delta_w = (width * scale - width) * 0.5f;
								float delta_h = (height * scale - height) * 0.5f;
								float target_left = left - delta_w;
								float target_right = right + delta_w;
								float target_top = top - delta_h;
								float target_bottom = bottom + delta_h;

								if (cursor_pt.x >= target_left && cursor_pt.x <= target_right &&
									cursor_pt.y >= target_top && cursor_pt.y <= target_bottom)
								{
									if (settings::botter::wall_check && camera_inst.address != 0)
									{
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
											if (!is_occluded(camera_pos, world_pos))
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

									auto now = std::chrono::steady_clock::now();
									auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_bot_click).count();
									int cps = settings::botter::cps;
									if (cps < 1) cps = 1;
									if (duration >= (1000 / cps))
									{
										trigger_immediate_click();
										last_bot_click = now;
										clicked_this_tick = true;
									}
								}
							}
						}
					}
				}

				if (clicked_this_tick)
				{
					break;
				}
			}
		}
	}
}

namespace shot_detect
{
	cache::entity_t target_player = {};
	bool has_target = false;
	int last_ammo_val = -1;

	void trigger_immediate_click()
	{
		INPUT input = {};
		input.type = INPUT_MOUSE;
		input.mi.dx = 0;
		input.mi.dy = 0;
		input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
		SendInput(1, &input, sizeof(INPUT));

		Sleep(15);

		input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
		SendInput(1, &input, sizeof(INPUT));
	}

	bool get_keybind_state()
	{
		switch (settings::shot_detect::trigger_keybind_mode)
		{
		case 0: 
			return (GetAsyncKeyState(settings::shot_detect::trigger_keybind) & 0x8000) != 0;
		case 1: 
			{
				static bool key_was_pressed = false;
				static bool toggle_state = false;
				bool pressed = (GetAsyncKeyState(settings::shot_detect::trigger_keybind) & 0x8000) != 0;
				if (pressed && !key_was_pressed) toggle_state = !toggle_state;
				key_was_pressed = pressed;
				return toggle_state;
			}
		case 2: 
			return true;
		default:
			return false;
		}
	}

	int read_value_instance(rbx::instance_t child)
	{
		std::string cclass = child.get_class_name();
		if (cclass == "IntValue")
		{
			return (int)memory->read<std::int64_t>(child.address + Offsets::Misc::Value);
		}
		else if (cclass == "NumberValue" || cclass == "DoubleValue")
		{
			double val = memory->read<double>(child.address + Offsets::Misc::Value);
			return (int)val;
		}
		else if (cclass == "StringValue")
		{
			std::string s_val = memory->read_string(child.address + Offsets::Misc::Value);
			if (!s_val.empty())
			{
				try {
					return std::stoi(s_val);
				} catch (...) {}
			}
		}
		return memory->read<int>(child.address + Offsets::Misc::Value);
	}

	std::string get_local_tool_name()
	{
		if (cache::cached_local_player.instance.address == 0 || game::local_character.address == 0)
			return "";
		try {
			rbx::instance_t model_inst{ game::local_character.address };
			for (auto& child : model_inst.get_children())
			{
				std::string cclass = child.get_class_name();
				if (cclass == "Tool" || cclass == "HopperBin")
				{
					return child.get_name();
				}
			}
		} catch (...) {}
		return "";
	}

	int get_local_ammo()
	{
		if (cache::cached_local_player.instance.address == 0 || game::local_character.address == 0)
			return -1;
		try {
			rbx::instance_t model_inst{ game::local_character.address };
			rbx::instance_t equipped_tool = {};
			for (auto& child : model_inst.get_children())
			{
				std::string cclass = child.get_class_name();
				if (cclass == "Tool" || cclass == "HopperBin")
				{
					equipped_tool = child;
					break;
				}
			}

			if (equipped_tool.address != 0)
			{
				for (auto& child : equipped_tool.get_children())
				{
					std::string cclass = child.get_class_name();
					if (cclass.find("Value") != std::string::npos)
					{
						std::string cname = child.get_name();
						std::string lower_cname = cname;
						std::transform(lower_cname.begin(), lower_cname.end(), lower_cname.begin(), ::tolower);
						if (lower_cname.find("ammo") != std::string::npos || lower_cname.find("clip") != std::string::npos)
						{
							int val = read_value_instance(child);
							return val;
						}
					}
				}
			}
		} catch (...) {}
		return -1;
	}

	void press_slot_key(int slot)
	{
		if (slot < 1 || slot > 9) return;
		WORD vk = '0' + slot; // '1' = 0x31, etc.

		INPUT inputs[2] = {};
		inputs[0].type = INPUT_KEYBOARD;
		inputs[0].ki.wVk = vk;
		inputs[0].ki.wScan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
		inputs[0].ki.dwFlags = 0;
		
		inputs[1].type = INPUT_KEYBOARD;
		inputs[1].ki.wVk = vk;
		inputs[1].ki.wScan = inputs[0].ki.wScan;
		inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

		SendInput(2, inputs, sizeof(INPUT));
	}

	void gun_swap_loop()
	{
		int last_local_ammo = -1;
		std::string last_tool = "";
		bool db_force_equipped = false;
		bool has_swapped_to_revolver = false;

		while (true)
		{
			Sleep(10);

			if (!settings::shot_detect::gunswap_enabled || !game::local_character.address)
			{
				last_local_ammo = -1;
				last_tool = "";
				db_force_equipped = false;
				has_swapped_to_revolver = false;
				continue;
			}

			bool sd_active = settings::shot_detect::enabled && has_target && get_keybind_state();
			bool should_start_with_db = settings::shot_detect::always_start_with_db && sd_active;

			std::string current_tool = get_local_tool_name();
			std::string lower_tool = current_tool;
			std::transform(lower_tool.begin(), lower_tool.end(), lower_tool.begin(), ::tolower);

			// We check if current tool is DB/Double-Barrel
			bool is_db = (lower_tool.find("double") != std::string::npos || 
			              lower_tool.find("db") != std::string::npos || 
			              lower_tool.find("barrel") != std::string::npos);

			if (should_start_with_db)
			{
				if (!is_db && !db_force_equipped)
				{
					press_slot_key(settings::shot_detect::db_slot);
					db_force_equipped = true;
					has_swapped_to_revolver = false;
					Sleep(150);
					continue;
				}
			}
			else
			{
				db_force_equipped = false;
			}

			if (is_db)
			{
				int ammo = get_local_ammo();
				if (ammo != -1)
				{
					// If the weapon is reloaded or newly equipped, update the last ammo cache
					if (last_tool != current_tool || last_local_ammo == -1 || ammo > last_local_ammo)
					{
						last_local_ammo = ammo;
						has_swapped_to_revolver = false;
					}
					else if (ammo < last_local_ammo && !has_swapped_to_revolver)
					{
						has_swapped_to_revolver = true;

						// Fired! Swap to revolver slot after configured delay
						Sleep(settings::shot_detect::gunswap_delay);
						
						// Verify we are not already holding revolver before pressing slot key
						std::string check_tool = get_local_tool_name();
						std::transform(check_tool.begin(), check_tool.end(), check_tool.begin(), ::tolower);
						bool is_revolver = (check_tool.find("revolver") != std::string::npos || 
						                    check_tool.find("rev") != std::string::npos);
						if (!is_revolver)
						{
							press_slot_key(settings::shot_detect::revolver_slot);
						}
						last_local_ammo = ammo;
					}
				}
				last_tool = current_tool;
			}
			else
			{
				last_local_ammo = -1;
				last_tool = current_tool;
			}
		}
	}

	cache::entity_t get_player_under_cursor()
	{
		POINT cursor_pt;
		if (!GetCursorPos(&cursor_pt))
			return {};

		HWND roblox_wnd = game::wnd;
		if (!roblox_wnd) {
			roblox_wnd = FindWindowA(nullptr, "Roblox");
			if (roblox_wnd) game::wnd = roblox_wnd;
		}
		if (!roblox_wnd || !ScreenToClient(roblox_wnd, &cursor_pt))
			return {};

		math::vector2 dims = game::visengine.get_dimensions();
		math::matrix4 view = game::visengine.get_viewmatrix();

		std::shared_ptr<std::vector<cache::entity_t>> players_snapshot;
		{
			std::lock_guard<std::mutex> lock(cache::mtx);
			players_snapshot = cache::cached_players;
		}

		if (!players_snapshot)
			return {};

		static math::vector3 corners[8] =
		{
			{-1, -1, -1}, {1, -1, -1}, {-1, 1, -1}, {1, 1, -1},
			{-1, -1, 1}, {1, -1, 1}, {-1, 1, 1}, {1, 1, 1}
		};

		cache::entity_t best_target = {};
		float best_target_dist_from_cursor = FLT_MAX;

		for (const auto& player : *players_snapshot)
		{
			if (player.instance.address == 0 ||
				player.instance.address == game::local_player.address)
			{
				continue;
			}

			auto hrp_it = player.parts.find("HumanoidRootPart");
			if (hrp_it == player.parts.end() || hrp_it->second.address == 0)
				continue;

			rbx::part_t hrp_part = hrp_it->second;
			math::vector3 hrp_pos = hrp_part.get_primitive().get_position();
			math::matrix3 hrp_rot = hrp_part.get_primitive().get_rotation();

			bool valid = false;
			float left = FLT_MAX, top = FLT_MAX;
			float right = -FLT_MAX, bottom = -FLT_MAX;

			math::vector3 char_size = { 4.0f, 6.0f, 2.0f };

			for (auto& corner : corners)
			{
				math::vector3 world = hrp_pos + hrp_rot * math::vector3
				{
					corner.x * char_size.x * 0.5f,
					corner.y * char_size.y * 0.5f,
					corner.z * char_size.z * 0.5f
				};

				math::vector2 out{};
				if (game::visengine.world_to_screen(world, out, dims, view))
				{
					valid = true;
					left = std::min(left, out.x);
					top = std::min(top, out.y);
					right = std::max(right, out.x);
					bottom = std::max(bottom, out.y);
				}
			}

			if (valid && left < right && top < bottom)
			{
				if (cursor_pt.x >= left && cursor_pt.x <= right &&
					cursor_pt.y >= top && cursor_pt.y <= bottom)
				{
					float center_x = (left + right) * 0.5f;
					float center_y = (top + bottom) * 0.5f;
					float dx = (float)cursor_pt.x - center_x;
					float dy = (float)cursor_pt.y - center_y;
					float dist = std::sqrt(dx*dx + dy*dy);
					if (dist < best_target_dist_from_cursor)
					{
						best_target_dist_from_cursor = dist;
						best_target = player;
					}
				}
			}
		}

		return best_target;
	}

	int get_target_ammo(const cache::entity_t& target)
	{
		if (target.instance.address == 0 || target.model_address == 0)
			return -1;

		try {
			rbx::instance_t model_inst{ target.model_address };
			rbx::instance_t equipped_tool = {};
			for (auto& child : model_inst.get_children())
			{
				std::string cclass = child.get_class_name();
				if (cclass == "Tool" || cclass == "HopperBin")
				{
					equipped_tool = child;
					break;
				}
			}

			if (equipped_tool.address != 0)
			{
				for (auto& child : equipped_tool.get_children())
				{
					std::string cclass = child.get_class_name();
					if (cclass.find("Value") != std::string::npos)
					{
						std::string cname = child.get_name();
						std::string lower_cname = cname;
						std::transform(lower_cname.begin(), lower_cname.end(), lower_cname.begin(), ::tolower);
						if (lower_cname.find("ammo") != std::string::npos || lower_cname.find("clip") != std::string::npos)
						{
							int val = read_value_instance(child);
							return val;
						}
					}
				}
				return -2; // Holding tool, but no ammo found
			}
		} catch (...) {}

		return -1; // No tool held
	}

	void run()
	{
		std::thread(gun_swap_loop).detach();

		bool mb5_was_pressed = false;
		bool is_clicking = false;
		bool is_first_click = true;
		auto last_click_time = std::chrono::steady_clock::now();
		int current_delay = 100;

		while (true)
		{
			Sleep(5);

			if (!game::workspace.address || !game::local_player.address)
			{
				is_clicking = false;
				continue;
			}

			// 1. Handle target selection via Mouse Button 5 (VK_XBUTTON2)
			bool mb5_is_pressed = (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0;
			if (mb5_is_pressed && !mb5_was_pressed)
			{
				cache::entity_t target = get_player_under_cursor();
				if (target.instance.address != 0)
				{
					target_player = target;
					has_target = true;
					last_ammo_val = -1;
					is_clicking = false;
					notifications::add("Shot Detect Target: " + target.display_name, notifications::NotificationType::Success, 3.0f);
				}
				else
				{
					// Fallback: check closest player within 120px
					POINT cursor_pt;
					if (GetCursorPos(&cursor_pt))
					{
						HWND roblox_wnd = game::wnd;
						if (!roblox_wnd) roblox_wnd = FindWindowA(nullptr, "Roblox");
						if (roblox_wnd && ScreenToClient(roblox_wnd, &cursor_pt))
						{
							math::vector2 dims = game::visengine.get_dimensions();
							math::matrix4 view = game::visengine.get_viewmatrix();

							std::shared_ptr<std::vector<cache::entity_t>> players_snapshot;
							{
								std::lock_guard<std::mutex> lock(cache::mtx);
								players_snapshot = cache::cached_players;
							}

							if (players_snapshot)
							{
								cache::entity_t closest_player = {};
								float min_dist = 120.0f;

								for (const auto& player : *players_snapshot)
								{
									if (player.instance.address == 0 || player.instance.address == game::local_player.address)
										continue;

									auto hrp_it = player.parts.find("HumanoidRootPart");
									if (hrp_it == player.parts.end() || hrp_it->second.address == 0)
										continue;

									rbx::part_t hrp_part = hrp_it->second;
									math::vector3 world_pos = hrp_part.get_primitive().get_position();
									math::vector2 screen_pos = {};
									if (game::visengine.world_to_screen(world_pos, screen_pos, dims, view))
									{
										float dx = (float)cursor_pt.x - screen_pos.x;
										float dy = (float)cursor_pt.y - screen_pos.y;
										float dist = std::sqrt(dx*dx + dy*dy);
										if (dist < min_dist)
										{
											min_dist = dist;
											closest_player = player;
										}
									}
								}

								if (closest_player.instance.address != 0)
								{
									target_player = closest_player;
									has_target = true;
									last_ammo_val = -1;
									is_clicking = false;
									notifications::add("Shot Detect Target: " + closest_player.display_name, notifications::NotificationType::Success, 3.0f);
								}
							}
						}
					}
				}
			}
			mb5_was_pressed = mb5_is_pressed;

			// 2. Handle Autoclicker Triggered by Key Bind and target ammo decrease
			if (settings::shot_detect::enabled && has_target)
			{
				bool key_active = get_keybind_state();
				if (key_active)
				{
					bool target_still_valid = false;
					cache::entity_t current_target_state = {};
					{
						std::lock_guard<std::mutex> lock(cache::mtx);
						if (cache::cached_players)
						{
							for (const auto& player : *cache::cached_players)
							{
								if (player.instance.address == target_player.instance.address)
								{
									current_target_state = player;
									target_still_valid = true;
									break;
								}
							}
						}
					}

					if (target_still_valid)
					{
						target_player = current_target_state;

						int current_ammo = get_target_ammo(current_target_state);
						if (current_ammo >= 0)
						{
							if (last_ammo_val >= 0)
							{
								if (current_ammo < last_ammo_val)
								{
									if (!is_clicking)
									{
										is_clicking = true;
										is_first_click = true;
										if (settings::shot_detect::randomize_delay)
										{
											int min_val = settings::shot_detect::min_delay;
											int max_val = settings::shot_detect::max_delay;
											if (min_val > max_val) std::swap(min_val, max_val);
											if (min_val < 1) min_val = 1;
											if (max_val < 1) max_val = 1;

											std::random_device rd;
											std::mt19937 gen(rd());
											std::uniform_int_distribution<> distrib(min_val, max_val);
											current_delay = distrib(gen);
										}
										else
										{
											current_delay = settings::shot_detect::click_delay;
										}
										last_click_time = std::chrono::steady_clock::now();
									}
								}
							}
							last_ammo_val = current_ammo;
						}
						else
						{
							// Keep clicking at the same speed (CPS) if it was already active, even when tools are swapped/unequipped
							last_ammo_val = -1;
						}
					}
					else
					{
						is_clicking = false;
						last_ammo_val = -1;
					}
				}
				else
				{
					is_clicking = false;
					last_ammo_val = -1;
				}

				if (is_clicking)
				{
					if (settings::shot_detect::click_mode == 0) // Continuous
					{
						auto now = std::chrono::steady_clock::now();
						auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_click_time).count();

						int target_delay = current_delay;
						if (!settings::shot_detect::randomize_delay)
						{
							if (is_first_click)
							{
								target_delay = settings::shot_detect::click_delay;
							}
							else
							{
								int cps = settings::shot_detect::cps;
								if (cps < 1) cps = 1;
								target_delay = 1000 / cps;
							}
						}

						if (duration >= target_delay)
						{
							trigger_immediate_click();
							last_click_time = now;

							if (is_first_click)
							{
								is_first_click = false;
							}

							if (settings::shot_detect::randomize_delay)
							{
								int min_val = settings::shot_detect::min_delay;
								int max_val = settings::shot_detect::max_delay;
								if (min_val > max_val) std::swap(min_val, max_val);
								if (min_val < 1) min_val = 1;
								if (max_val < 1) max_val = 1;

								std::random_device rd;
								std::mt19937 gen(rd());
								std::uniform_int_distribution<> distrib(min_val, max_val);
								current_delay = distrib(gen);
							}
							else
							{
								int cps = settings::shot_detect::cps;
								if (cps < 1) cps = 1;
								current_delay = 1000 / cps;
							}
						}
					}
					else // Single Click
					{
						auto now = std::chrono::steady_clock::now();
						auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_click_time).count();

						int target_delay = current_delay;
						if (!settings::shot_detect::randomize_delay)
						{
							target_delay = settings::shot_detect::click_delay;
						}

						if (duration >= target_delay)
						{
							trigger_immediate_click();
							is_clicking = false;
						}
					}
				}
			}
			else
			{
				is_clicking = false;
				last_ammo_val = -1;
			}
		}
	}
}
