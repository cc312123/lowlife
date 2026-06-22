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
#include "../expl/luau_hook.h"
#include "triggerbot.h"

namespace {
	volatile std::uint64_t cached_lua_state = 0;
	volatile std::uint64_t cached_global_state = 0;
	volatile std::uint64_t cached_rngstate_offset = 0;
	volatile std::uint64_t last_dm_address = 0;
	volatile bool is_resolving = false;

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

	std::unordered_set<std::uint64_t> cached_random_metatables;
	std::unordered_set<std::uint64_t> cached_non_random_metatables;

	bool is_random_metatable(std::uint64_t metatable_addr)
	{
		if (metatable_addr == 0) return false;
		if (cached_random_metatables.count(metatable_addr)) return true;
		if (cached_non_random_metatables.count(metatable_addr)) return false;

		// Read table header: tt is at offset 0
		std::uint8_t tt = memory->read<std::uint8_t>(metatable_addr + 0);
		if (tt != 9 && tt != 8 && tt != 7 && tt != 6 && tt != 5) return false; // LUA_TTABLE is 9/8/7/6/5 (shifted or standard)

		std::uint8_t lsizenode = memory->read<std::uint8_t>(metatable_addr + 6);
		std::uint64_t node_ptr = memory->read<std::uint64_t>(metatable_addr + 32);
		if (node_ptr == 0) return false;

		int size = 1 << lsizenode;
		for (int i = 0; i < size; ++i)
		{
			std::uint64_t node_addr = node_ptr + i * 32;

			// Read key.tt from LuaNode
			std::uint32_t val_28 = memory->read<std::uint32_t>(node_addr + 28);
			std::uint32_t key_tt = val_28 & 0xF;

			if (key_tt == 6 || key_tt == 5 || key_tt == 4) // LUA_TSTRING is 6/5/4 (shifted or standard)
			{
				std::uint64_t ts_ptr = memory->read<std::uint64_t>(node_addr + 16);
				if (ts_ptr != 0)
				{
					unsigned int len = memory->read<unsigned int>(ts_ptr + 20);
					if (len > 0 && len < 64)
					{
						std::vector<char> buf(len + 1, 0);
						Luck_ReadVirtualMemory(memory->get_process_handle(), reinterpret_cast<void*>(ts_ptr + 24), buf.data(), len, nullptr);
						std::string str(buf.data(), len);
						if (str == "NextNumber" || str == "NextInteger" || str == "NextUnitVector")
						{
							cached_random_metatables.insert(metatable_addr);
							return true;
						}
					}
				}
			}
		}

		cached_non_random_metatables.insert(metatable_addr);
		return false;
	}

	void scan_gc_heap_for_random_objects(std::uint64_t global_state)
	{
		if (global_state == 0) return;

		static std::uint64_t allgcopages_offset = 0;
		if (allgcopages_offset == 0)
		{
			// Try 744 first
			std::uint64_t p = memory->read<std::uint64_t>(global_state + 744);
			if (p != 0)
			{
				int pageSize = memory->read<int>(p + 32);
				int blockSize = memory->read<int>(p + 36);
				if (pageSize >= 4096 && pageSize <= 65536 && blockSize >= 8 && blockSize <= pageSize)
				{
					allgcopages_offset = 744;
				}
			}

			// Fallback scan
			if (allgcopages_offset == 0)
			{
				for (std::uint64_t offset = 500; offset < 1000; offset += 8)
				{
					std::uint64_t ptr = memory->read<std::uint64_t>(global_state + offset);
					if (ptr == 0 || (ptr % 8) != 0 || ptr < 0x100000 || ptr > 0x7FFFFFFFFFFF)
						continue;

					int pageSize = memory->read<int>(ptr + 32);
					int blockSize = memory->read<int>(ptr + 36);
					int busyBlocks = memory->read<int>(ptr + 52);

					if (pageSize >= 4096 && pageSize <= 65536 && blockSize >= 8 && blockSize <= pageSize && busyBlocks >= 0)
					{
						allgcopages_offset = offset;
						printf("[ LOWLIFE ]: Dynamically resolved allgcopages offset to 0x%llx\n", offset);
						break;
					}
				}
			}
		}

		if (allgcopages_offset == 0) return;

		std::uint64_t page = memory->read<std::uint64_t>(global_state + allgcopages_offset);
		int page_count = 0;

		while (page != 0 && page_count < 2000)
		{
			page_count++;

			int pageSize = memory->read<int>(page + 32);
			int blockSize = memory->read<int>(page + 36);
			int freeNext = memory->read<int>(page + 48);
			int busyBlocks = memory->read<int>(page + 52);

			if (pageSize > 0 && blockSize > 0 && busyBlocks > 0)
			{
				int blockCount = (pageSize - 64) / blockSize;
				std::uint64_t start_addr = page + 64 + freeNext + blockSize;
				std::uint64_t end_addr = page + 64 + blockCount * blockSize;

				for (std::uint64_t pos = start_addr; pos < end_addr; pos += blockSize)
				{
					std::uint8_t tt = memory->read<std::uint8_t>(pos + 0);
					if (tt == 9 || tt == 8 || tt == 7) // LUA_TUSERDATA is 9/8/7 (shifted or standard)
					{
						std::uint64_t metatable = memory->read<std::uint64_t>(pos + 8);
						if (is_random_metatable(metatable))
						{
							std::uint64_t state = memory->read<std::uint64_t>(pos + 16);
							std::uint64_t inc = memory->read<std::uint64_t>(pos + 24);
							if (state != 0 || inc != 0)
							{
								memory->write<std::uint64_t>(pos + 16, 0);
								memory->write<std::uint64_t>(pos + 24, 0);
								printf("[ LOWLIFE ]: Zeroed out Random userdata state and increment at 0x%llx\n", pos);
							}
						}
					}
				}
			}

			page = memory->read<std::uint64_t>(page + 24);
		}
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
				                      (settings::aimbot::enabled && settings::aimbot::wall_check);

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

		bool invert_matrix4(const math::matrix4& m, math::matrix4& invOut)
		{
			float inv[16], det;
			int i;

			inv[0] = m.m[1][1]  * m.m[2][2] * m.m[3][3] - 
					 m.m[1][1]  * m.m[2][3] * m.m[3][2] - 
					 m.m[2][1]  * m.m[1][2] * m.m[3][3] + 
					 m.m[2][1]  * m.m[1][3] * m.m[3][2] +
					 m.m[3][1] * m.m[1][2] * m.m[2][3] - 
					 m.m[3][1] * m.m[1][3] * m.m[2][2];

			inv[4] = -m.m[1][0]  * m.m[2][2] * m.m[3][3] + 
					  m.m[1][0]  * m.m[2][3] * m.m[3][2] + 
					  m.m[2][0]  * m.m[1][2] * m.m[3][3] - 
					  m.m[2][0]  * m.m[1][3] * m.m[3][2] - 
					  m.m[3][0] * m.m[1][2] * m.m[2][3] + 
					  m.m[3][0] * m.m[1][3] * m.m[2][2];

			inv[8] = m.m[1][0]  * m.m[2][1] * m.m[3][3] - 
					 m.m[1][0]  * m.m[2][3] * m.m[3][1] - 
					 m.m[2][0]  * m.m[1][1] * m.m[3][3] + 
					 m.m[2][0]  * m.m[1][3] * m.m[3][1] + 
					 m.m[3][0] * m.m[1][1] * m.m[2][3] - 
					 m.m[3][0] * m.m[1][3] * m.m[2][1];

			inv[12] = -m.m[1][0]  * m.m[2][1] * m.m[3][2] + 
					   m.m[1][0]  * m.m[2][2] * m.m[3][1] + 
					   m.m[2][0]  * m.m[1][1] * m.m[3][2] - 
					   m.m[2][0]  * m.m[1][2] * m.m[3][1] - 
					   m.m[3][0] * m.m[1][1] * m.m[2][2] + 
					   m.m[3][0] * m.m[1][2] * m.m[2][1];

			inv[1] = -m.m[0][1]  * m.m[2][2] * m.m[3][3] + 
					  m.m[0][1]  * m.m[2][3] * m.m[3][2] + 
					  m.m[2][1]  * m.m[0][2] * m.m[3][3] - 
					  m.m[2][1]  * m.m[0][3] * m.m[3][2] - 
					  m.m[3][1] * m.m[0][2] * m.m[2][3] + 
					  m.m[3][1] * m.m[0][3] * m.m[2][2];

			inv[5] = m.m[0][0]  * m.m[2][2] * m.m[3][3] - 
					 m.m[0][0]  * m.m[2][3] * m.m[3][2] - 
					 m.m[2][0]  * m.m[0][2] * m.m[3][3] + 
					 m.m[2][0]  * m.m[0][3] * m.m[3][2] + 
					 m.m[3][0] * m.m[0][2] * m.m[2][3] - 
					 m.m[3][0] * m.m[0][3] * m.m[2][2];

			inv[9] = -m.m[0][0]  * m.m[2][1] * m.m[3][3] + 
					  m.m[0][0]  * m.m[2][3] * m.m[3][1] + 
					  m.m[2][0]  * m.m[0][1] * m.m[3][3] - 
					  m.m[2][0]  * m.m[0][3] * m.m[3][1] - 
					  m.m[3][0] * m.m[0][1] * m.m[2][3] + 
					  m.m[3][0] * m.m[0][3] * m.m[2][1];

			inv[13] = m.m[0][0]  * m.m[2][1] * m.m[3][2] - 
					  m.m[0][0]  * m.m[2][2] * m.m[3][1] - 
					  m.m[2][0]  * m.m[0][1] * m.m[3][2] + 
					  m.m[2][0]  * m.m[0][2] * m.m[3][1] + 
					  m.m[3][0] * m.m[0][1] * m.m[2][2] - 
					  m.m[3][0] * m.m[0][2] * m.m[2][1];

			inv[2] = m.m[0][1]  * m.m[1][2] * m.m[3][3] - 
					 m.m[0][1]  * m.m[1][3] * m.m[3][2] - 
					 m.m[1][1]  * m.m[0][2] * m.m[3][3] + 
					 m.m[1][1]  * m.m[0][3] * m.m[3][2] + 
					 m.m[3][1] * m.m[0][2] * m.m[1][3] - 
					 m.m[3][1] * m.m[0][3] * m.m[1][2];

			inv[6] = -m.m[0][0]  * m.m[1][2] * m.m[3][3] + 
					  m.m[0][0]  * m.m[1][3] * m.m[3][2] + 
					  m.m[1][0]  * m.m[0][2] * m.m[3][3] - 
					  m.m[1][0]  * m.m[0][3] * m.m[3][2] - 
					  m.m[3][0] * m.m[0][2] * m.m[1][3] + 
					  m.m[3][0] * m.m[0][3] * m.m[1][2];

			inv[10] = m.m[0][0]  * m.m[1][1] * m.m[3][3] - 
					  m.m[0][0]  * m.m[1][3] * m.m[3][1] - 
					  m.m[1][0]  * m.m[0][1] * m.m[3][3] + 
					  m.m[1][0]  * m.m[0][3] * m.m[3][1] + 
					  m.m[3][0] * m.m[0][1] * m.m[1][3] - 
					  m.m[3][0] * m.m[0][3] * m.m[1][1];

			inv[14] = -m.m[0][0]  * m.m[1][1] * m.m[3][2] + 
					   m.m[0][0]  * m.m[1][2] * m.m[3][1] + 
					   m.m[1][0]  * m.m[0][1] * m.m[3][2] - 
					   m.m[1][0]  * m.m[0][2] * m.m[3][1] - 
					   m.m[3][0] * m.m[0][1] * m.m[1][2] + 
					   m.m[3][0] * m.m[0][2] * m.m[1][1];

			inv[3] = -m.m[0][1] * m.m[1][2] * m.m[2][3] + 
					  m.m[0][1] * m.m[1][3] * m.m[2][2] + 
					  m.m[1][1] * m.m[0][2] * m.m[2][3] - 
					  m.m[1][1] * m.m[0][3] * m.m[2][2] - 
					  m.m[2][1] * m.m[0][2] * m.m[1][3] + 
					  m.m[2][1] * m.m[0][3] * m.m[1][2];

			inv[7] = m.m[0][0] * m.m[1][2] * m.m[2][3] - 
					 m.m[0][0] * m.m[1][3] * m.m[2][2] - 
					 m.m[1][0] * m.m[0][2] * m.m[2][3] + 
					 m.m[1][0] * m.m[0][3] * m.m[2][2] + 
					 m.m[2][0] * m.m[0][2] * m.m[1][3] - 
					 m.m[2][0] * m.m[0][3] * m.m[1][2];

			inv[11] = -m.m[0][0] * m.m[1][1] * m.m[2][3] + 
					   m.m[0][0] * m.m[1][3] * m.m[2][1] + 
					   m.m[1][0] * m.m[0][1] * m.m[2][3] - 
					   m.m[1][0] * m.m[0][3] * m.m[2][1] - 
					   m.m[2][0] * m.m[0][1] * m.m[1][3] + 
					   m.m[2][0] * m.m[0][3] * m.m[1][1];

			inv[15] = m.m[0][0] * m.m[1][1] * m.m[2][2] - 
					  m.m[0][0] * m.m[1][2] * m.m[2][1] - 
					  m.m[1][0] * m.m[0][1] * m.m[2][2] + 
					  m.m[1][0] * m.m[0][2] * m.m[2][1] + 
					  m.m[2][0] * m.m[0][1] * m.m[1][2] - 
					  m.m[2][0] * m.m[0][2] * m.m[1][1];

			det = m.m[0][0] * inv[0] + m.m[0][1] * inv[4] + m.m[0][2] * inv[8] + m.m[0][3] * inv[12];

			if (std::abs(det) < 1e-6f)
				return false;

			det = 1.0f / det;

			for (i = 0; i < 4; i++) {
				invOut.m[i][0] = inv[i*4 + 0] * det;
				invOut.m[i][1] = inv[i*4 + 1] * det;
				invOut.m[i][2] = inv[i*4 + 2] * det;
				invOut.m[i][3] = inv[i*4 + 3] * det;
			}

			return true;
		}

		math::vector3 get_ray_direction(math::vector2 screen_pos, math::vector2 dimensions, const math::matrix4& viewmatrix)
		{
			math::matrix4 inv_view;
			if (!invert_matrix4(viewmatrix, inv_view))
				return { 0.f, 0.f, 0.f };

			float ndcX = (2.0f * screen_pos.x / dimensions.x) - 1.0f;
			float ndcY = 1.0f - (2.0f * screen_pos.y / dimensions.y);

			math::vector4 p1_clip = { ndcX, ndcY, 0.0f, 1.0f };
			math::vector4 p2_clip = { ndcX, ndcY, 1.0f, 1.0f };

			math::vector4 p1_world = inv_view.multiply(p1_clip);
			math::vector4 p2_world = inv_view.multiply(p2_clip);

			if (std::abs(p1_world.w) < 1e-6f || std::abs(p2_world.w) < 1e-6f)
				return { 0.f, 0.f, 0.f };

			math::vector3 w1 = { p1_world.x / p1_world.w, p1_world.y / p1_world.w, p1_world.z / p1_world.w };
			math::vector3 w2 = { p2_world.x / p2_world.w, p2_world.y / p2_world.w, p2_world.z / p2_world.w };

			math::vector3 dir = w2 - w1;
			float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
			if (len > 1e-5f)
			{
				dir.x /= len;
				dir.y /= len;
				dir.z /= len;
			}
			return dir;
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

			bool autoclicker_active = settings::botter::autoclicker_enabled;
			bool no_spread_active = settings::botter::db_spread_raycast;

			// Resolve DataModel transitions
			static auto last_resolve_attempt = std::chrono::steady_clock::now() - std::chrono::seconds(5);
			if (game::datamodel.address != last_dm_address)
			{
				cached_lua_state = 0;
				cached_global_state = 0;
				cached_rngstate_offset = 0;
				is_resolving = false;
				last_resolve_attempt = std::chrono::steady_clock::now() - std::chrono::seconds(5);
				last_dm_address = game::datamodel.address;
			}

			// Asynchronously resolve Luau RNG offset if not cached
			if (no_spread_active && cached_global_state == 0 && game::datamodel.address != 0 && !is_resolving)
			{
				auto now = std::chrono::steady_clock::now();
				if (std::chrono::duration_cast<std::chrono::seconds>(now - last_resolve_attempt).count() >= 3)
				{
					last_resolve_attempt = now;
					is_resolving = true;
					std::thread([]() {
						std::uint64_t L = luau::find_lua_state();
						if (L != 0)
						{
							std::uint64_t g = 0;
							std::uint64_t offset = luau::find_rngstate_offset(L, g);
							if (g != 0 && offset != 0)
							{
								cached_lua_state = L;
								cached_global_state = g;
								cached_rngstate_offset = offset;
								printf("[ LOWLIFE ]: Asynchronously resolved global_State to 0x%llx, rngstate offset to 0x%llx\n", g, offset);
							}
						}
						is_resolving = false;
					}).detach();
				}
			}


			if ((!autoclicker_active && !no_spread_active) || check::textchatopen || !game::workspace.address)
			{
				continue;
			}

			// Check keybind state for autoclicker
			bool autoclick_gated = autoclicker_active && get_keybind_state();

			// Check if local player is holding a tool (and if it is a weapon)
			bool holding_tool = false;
			std::string tool_name = "";
			rbx::instance_t equipped_tool = {};
			if (game::local_player.address != 0)
			{
				rbx::player_t lp{ game::local_player.address };
				rbx::model_instance_t model = lp.get_model_instance();
				if (model.address != 0)
				{
					for (rbx::instance_t& child : model.get_children<rbx::instance_t>())
					{
						std::string cc = child.get_class_name();
						if (cc == "Tool" || cc == "HopperBin")
						{
							holding_tool = true;
							equipped_tool = child;
							tool_name = child.get_name();
							break;
						}
					}
				}
			}

			bool is_weapon = false;
			if (holding_tool && equipped_tool.address != 0)
			{
				std::string lower_tool = tool_name;
				std::transform(lower_tool.begin(), lower_tool.end(), lower_tool.begin(), ::tolower);
				
				bool is_non_weapon = (
					lower_tool.find("wallet") != std::string::npos ||
					lower_tool.find("phone") != std::string::npos ||
					lower_tool.find("key") != std::string::npos ||
					lower_tool.find("hamburger") != std::string::npos ||
					lower_tool.find("pizza") != std::string::npos ||
					lower_tool.find("chicken") != std::string::npos ||
					lower_tool.find("water") != std::string::npos ||
					lower_tool.find("pepperspray") != std::string::npos ||
					lower_tool.find("handcuffs") != std::string::npos ||
					lower_tool.find("tipjar") != std::string::npos ||
					lower_tool.find("stomp-effect") != std::string::npos ||
					lower_tool.find("visual") != std::string::npos ||
					lower_tool.find("debris") != std::string::npos ||
					lower_tool.find("effect") != std::string::npos
				);
				
				if (!is_non_weapon)
				{
					is_weapon = true;
				}
			}

			// Require holding a weapon tool for triggerbot/autoclicker to fire
			if (autoclick_gated && (!holding_tool || !is_weapon))
			{
				autoclick_gated = false;
			}

			// If autoclicker is not actively firing, and we either don't want no-spread or aren't holding a tool, skip
			if (!autoclick_gated && (!no_spread_active || !holding_tool))
			{
				continue;
			}

			static bool was_holding_tool = false;
			static int loop_counter = 0;

			// Apply No Spread unconditionally when holding a tool and the feature is enabled
			if (no_spread_active && holding_tool)
			{
				if (cached_global_state != 0)
				{
					if (!was_holding_tool)
					{
						was_holding_tool = true;
						scan_gc_heap_for_random_objects(cached_global_state);
					}

					loop_counter++;
					if (loop_counter >= 100)
					{
						loop_counter = 0;
						scan_gc_heap_for_random_objects(cached_global_state);
					}
				}

				if (cached_global_state != 0 && cached_rngstate_offset != 0)
				{
					memory->write<std::uint64_t>(cached_global_state + cached_rngstate_offset, 0);
				}
			}
			else
			{
				// Reset tracking when not actively doing no-spread or not holding a tool
				// (this ensures we re-scan when a tool is equipped again)
				was_holding_tool = false;
				loop_counter = 0;
			}

			// Active window and cursor checks
			HWND active_wnd = GetForegroundWindow();
			HWND roblox_wnd = game::wnd;
			if (!roblox_wnd)
			{
				roblox_wnd = FindWindowA(nullptr, "Roblox");
				if (roblox_wnd) game::wnd = roblox_wnd;
			}
			if (!roblox_wnd || active_wnd != roblox_wnd)
			{
				continue;
			}

			if (!GetCursorPos(&cursor_pt)) continue;
			ScreenToClient(roblox_wnd, &cursor_pt);

			math::vector2 dims = game::visengine.get_dimensions();
			if (cursor_pt.x < 0 || cursor_pt.y < 0 || cursor_pt.x > dims.x || cursor_pt.y > dims.y)
			{
				continue;
			}

			math::vector3 camera_pos = {};
			rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
			if (camera_inst.address != 0)
			{
				rbx::camera_t camera{ camera_inst.address };
				camera_pos = camera.get_position();
			}

			std::shared_ptr<std::vector<cache::entity_t>> players_snapshot;
			cache::entity_t local_player_snapshot = {};
			{
				std::lock_guard<std::mutex> lock(cache::mtx);
				players_snapshot = cache::cached_players;
				local_player_snapshot = cache::cached_local_player;
			}

			if (!players_snapshot) continue;

			math::matrix4 view = game::visengine.get_viewmatrix();

			// Precalculate ray direction if raycast hitbox check or db_spread_raycast is enabled
			math::vector3 cursor_ray_dir = {};
			bool ray_valid = false;
			if (settings::botter::raycast_hitbox || no_spread_active)
			{
				cursor_ray_dir = get_ray_direction({ (float)cursor_pt.x, (float)cursor_pt.y }, dims, view);
				float ray_len_sq = cursor_ray_dir.x * cursor_ray_dir.x + cursor_ray_dir.y * cursor_ray_dir.y + cursor_ray_dir.z * cursor_ray_dir.z;
				if (ray_len_sq > 0.1f)
				{
					ray_valid = true;
				}
			}

			bool clicked_this_tick = false;

			for (auto& player : *players_snapshot)
			{
				if (cache::is_local_player(player))
				{
					continue;
				}

				if (settings::botter::team_check)
				{
					if (!local_player_snapshot.crew_id.empty() && !player.crew_id.empty() &&
						local_player_snapshot.crew_id != "0" && player.crew_id != "0" &&
						local_player_snapshot.crew_id == player.crew_id)
					{
						continue;
					}
				}

				// Unconditionally skip dead players (health <= 0)
				if (player.humanoid.address != 0)
				{
					try {
						float health = const_cast<cache::entity_t&>(player).humanoid.get_health();
						if (health <= 0.0f || !std::isfinite(health))
						{
							continue;
						}
					} catch (...) {}
				}

				if (settings::botter::knocked_check && is_player_knocked(player))
				{
					continue;
				}

				// -- INTERSECTION CHECK --
				bool is_aiming_at_this_player = false;
				if (ray_valid)
				{
					static const std::vector<std::string> parts_to_check = {
						"HumanoidRootPart", "Head", "Torso", "UpperTorso", "LowerTorso",
						"Left Arm", "LeftUpperArm", "LeftLowerArm", "LeftHand",
						"Right Arm", "RightUpperArm", "RightLowerArm", "RightHand",
						"Left Leg", "LeftUpperLeg", "LeftLowerLeg", "LeftFoot",
						"Right Leg", "RightUpperLeg", "RightLowerLeg", "RightFoot"
					};
					
					float scale = settings::botter::hitbox_size / 100.0f;
					for (const auto& part_name : parts_to_check)
					{
						auto part_it = player.parts.find(part_name);
						if (part_it != player.parts.end())
						{
							rbx::part_t p_part = part_it->second;
							if (p_part.address)
							{
								rbx::primitive_t p_prim = p_part.get_primitive();
								if (p_prim.address)
								{
									math::vector3 pos = p_prim.get_position();
									math::vector3 size = p_prim.get_size();
									if (part_name == "HumanoidRootPart")
									{
										size = { 4.0f, 6.0f, 2.0f };
									}
									math::matrix3 rot = p_prim.get_rotation();

									cached_part_t box;
									box.position = pos;
									box.rotation = rot;
									box.size = size * scale;
									box.type = 0;

									float dist = 0.0f;
									if (ray_intersects_obb(camera_pos, cursor_ray_dir, 2000.0f, box, dist))
									{
										is_aiming_at_this_player = true;
										break;
									}
								}
							}
						}
					}
				}

				// Autoclicker execution logic
				if (autoclick_gated)
				{
					bool hit = false;
					if (settings::botter::raycast_hitbox)
					{
						hit = is_aiming_at_this_player;
					}
					else
					{
						// Bounding box screen space check
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
										HWND roblox_window = game::wnd;
										if (!roblox_window) roblox_window = FindWindowA(nullptr, "Roblox");
										RECT client_rect{};
										POINT client_pos{};
										float offset_x = 0.f;
										float offset_y = 0.f;
										if (roblox_window && GetClientRect(roblox_window, &client_rect))
										{
											client_pos.x = client_rect.left;
											client_pos.y = client_rect.top;
											ClientToScreen(roblox_window, &client_pos);
											offset_x = (float)client_pos.x;
											offset_y = (float)client_pos.y;
										}

										float client_cursor_x = (float)cursor_pt.x + offset_x;
										float client_cursor_y = (float)cursor_pt.y + offset_y;

										float scale = settings::botter::hitbox_size / 100.0f;
										float width = right - left;
										float height = bottom - top;
										float delta_w = (width * scale - width) * 0.5f;
										float delta_h = (height * scale - height) * 0.5f;
										float target_left = left - delta_w;
										float target_right = right + delta_w;
										float target_top = top - delta_h;
										float target_bottom = bottom + delta_h;

										if (client_cursor_x >= target_left && client_cursor_x <= target_right &&
											client_cursor_y >= target_top && client_cursor_y <= target_bottom)
										{
											hit = true;
										}
									}
								}
							}
						}
					}

					// Perform wall check ONLY if aiming at the player (hit is true) and wall check is enabled
					if (hit && settings::botter::wall_check && camera_inst.address != 0)
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
						hit = any_part_visible;
					}

					if (hit)
					{
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
		std::uint64_t lp_address = 0;
		{
			std::lock_guard<std::mutex> lock(cache::mtx);
			lp_address = cache::cached_local_player.instance.address;
		}
		if (lp_address == 0 || game::local_character.address == 0)
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
		std::uint64_t lp_address = 0;
		{
			std::lock_guard<std::mutex> lock(cache::mtx);
			lp_address = cache::cached_local_player.instance.address;
		}
		if (lp_address == 0 || game::local_character.address == 0)
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
				rbx::instance_t ammo_val_obj = equipped_tool.find_descendant_value_by_name_substrings({ "ammo", "clip" });
				if (ammo_val_obj.address != 0)
				{
					int val = read_value_instance(ammo_val_obj);
					return val;
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
			if (cache::is_local_player(player))
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
				rbx::instance_t ammo_val_obj = equipped_tool.find_descendant_value_by_name_substrings({ "ammo", "clip" });
				if (ammo_val_obj.address != 0)
				{
					int val = read_value_instance(ammo_val_obj);
					return val;
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
					{
						std::lock_guard<std::mutex> lock(g_shot_detect_mutex);
						target_player = target;
						has_target = true;
					}
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
									if (cache::is_local_player(player))
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
									{
										std::lock_guard<std::mutex> lock(g_shot_detect_mutex);
										target_player = closest_player;
										has_target = true;
									}
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
			bool has_target_val = false;
			{
				std::lock_guard<std::mutex> lock(g_shot_detect_mutex);
				has_target_val = has_target;
			}
			if (settings::shot_detect::enabled && has_target_val)
			{
				bool key_active = get_keybind_state();
				if (key_active)
				{
					bool target_still_valid = false;
					cache::entity_t current_target_state = {};
					std::uint64_t target_addr = 0;
					{
						std::lock_guard<std::mutex> lock(g_shot_detect_mutex);
						target_addr = target_player.instance.address;
					}
					{
						std::lock_guard<std::mutex> lock(cache::mtx);
						if (cache::cached_players)
						{
							for (const auto& player : *cache::cached_players)
							{
								if (player.instance.address == target_addr)
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
						{
							std::lock_guard<std::mutex> lock(g_shot_detect_mutex);
							target_player = current_target_state;
						}

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
