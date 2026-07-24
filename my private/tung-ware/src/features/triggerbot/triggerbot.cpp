#define NOMINMAX
#include <Windows.h>
#include <thread>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <atomic>
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

		// Try multiple Luau Table struct layouts — different Roblox versions vary.
		// {tt_off, lsizenode_off, node_ptr_off}
		struct Layout { int tt_off; int lsz_off; int node_off; };
		static const Layout layouts[] = {
			{8, 11, 32},  // Standard Luau: [next@0][tt@8][..][lsizenode@11][..][node@32]
			{8, 13, 32},  // Variant: lsizenode at +13
			{8, 11, 40},  // Variant: node at +40 (with extra fields between)
			{8, 12, 32},  // Variant: lsizenode at +12
			{0,  6, 32},  // Legacy/custom: tt first (original assumption)
		};

		for (const auto& L : layouts)
		{
			std::uint8_t tt = memory->read<std::uint8_t>(metatable_addr + L.tt_off);
			if (tt < 5 || tt > 9) continue; // must be a valid GCObject type

			std::uint8_t lsizenode = memory->read<std::uint8_t>(metatable_addr + L.lsz_off);
			if (lsizenode > 8) continue; // max 256 nodes sanity check

			std::uint64_t node_ptr = memory->read<std::uint64_t>(metatable_addr + L.node_off);
			if (node_ptr < 0x10000 || node_ptr > 0x7FFFFFFFFFFF || (node_ptr % 8) != 0) continue;

			int size = 1 << lsizenode;
			if (size > 256) continue;

			for (int ni = 0; ni < size; ++ni)
			{
				std::uint64_t node_addr = node_ptr + ni * 32;

				// In Roblox's Luau Node: key type tag packed at +28 (lower 4 bits)
				std::uint32_t val_28 = memory->read<std::uint32_t>(node_addr + 28);
				std::uint32_t key_tt = val_28 & 0xF;

				if (key_tt == 4 || key_tt == 5 || key_tt == 6) // string / table / function key
				{
					std::uint64_t ts_ptr = memory->read<std::uint64_t>(node_addr + 16);
					if (ts_ptr < 0x10000 || ts_ptr > 0x7FFFFFFFFFFF) continue;

					unsigned int len = memory->read<unsigned int>(ts_ptr + 20);
					// "NextNumber"=10, "NextInteger"=11, "NextUnitVector"=13
					if (len == 10 || len == 11 || len == 13)
					{
						std::vector<char> sbuf(len + 1, 0);
						Luck_ReadVirtualMemory(memory->get_process_handle(),
							reinterpret_cast<void*>(ts_ptr + 24), sbuf.data(), len, nullptr);
						std::string str(sbuf.data(), len);
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

	// --- VirtualQueryEx-based Random object scan ---
	// Replaces the broken allgcopages GC traversal.
	// Scans all committed readwrite memory regions <= 4MB for Luau Random userdata objects
	// (tt=7, valid metatable, non-zero state, odd inc). Results are cached and zeroed every
	// 200ms. A full rescan runs in a background thread every 5 seconds.
	static std::vector<std::uint64_t> vmq_random_addrs;
	static std::mutex                 vmq_mutex;
	static std::atomic<bool>          vmq_running{false};
	static std::chrono::steady_clock::time_point vmq_last_scan =
		std::chrono::steady_clock::now() - std::chrono::seconds(10);
	static int vmq_call_count = 0;

	void scan_gc_heap_for_random_objects(std::uint64_t /*global_state*/)
	{
		vmq_call_count++;

		// --- Zero all cached Random objects every call (~200ms) ---
		// Addresses are tagged in lower 3 bits (safe: Random objects are 8-byte aligned):
		//   bits 0-1 == 0 → Layout A: state@+16, inc@+24
		//   bits 0-1 == 1 → Layout B: state@+24, inc@+32  (standard Luau, tt@+8)
		//   bits 0-1 == 2 → Layout C: state@+32, inc@+40  (standard Luau, env ptr before data)
		int zeroed = 0;
		{
			std::lock_guard<std::mutex> lock(vmq_mutex);
			for (std::uint64_t tagged : vmq_random_addrs)
			{
				std::uint64_t addr  = tagged & ~7uLL;
				int layout          = (int)(tagged & 7);
				int state_off       = (layout == 1) ? 24 : (layout == 2) ? 32 : 16;
				int inc_off         = state_off + 8;

				std::uint64_t state = memory->read<std::uint64_t>(addr + state_off);
				std::uint64_t inc   = memory->read<std::uint64_t>(addr + inc_off);
				if (state != 0 || inc != 0)
				{
					memory->write<std::uint64_t>(addr + state_off, 0);
					memory->write<std::uint64_t>(addr + inc_off,   0);
					zeroed++;
				}
			}
		}

		// --- Trigger background VMQ rescan every 5 seconds ---
		auto now = std::chrono::steady_clock::now();
		bool scan_due = !vmq_running.load() &&
			std::chrono::duration_cast<std::chrono::seconds>(now - vmq_last_scan).count() >= 5;

		if (scan_due)
		{
			vmq_last_scan = now;
			vmq_running   = true;
			HANDLE hProc  = memory->get_process_handle();

			std::thread([hProc]() {
				std::vector<std::uint64_t> found;
				MEMORY_BASIC_INFORMATION mbi = {};
				std::uintptr_t addr = 0x10000;
				int regions = 0;

				while (addr < 0x7FFFFFFFFFFuLL)
				{
					if (VirtualQueryEx(hProc, reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) != sizeof(mbi))
						break;
					addr = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;

					// Only committed, readwrite regions of ≤ 4MB
					if (mbi.State   != MEM_COMMIT)    continue;
					if ((mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) == 0) continue;
					if (mbi.RegionSize < 32 || mbi.RegionSize > 4u * 1024 * 1024) continue;

					std::vector<std::uint8_t> buf(mbi.RegionSize);
					SIZE_T bytesRead = 0;
					if (!ReadProcessMemory(hProc, mbi.BaseAddress, buf.data(), mbi.RegionSize, &bytesRead))
						continue;
					regions++;

					for (std::size_t i = 0; i + 48 <= bytesRead; i += 8)
					{
						std::uint64_t obj_addr = reinterpret_cast<std::uint64_t>(mbi.BaseAddress) + i;

						// === Layout B: standard Luau Udata ===
						// [GCObject* next @0][tt=7 @8][marked @9][memcat @10][pad @11]
						// [int len @12][Table* metatable @16][data: state @24, inc @32]
						// Random: len == 16 (sizeof(state)+sizeof(inc) = 2 x uint64)
						if (buf[i + 8] == 7)
						{
							std::int32_t len12 = *reinterpret_cast<const std::int32_t*>(buf.data() + i + 12);
							std::uint64_t mt   = *reinterpret_cast<const std::uint64_t*>(buf.data() + i + 16);
							if (mt >= 0x10000 && mt <= 0x7FFFFFFFFFFuLL && (mt % 8) == 0)
							{
								if (len12 == 16) // state@+24, inc@+32
								{
									std::uint64_t state = *reinterpret_cast<const std::uint64_t*>(buf.data() + i + 24);
									std::uint64_t inc   = *reinterpret_cast<const std::uint64_t*>(buf.data() + i + 32);
									if (state != 0 && inc != 0 && (inc & 1) == 1)
										found.push_back(obj_addr | 1); // tag=1: layout B
								}
								else if (len12 == 24) // env ptr variant: state@+32, inc@+40
								{
									std::uint64_t state = *reinterpret_cast<const std::uint64_t*>(buf.data() + i + 32);
									std::uint64_t inc   = *reinterpret_cast<const std::uint64_t*>(buf.data() + i + 40);
									if (state != 0 && inc != 0 && (inc & 1) == 1)
										found.push_back(obj_addr | 2); // tag=2: layout C
								}
							}
						}

						// === Layout A: Roblox custom (tt first) ===
						// [tt=7 @0][pad @1-3][int len @4][Table* mt @8][state @16][inc @24]
						if (buf[i] == 7)
						{
							std::int32_t len4 = *reinterpret_cast<const std::int32_t*>(buf.data() + i + 4);
							if (len4 == 16 || len4 == 8)
							{
								std::uint64_t mt = *reinterpret_cast<const std::uint64_t*>(buf.data() + i + 8);
								if (mt >= 0x10000 && mt <= 0x7FFFFFFFFFFuLL && (mt % 8) == 0)
								{
									std::uint64_t state = *reinterpret_cast<const std::uint64_t*>(buf.data() + i + 16);
									std::uint64_t inc   = *reinterpret_cast<const std::uint64_t*>(buf.data() + i + 24);
									if (state != 0 && inc != 0 && (inc & 1) == 1)
										found.push_back(obj_addr); // tag=0: layout A
								}
							}
						}
					}
				}

				{
					std::lock_guard<std::mutex> lock(vmq_mutex);
					vmq_random_addrs = std::move(found);
				}

				char nbuf[160];
				std::snprintf(nbuf, sizeof(nbuf),
					"DB VMQ scan done: %d regions, %d Random objs",
					regions, (int)vmq_random_addrs.size());
				notifications::add(nbuf, notifications::NotificationType::Success, 6.0f);

				vmq_running = false;
			}).detach();
		}

		// Periodic status every ~10 calls (~2 seconds)
		if (vmq_call_count % 10 == 0)
		{
			int cached;
			{
				std::lock_guard<std::mutex> lock(vmq_mutex);
				cached = static_cast<int>(vmq_random_addrs.size());
			}
			char buf[160];
			std::snprintf(buf, sizeof(buf),
				"DB VMQ: cached=%d zeroed/call=%d %s",
				cached, zeroed, vmq_running.load() ? "(scanning...)" : "");
			notifications::add(buf, notifications::NotificationType::Info, 2.0f);
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
			int type; 
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

			if (box.type == 0) 
			{
				intersection_distance = t_entry;
				return true;
			}

			
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

			if (box.type == 1) 
			{
				
				
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
			else if (box.type == 2) 
			{
				
				
				float scale_x = box.size.y / box.size.x;
				float a1 = y_d - x_d * scale_x;
				float b1 = y_0 - x_0 * scale_x;

				
				
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

			if (box.type == 1) 
			{
				math::vector3 delta = p - box.position;
				float local_y = delta.x * box.rotation.m[1] + delta.y * box.rotation.m[4] + delta.z * box.rotation.m[7];
				float local_z = delta.x * box.rotation.m[2] + delta.y * box.rotation.m[5] + delta.z * box.rotation.m[8];

				
				float slope_y = local_z * (box.size.y / box.size.z);
				if (local_y > slope_y + 0.05f)
				{
					return false;
				}
			}
			else if (box.type == 2) 
			{
				math::vector3 delta = p - box.position;
				float local_x = delta.x * box.rotation.m[0] + delta.y * box.rotation.m[3] + delta.z * box.rotation.m[6];
				float local_y = delta.x * box.rotation.m[1] + delta.y * box.rotation.m[4] + delta.z * box.rotation.m[7];
				float local_z = delta.x * box.rotation.m[2] + delta.y * box.rotation.m[5] + delta.z * box.rotation.m[8];

				
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

				
				if (game::local_character.address != 0 && child.address == game::local_character.address) continue;

				std::string name = child.get_name();
				if (should_skip_instance(name)) continue;

				std::string class_name = get_class_name_fast(child.address);

				
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

					
					if (child.find_first_child("Humanoid").address != 0 ||
						child.find_first_child_by_class("Humanoid").address != 0 ||
						child.find_first_child("HumanoidRootPart").address != 0) {
						continue;
					}
				}

				
				if (class_name == "Part" || class_name == "MeshPart" || class_name == "WedgePart" ||
					class_name == "CornerWedgePart" || class_name == "TrussPart" || class_name == "SpawnLocation" ||
					class_name == "UnionOperation")
				{
					try {
						rbx::part_t part{ child.address };
						
						
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

			
			if (no_spread_active && cached_global_state == 0 && game::datamodel.address != 0 && !is_resolving)
			{
				auto now = std::chrono::steady_clock::now();
				if (std::chrono::duration_cast<std::chrono::seconds>(now - last_resolve_attempt).count() >= 3)
				{
					last_resolve_attempt = now;
					is_resolving = true;
					std::thread([]() {
						notifications::add("DB NoSpread: resolving Lua state...", notifications::NotificationType::Info, 3.0f);
						std::uint64_t sc = 0;
						std::uint64_t L = luau::find_lua_state(&sc);
						if (L != 0)
						{
							std::uint64_t g = 0;
							std::uint64_t offset = luau::find_rngstate_offset(L, g);
							if (g != 0)
							{
								// Cache global_state EVEN IF rngstate_offset == 0.
								// The DB gun uses Random.new() (not global math.random()), so
								// scan_gc_heap_for_random_objects needs global_state to find and
								// zero those objects in the GC heap.
								cached_lua_state = L;
								cached_global_state = g;
								cached_rngstate_offset = offset; // 0 if LCG scan failed, non-zero if found

								printf("[ TUNG-WARE ]: Resolved global_State=0x%llx, rngstate_offset=0x%llx\n", g, offset);
								char buf[160];
								if (offset != 0)
								{
									std::snprintf(buf, sizeof(buf), "DB NoSpread: FULL RNG resolved! gs=0x%llX off=0x%llX", (unsigned long long)g, (unsigned long long)offset);
									notifications::add(buf, notifications::NotificationType::Success, 8.0f);
								}
								else
								{
									std::snprintf(buf, sizeof(buf), "DB NoSpread: gs=0x%llX found! GC heap scan ACTIVE (no LCG off)", (unsigned long long)g);
									notifications::add(buf, notifications::NotificationType::Info, 8.0f);
								}
							}
							else
							{
								char buf[160];
								std::snprintf(buf, sizeof(buf), "DB NoSpread: L=0x%llX sc=0x%llX but global_state NOT found", (unsigned long long)L, (unsigned long long)sc);
								notifications::add(buf, notifications::NotificationType::Warning, 8.0f);
							}
						}
						else
						{
							char buf[160];
							std::snprintf(buf, sizeof(buf), "DB NoSpread: Lua state NOT found | sc=0x%llX dm=0x%llX", (unsigned long long)sc, (unsigned long long)game::datamodel.address);
							notifications::add(buf, notifications::NotificationType::Warning, 8.0f);
						}
						is_resolving = false;
					}).detach();
				}
			}


			if ((!autoclicker_active && !no_spread_active) || check::textchatopen || !game::workspace.address)
			{
				continue;
			}

			
			bool autoclick_gated = autoclicker_active && get_keybind_state();

			
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

			
			if (autoclick_gated && (!holding_tool || !is_weapon))
			{
				autoclick_gated = false;
			}

			
			if (!autoclick_gated && (!no_spread_active || !holding_tool))
			{
				continue;
			}

			static bool was_holding_tool = false;
			static int loop_counter = 0;

			
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

				// --- DB No Spread: RNG state diagnostics ---
				{
					static int  rng_diag_counter   = 0;
					static bool rng_fired_once     = false;
					rng_diag_counter++;

					bool gs_ok  = (cached_global_state    != 0);
					bool rng_ok = (cached_rngstate_offset != 0);

					// One-shot: first time the actual zero-write executes
					if (gs_ok && rng_ok && !rng_fired_once)
					{
						rng_fired_once = true;
						char buf[128];
						std::snprintf(buf, sizeof(buf),
							"DB NoSpread RNG: zeroing gs=0x%llX off=0x%llX",
							(unsigned long long)cached_global_state,
							(unsigned long long)cached_rngstate_offset);
						notifications::add(buf, notifications::NotificationType::Success, 6.0f);
					}

					// Periodic status every ~200 ticks (~400ms)
					if (rng_diag_counter % 200 == 0)
					{
						char buf[160];
						if (gs_ok && rng_ok)
						{
							std::uint64_t cur_rng = memory->read<std::uint64_t>(cached_global_state + cached_rngstate_offset);
							std::snprintf(buf, sizeof(buf),
								"DB RNG FULL | gs=0x%llX rng_val=0x%llX",
								(unsigned long long)cached_global_state,
								(unsigned long long)cur_rng);
							notifications::add(buf, notifications::NotificationType::Info, 2.0f);
						}
						else if (gs_ok && !rng_ok)
						{
							// global_state found, GC heap scan is active zeroing Random.new() objects
							std::snprintf(buf, sizeof(buf),
								"DB NoSpread: GC heap scan ACTIVE | gs=0x%llX",
								(unsigned long long)cached_global_state);
							notifications::add(buf, notifications::NotificationType::Info, 2.0f);
						}
						else
						{
							notifications::add("DB NoSpread: global_state NOT resolved (RNG inactive)", notifications::NotificationType::Warning, 2.0f);
						}
					}
				}

				// =========================================================
				// DB No Spread: DISTANCE-BASED approach (v2)
				// Writes ALL part primitives inside the tool AND the local
				// player's HRP to be 2.23 studs from the nearest enemy.
				// This covers every possible barrel-position formula the gun
				// script might use (Handle, Barrel, Muzzle, HRP, etc.).
				// =========================================================
				{
					static std::vector<std::uint64_t> db_part_prims; // all tool part prim addrs
					static std::uint64_t db_lp_hrp_prim_addr = 0;    // local player HRP prim addr
					static std::uint64_t db_last_tool_addr2  = 0;
					static int           db_dist_ctr         = 0;

					// Reset and re-collect on tool change
					if (equipped_tool.address != db_last_tool_addr2)
					{
						db_last_tool_addr2   = equipped_tool.address;
						db_part_prims.clear();
						db_lp_hrp_prim_addr  = 0;
						db_dist_ctr          = 0;
					}

					db_dist_ctr++;

					// Collect every BasePart inside the tool (re-scan every ~1 s)
					if (db_part_prims.empty() || db_dist_ctr % 500 == 1)
					{
						db_part_prims.clear();
						for (rbx::instance_t& child : equipped_tool.get_children<rbx::instance_t>())
						{
							std::string cls = child.get_class_name();
							if (cls.find("Part") != std::string::npos ||
								cls == "MeshPart" || cls == "SpecialMesh")
							{
								rbx::part_t cp{ child.address };
								rbx::primitive_t prim = cp.get_primitive();
								if (prim.address != 0)
									db_part_prims.push_back(prim.address);
							}
						}
					}

					// Collect local player HRP prim (re-scan every ~1 s)
					if (db_lp_hrp_prim_addr == 0 || db_dist_ctr % 500 == 1)
					{
						auto it2 = cache::cached_local_player.parts.find("HumanoidRootPart");
						if (it2 != cache::cached_local_player.parts.end())
						{
							rbx::part_t lp_hrp_copy = it2->second;
							rbx::primitive_t lp_prim = lp_hrp_copy.get_primitive();
							if (lp_prim.address != 0)
								db_lp_hrp_prim_addr = lp_prim.address;
						}
					}

					// Find nearest alive enemy + get a reference position for ourselves
					math::vector3 nearest_enemy_pos = {};
					bool enemy_found = false;
					float nearest_dsq2 = 1e18f;

					// Reference: local player HRP real position (read first)
					math::vector3 lp_real_pos = {};
					if (db_lp_hrp_prim_addr != 0)
						lp_real_pos = memory->read<math::vector3>(
							db_lp_hrp_prim_addr + Offsets::Primitive::Position);

					std::shared_ptr<std::vector<cache::entity_t>> snap2;
					{
						std::lock_guard<std::mutex> lk2(cache::mtx);
						snap2 = cache::cached_players;
					}
					if (snap2)
					{
						for (const auto& ent : *snap2)
						{
							if (cache::is_local_player(ent)) continue;
							if (ent.is_knocked)               continue;
							if (ent.health <= 0.0f)           continue;
							auto eit = ent.parts.find("HumanoidRootPart");
							if (eit == ent.parts.end()) continue;
							rbx::part_t ec = eit->second;
							math::vector3 ep = ec.get_primitive().get_position();
							float dx = ep.x - lp_real_pos.x;
							float dy = ep.y - lp_real_pos.y;
							float dz = ep.z - lp_real_pos.z;
							float dsq2 = dx*dx + dy*dy + dz*dz;
							if (dsq2 < nearest_dsq2)
							{
								nearest_dsq2     = dsq2;
								nearest_enemy_pos = ep;
								enemy_found       = true;
							}
						}
					}

				if (enemy_found)
				{
					// Compute direction LP → enemy
					float dx   = nearest_enemy_pos.x - lp_real_pos.x;
					float dy   = nearest_enemy_pos.y - lp_real_pos.y;
					float dz   = nearest_enemy_pos.z - lp_real_pos.z;
					float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

					if (dist > 0.001f)
					{
						float nx = dx / dist, ny = dy / dist, nz = dz / dist;
						// fake_shooter = 2.23 studs behind enemy along LP→enemy axis
						math::vector3 fake_shooter = {
							nearest_enemy_pos.x - nx * 2.23f,
							nearest_enemy_pos.y - ny * 2.23f,
							nearest_enemy_pos.z - nz * 2.23f
						};

						// Always write tool part primitives (Handle, Barrel, etc.)
						// These don't visually move the player character.
						for (std::uint64_t p : db_part_prims)
							memory->write<math::vector3>(p + Offsets::Primitive::Position, fake_shooter);

						// Write local player HRP ONLY while LMB is held (shot window).
						// Continuous write teleports the player; this gates it to the
						// exact frame(s) where the gun script fires and reads HRP.Position.
						bool lmb_held = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
						if (lmb_held && db_lp_hrp_prim_addr != 0)
							memory->write<math::vector3>(
								db_lp_hrp_prim_addr + Offsets::Primitive::Position, fake_shooter);

						// Diagnostic: every ~500 ticks (~1 s)
						if (db_dist_ctr % 500 == 0)
						{
							char nb[160];
							std::snprintf(nb, sizeof(nb),
								"DB Dist: real=%.1f parts=%d lmb=%s",
								dist,
								(int)db_part_prims.size(),
								lmb_held ? "SHOT" : "idle");
							notifications::add(nb, notifications::NotificationType::Info, 3.0f);
						}
					}
				}
				else if (db_dist_ctr % 500 == 0)
				{
					notifications::add("DB Dist: no enemy found", notifications::NotificationType::Warning, 2.0f);
				}
			}
			// =====================================================================

			}
			else
			{
				
				
				was_holding_tool = false;
				loop_counter = 0;
			}

			
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

				
				if (autoclick_gated)
				{
					bool hit = false;
					if (settings::botter::raycast_hitbox)
					{
						hit = is_aiming_at_this_player;
					}
					else
					{
						// Bounding box screen space check for all bones (hitbox detector)
						static const std::vector<std::string> target_bones = {
							"Head", "Torso", "UpperTorso", "LowerTorso",
							"Left Arm", "LeftUpperArm", "LeftLowerArm", "LeftHand",
							"Right Arm", "RightUpperArm", "RightLowerArm", "RightHand",
							"Left Leg", "LeftUpperLeg", "LeftLowerLeg", "LeftFoot",
							"Right Leg", "RightUpperLeg", "RightLowerLeg", "RightFoot",
							"HumanoidRootPart"
						};

						for (const auto& bone_name : target_bones)
						{
							auto part_it = player.parts.find(bone_name);
							if (part_it == player.parts.end()) continue;

							rbx::part_t part = part_it->second;
							if (!part.address) continue;

							rbx::primitive_t prim = part.get_primitive();
							if (!prim.address) continue;

							math::vector3 pos = prim.get_position();
							math::vector3 size = prim.get_size();
							if (bone_name == "HumanoidRootPart")
							{
								size = { 4.0f, 6.0f, 2.0f };
							}
							math::matrix3 rot = prim.get_rotation();

							if (size.x < 0.01f || size.y < 0.01f || size.z < 0.01f) continue;

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
								if (game::visengine.world_to_client(world, out, dims, view))
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
								float client_cursor_x = (float)cursor_pt.x;
								float client_cursor_y = (float)cursor_pt.y;

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
									break;
								}
							}
						}
					}

					
					/*
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
					*/

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
	int sd2_last_ammo_val = -1;

	// Caching target state to eliminate lag/CPU overhead
	std::uint64_t cached_target_address = 0;
	std::uint64_t cached_equipped_tool_address = 0;
	std::uint64_t cached_ammo_object_address = 0;

	void trigger_immediate_click()
	{
		notifications::add("Clicking...", notifications::NotificationType::Success, 1.0f);
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
		WORD vk = '0' + slot; 

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

			if (!game::local_character.address)
			{
				last_local_ammo = -1;
				last_tool = "";
				db_force_equipped = false;
				has_swapped_to_revolver = false;
				continue;
			}

			bool sd1_active = settings::shot_detect::enabled && has_target && get_keybind_state();
			bool sd2_active = settings::shot_detect_2::enabled;
			if (sd2_active) {
				switch (settings::shot_detect_2::trigger_keybind_mode)
				{
				case 0: sd2_active = (GetAsyncKeyState(settings::shot_detect_2::trigger_keybind) & 0x8000) != 0; break;
				case 1: sd2_active = true; break; 
				case 2: sd2_active = true; break;
				}
			}

			bool sd1_swap_active = sd1_active && settings::shot_detect::gunswap_enabled;
			bool sd2_swap_active = sd2_active && settings::shot_detect_2::gunswap_enabled;

			if (!sd1_swap_active && !sd2_swap_active)
			{
				last_local_ammo = -1;
				last_tool = "";
				db_force_equipped = false;
				has_swapped_to_revolver = false;
				continue;
			}

			bool should_start_with_db = (settings::shot_detect::always_start_with_db && sd1_swap_active) || (settings::shot_detect_2::always_start_with_db && sd2_swap_active);

			std::string current_tool = get_local_tool_name();
			std::string lower_tool = current_tool;
			std::transform(lower_tool.begin(), lower_tool.end(), lower_tool.begin(), ::tolower);

			bool is_db = (lower_tool.find("double") != std::string::npos || 
			              lower_tool.find("db") != std::string::npos || 
			              lower_tool.find("barrel") != std::string::npos);

			if (should_start_with_db)
			{
				if (!is_db && !db_force_equipped)
				{
					int target_slot = sd1_swap_active ? settings::shot_detect::db_slot : settings::shot_detect_2::db_slot;
					press_slot_key(target_slot);
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
					if (last_tool != current_tool || last_local_ammo == -1 || ammo > last_local_ammo)
					{
						last_local_ammo = ammo;
						has_swapped_to_revolver = false;
					}
					else if (ammo < last_local_ammo && !has_swapped_to_revolver)
					{
						has_swapped_to_revolver = true;

						int current_delay = sd1_swap_active ? settings::shot_detect::gunswap_delay : settings::shot_detect_2::gunswap_delay;
						Sleep(current_delay);
						
						std::string check_tool = get_local_tool_name();
						std::transform(check_tool.begin(), check_tool.end(), check_tool.begin(), ::tolower);
						bool is_revolver = (check_tool.find("revolver") != std::string::npos || 
						                    check_tool.find("rev") != std::string::npos);
						if (!is_revolver)
						{
							int target_slot = sd1_swap_active ? settings::shot_detect::revolver_slot : settings::shot_detect_2::revolver_slot;
							press_slot_key(target_slot);
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
				if (game::visengine.world_to_client(world, out, dims, view))
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
					return player;
				}
			}
		}

		return {};
	}

	rbx::instance_t get_target_character_model(const cache::entity_t& target)
	{
		if (target.instance.address == 0) return {};

		if (game::workspace.address != 0)
		{
			// 1. Try finding it in game.Workspace.Characters
			rbx::instance_t characters_folder = game::workspace.find_first_child("Characters");
			if (characters_folder.address != 0)
			{
				rbx::instance_t char_model = characters_folder.find_first_child(target.name);
				if (char_model.address != 0)
				{
					return char_model;
				}
			}

			// 2. Try finding it directly in game.Workspace
			rbx::instance_t char_model = game::workspace.find_first_child(target.name);
			if (char_model.address != 0)
			{
				return char_model;
			}
		}

		// 3. Fallback to cached target.model_address
		if (target.model_address != 0)
		{
			return rbx::instance_t{ target.model_address };
		}

		return {};
	}

	int get_target_ammo(const cache::entity_t& target, std::uint64_t* out_tool_addr)
	{
		if (out_tool_addr) *out_tool_addr = 0;
		if (target.instance.address == 0)
		{
			cached_target_address = 0;
			cached_equipped_tool_address = 0;
			cached_ammo_object_address = 0;
			return -1;
		}

		try {
			rbx::instance_t model_inst = get_target_character_model(target);
			if (model_inst.address == 0)
			{
				cached_target_address = 0;
				cached_equipped_tool_address = 0;
				cached_ammo_object_address = 0;
				return -1;
			}

			// Find the equipped tool in the character
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

			// If no tool equipped in character, try Backpack
			if (equipped_tool.address == 0 && target.instance.address != 0)
			{
				try {
					rbx::instance_t player_inst{ target.instance.address };
					rbx::instance_t backpack = player_inst.find_first_child("Backpack");
					if (backpack.address != 0)
					{
						for (auto& child : backpack.get_children())
						{
							std::string cclass = child.get_class_name();
							if (cclass == "Tool" || cclass == "HopperBin")
							{
								equipped_tool = child;
								break;
							}
						}
					}
				} catch (...) {}
			}

			if (equipped_tool.address == 0)
			{
				cached_equipped_tool_address = 0;
				cached_ammo_object_address = 0;
				return -1;
			}

			if (out_tool_addr) *out_tool_addr = equipped_tool.address;

			// Read directly from cached address if target and tool are unchanged
			if (target.instance.address == cached_target_address &&
				equipped_tool.address == cached_equipped_tool_address &&
				cached_ammo_object_address != 0)
			{
				rbx::instance_t ammo_val_obj{ cached_ammo_object_address };
				return read_value_instance(ammo_val_obj);
			}

			cached_target_address = target.instance.address;
			cached_equipped_tool_address = equipped_tool.address;
			cached_ammo_object_address = 0;

			// Helper lambda: try to find ammo in a container instance
			auto try_find_ammo = [&](rbx::instance_t& container) -> rbx::instance_t {
				if (container.address == 0) return {};
				// 1. Direct children scan (exact name match)
				try {
					for (auto& child : container.get_children())
					{
						std::string name = child.get_name();
						std::transform(name.begin(), name.end(), name.begin(), ::tolower);
						if (name == "ammo" || name == "clip" || name == "bullets" || name == "rounds" || name == "magazine")
						{
							std::string cclass = child.get_class_name();
							if (cclass.find("Value") != std::string::npos)
								return child;
						}
					}
				} catch (...) {}
				// 2. Substring descendant search (deep scan)
				try {
					return container.find_descendant_value_by_name_substrings({ "ammo", "clip", "bullets", "rounds", "mag" });
				} catch (...) {}
				return {};
			};

			// --- Pass 1: equipped tool (character) ---
			rbx::instance_t ammo_val_obj = try_find_ammo(equipped_tool);

			// --- Pass 2: DataFolder inside the tool ---
			if (ammo_val_obj.address == 0)
			{
				try {
					rbx::instance_t data_folder = equipped_tool.find_first_child("DataFolder");
					if (data_folder.address != 0)
						ammo_val_obj = try_find_ammo(data_folder);
				} catch (...) {}
			}

			// --- Pass 3: try the character model itself (some games store ammo there) ---
			if (ammo_val_obj.address == 0)
				ammo_val_obj = try_find_ammo(model_inst);

			// --- Pass 4: try the Backpack's version of the same tool ---
			if (ammo_val_obj.address == 0 && target.instance.address != 0)
			{
				try {
					rbx::instance_t player_inst{ target.instance.address };
					rbx::instance_t backpack = player_inst.find_first_child("Backpack");
					if (backpack.address != 0)
					{
						std::string tool_name = equipped_tool.get_name();
						rbx::instance_t bp_tool = backpack.find_first_child(tool_name);
						if (bp_tool.address != 0)
							ammo_val_obj = try_find_ammo(bp_tool);
						if (ammo_val_obj.address == 0)
							ammo_val_obj = try_find_ammo(backpack);
					}
				} catch (...) {}
			}

			if (ammo_val_obj.address != 0)
			{
				bool newly_cached = (cached_ammo_object_address == 0);
				cached_ammo_object_address = ammo_val_obj.address;
				int val = read_value_instance(ammo_val_obj);
				if (newly_cached)
				{
					// Notify user that ammo value was found
					char notif[128];
					std::snprintf(notif, sizeof(notif),
						"Ammo Found! Tool: %s | Val: %d",
						equipped_tool.get_name().c_str(), val);
					notifications::add(notif, notifications::NotificationType::Success, 3.0f);
				}
				return val;
			}
		} catch (...) {}

		return -1;
	}

	static bool check_target_muzzle_flash(const cache::entity_t& target)
	{
		if (target.instance.address == 0) return false;
		try {
			rbx::instance_t model_inst = get_target_character_model(target);
			if (model_inst.address == 0) return false;

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
			if (equipped_tool.address == 0) return false;

			for (auto& child : equipped_tool.get_children())
			{
				if (!child.address) continue;
				std::string cclass = child.get_class_name();
				std::string cname = child.get_name();
				std::transform(cname.begin(), cname.end(), cname.begin(), ::tolower);

				if (cclass == "PointLight" || cclass == "SpotLight" || cclass == "SurfaceLight" ||
					cclass == "ParticleEmitter" || cclass == "Beam" || cclass == "Trail" ||
					cclass == "Fire" || cclass == "Smoke")
				{
					bool enabled = memory->read<bool>(child.address + Offsets::Misc::Value);
					if (enabled) return true;
				}
				if (cname.find("flash") != std::string::npos || cname.find("muzzle") != std::string::npos || cname.find("flame") != std::string::npos)
				{
					if (cclass == "Part" || cclass == "MeshPart" || cclass == "SpecialMesh")
					{
						float trans = memory->read<float>(child.address + Offsets::BasePart::Transparency);
						if (trans < 0.8f) return true;
					}
				}
			}
		} catch (...) {}
		return false;
	}

	void run()
	{
		std::thread(gun_swap_loop).detach();

		bool mb5_was_pressed = false;
		bool is_clicking = false;
		bool is_first_click = true;
		auto last_click_time = std::chrono::steady_clock::now();
		auto sd_trigger_start_time = std::chrono::steady_clock::now();
		int current_delay = 100;
		std::uint64_t last_tool_addr = 0;

		bool sd2_is_clicking = false;
		bool sd2_is_first_click = true;
		auto sd2_last_click_time = std::chrono::steady_clock::now();
		auto sd2_trigger_start_time = std::chrono::steady_clock::now();
		int sd2_current_delay = 100;
		std::uint64_t sd2_last_tool_addr = 0;

		std::unordered_set<std::uint64_t> sd2_seen_parts;
		auto sd2_last_seen_clear = std::chrono::steady_clock::now();

		while (true)
		{
			Sleep(5);

			if (!game::workspace.address || !game::local_player.address)
			{
				is_clicking = false;
				continue;
			}

			
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
					sd2_last_ammo_val = -1;
					last_tool_addr = 0;
					sd2_last_tool_addr = 0;
					cached_target_address = target.instance.address;
					cached_equipped_tool_address = 0;
					cached_ammo_object_address = 0;
					is_clicking = false;
					sd2_is_clicking = false;
					notifications::add("Shot Detect Target: " + target.display_name, notifications::NotificationType::Success, 3.0f);
				}
			}
			mb5_was_pressed = mb5_is_pressed;

			bool has_target_val = false;
			{
				std::lock_guard<std::mutex> lock(g_shot_detect_mutex);
				has_target_val = has_target;
			}

			bool target_still_valid = false;
			cache::entity_t current_target_state = {};
			int current_target_ammo = -1;
			std::uint64_t current_tool_addr = 0;

			if (has_target_val && (settings::shot_detect::enabled || settings::shot_detect_2::enabled))
			{
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
					current_target_ammo = get_target_ammo(current_target_state, &current_tool_addr);
					
					static int frame_counter = 0;
					if (++frame_counter % 100 == 0)
					{
						notifications::add("Target: " + current_target_state.display_name + " | Ammo: " + std::to_string(current_target_ammo), notifications::NotificationType::Info, 0.5f);
					}
				if (settings::shot_detect::enabled && has_target_val)
			{
				bool key_active = get_keybind_state();
				if (key_active && target_still_valid)
				{
					bool flash_triggered = check_target_muzzle_flash(current_target_state);
					bool ammo_triggered = (current_target_ammo >= 0 && current_tool_addr == last_tool_addr && last_ammo_val >= 0 && current_target_ammo < last_ammo_val);

					if (current_tool_addr != last_tool_addr)
					{
						last_ammo_val = current_target_ammo;
						last_tool_addr = current_tool_addr;
					}

					if (flash_triggered || ammo_triggered)
					{
						notifications::add("Shot Detect 1.0 Triggered!", notifications::NotificationType::Info, 2.0f);
						if (!is_clicking)
						{
							is_clicking = true;
							is_first_click = true;
							sd_trigger_start_time = std::chrono::steady_clock::now();
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
					if (current_target_ammo >= 0) last_ammo_val = current_target_ammo;
				}
				else
				{
					is_clicking = false;
					last_ammo_val = -1;
					last_tool_addr = 0;
				}
			}
			else
			{
				is_clicking = false;
				last_ammo_val = -1;
				last_tool_addr = 0;
			}

			if (is_clicking)
			{
				if (settings::shot_detect::click_mode == 0)
				{
					auto now = std::chrono::steady_clock::now();
					auto elapsed_since_trigger = std::chrono::duration_cast<std::chrono::milliseconds>(now - sd_trigger_start_time).count();
					if (elapsed_since_trigger >= 250) 
					{
						is_clicking = false;
					}
					else
					{
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
							is_first_click = false;

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
						}
					}
				}
				else if (settings::shot_detect::click_mode == 1)
				{
					auto now = std::chrono::steady_clock::now();
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_click_time).count();

					if (duration >= current_delay && is_first_click)
					{
						trigger_immediate_click();
						is_first_click = false;
						is_clicking = false;
					}
				}
			}

			if (settings::shot_detect_2::enabled && has_target_val)
			{
				bool key_active = false;
				switch (settings::shot_detect_2::trigger_keybind_mode)
				{
				case 0: key_active = (GetAsyncKeyState(settings::shot_detect_2::trigger_keybind) & 0x8000) != 0; break;
				case 1: 
					{
						static bool key_was_pressed2 = false;
						static bool toggle_state2 = false;
						bool pressed = (GetAsyncKeyState(settings::shot_detect_2::trigger_keybind) & 0x8000) != 0;
						if (pressed && !key_was_pressed2) toggle_state2 = !toggle_state2;
						key_was_pressed2 = pressed;
						key_active = toggle_state2;
					}
					break;
				case 2: key_active = true; break;
				}

				if (key_active && target_still_valid)
				{
					bool flash_triggered2 = check_target_muzzle_flash(current_target_state);
					bool ammo_triggered2 = (current_target_ammo >= 0 && current_tool_addr == sd2_last_tool_addr && sd2_last_ammo_val >= 0 && current_target_ammo < sd2_last_ammo_val);

					if (current_tool_addr != sd2_last_tool_addr)
					{
						sd2_last_ammo_val = current_target_ammo;
						sd2_last_tool_addr = current_tool_addr;
					}

					if (flash_triggered2 || ammo_triggered2)
					{
						notifications::add("Shot Detect 2.0 Triggered!", notifications::NotificationType::Info, 2.0f);
						if (!sd2_is_clicking)
						{
							sd2_is_clicking = true;
							sd2_is_first_click = true;
							sd2_trigger_start_time = std::chrono::steady_clock::now();
							if (settings::shot_detect_2::randomize_delay)
							{
								int min_val = settings::shot_detect_2::min_delay;
								int max_val = settings::shot_detect_2::max_delay;
								if (min_val > max_val) std::swap(min_val, max_val);
								if (min_val < 1) min_val = 1;
								if (max_val < 1) max_val = 1;
								std::random_device rd;
								std::mt19937 gen(rd());
								std::uniform_int_distribution<> distrib(min_val, max_val);
								sd2_current_delay = distrib(gen);
							}
							else
							{
								sd2_current_delay = settings::shot_detect_2::click_delay;
							}
							sd2_last_click_time = std::chrono::steady_clock::now();
						}
					}
					if (current_target_ammo >= 0) sd2_last_ammo_val = current_target_ammo;
				}
				else
				{
					sd2_is_clicking = false;
					sd2_last_ammo_val = -1;
					sd2_last_tool_addr = 0;
				}
			}
			else
			{
				sd2_is_clicking = false;
				sd2_last_ammo_val = -1;
				sd2_last_tool_addr = 0;
			}

			if (sd2_is_clicking)
			{
				if (settings::shot_detect_2::click_mode == 0) 
				{
					auto now = std::chrono::steady_clock::now();
					auto elapsed_since_trigger = std::chrono::duration_cast<std::chrono::milliseconds>(now - sd2_trigger_start_time).count();
					if (elapsed_since_trigger >= 250) 
					{
						sd2_is_clicking = false;
					}
					else
					{
						auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - sd2_last_click_time).count();

						int target_delay = sd2_current_delay;
						if (!settings::shot_detect_2::randomize_delay)
						{
							if (sd2_is_first_click)
							{
								target_delay = settings::shot_detect_2::click_delay;
							}
							else
							{
								int cps = settings::shot_detect_2::cps;
								if (cps < 1) cps = 1;
								target_delay = 1000 / cps;
							}
						}

						if (duration >= target_delay)
						{
							trigger_immediate_click();
							sd2_last_click_time = now;
							sd2_is_first_click = false;

							if (settings::shot_detect_2::randomize_delay)
							{
								int min_val = settings::shot_detect_2::min_delay;
								int max_val = settings::shot_detect_2::max_delay;
								if (min_val > max_val) std::swap(min_val, max_val);
								if (min_val < 1) min_val = 1;
								if (max_val < 1) max_val = 1;
								std::random_device rd;
								std::mt19937 gen(rd());
								std::uniform_int_distribution<> distrib(min_val, max_val);
								sd2_current_delay = distrib(gen);
							}
						}
					}
				}
				else if (settings::shot_detect_2::click_mode == 1) 
				{
					auto now = std::chrono::steady_clock::now();
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - sd2_last_click_time).count();

					if (duration >= sd2_current_delay && sd2_is_first_click)
					{
						trigger_immediate_click();
						sd2_is_first_click = false;
						sd2_is_clicking = false; 
					}
				}
			}
		}
	}
}

// ============================================================
// color_detect – bullet-color-based shot detection (ENHANCED)
// ============================================================
namespace color_detect
{
	std::atomic<bool> shot_fired{ false };

	static void trigger_immediate_click()
	{
		notifications::add("Color Detect: Clicking...", notifications::NotificationType::Success, 1.0f);
		INPUT input = {};
		input.type = INPUT_MOUSE;
		input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
		SendInput(1, &input, sizeof(INPUT));
		Sleep(15);
		input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
		SendInput(1, &input, sizeof(INPUT));
	}

	static bool get_keybind_state()
	{
		switch (settings::color_detect::trigger_keybind_mode)
		{
		case 0:
			return (GetAsyncKeyState(settings::color_detect::trigger_keybind) & 0x8000) != 0;
		case 1:
			{
				static bool key_was_pressed = false;
				static bool toggle_state = false;
				bool pressed = (GetAsyncKeyState(settings::color_detect::trigger_keybind) & 0x8000) != 0;
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

	static bool color_matches(float r, float g, float b)
	{
		float dr = r - settings::color_detect::bullet_color[0];
		float dg = g - settings::color_detect::bullet_color[1];
		float db = b - settings::color_detect::bullet_color[2];
		float dist = std::sqrt(dr*dr + dg*dg + db*db);
		return dist <= settings::color_detect::color_tolerance;
	}

	static bool get_part_color(std::uint64_t part_addr, float& r, float& g, float& b)
	{
		try {
			std::uint64_t color_addr = part_addr + Offsets::BasePart::Color3;
			r = memory->read<float>(color_addr);
			g = memory->read<float>(color_addr + 4);
			b = memory->read<float>(color_addr + 8);
			if (r < 0.0f || r > 1.0f || g < 0.0f || g > 1.0f || b < 0.0f || b > 1.0f)
				return false;
			return true;
		} catch (...) { return false; }
	}

	static bool scan_instance_tree(rbx::instance_t parent, math::vector3 local_pos, float radius_sq, std::unordered_set<std::uint64_t>& seen_parts, int depth = 0)
	{
		if (depth > 6 || !parent.address) return false;
		std::vector<rbx::instance_t> children;
		try { children = parent.get_children(); } catch (...) { return false; }

		for (auto& child : children)
		{
			if (!child.address || seen_parts.count(child.address)) continue;
			seen_parts.insert(child.address);

			try {
				std::string cname = child.get_class_name();
				if (cname == "Part" || cname == "MeshPart" || cname == "SpecialMesh" || cname == "WedgePart")
				{
					float trans = memory->read<float>(child.address + Offsets::BasePart::Transparency);
					if (trans > 0.6f) continue;

					float r, g, b;
					if (!get_part_color(child.address, r, g, b)) continue;
					if (!color_matches(r, g, b)) continue;

					try {
						rbx::part_t part_inst{ child.address };
						math::vector3 pos = part_inst.get_primitive().get_position();
						float dx = pos.x - local_pos.x;
						float dy = pos.y - local_pos.y;
						float dz = pos.z - local_pos.z;
						float dist_sq = dx*dx + dy*dy + dz*dz;
						if (dist_sq <= radius_sq)
						{
							char notif[128];
							std::snprintf(notif, sizeof(notif),
								"Color Detect: Bullet found (%.2f,%.2f,%.2f) dist=%.1f",
								r, g, b, std::sqrt(dist_sq));
							notifications::add(notif, notifications::NotificationType::Success, 2.0f);
							return true;
						}
					} catch (...) {}
				}
				else if (cname == "Folder" || cname == "Model" || cname == "Camera")
				{
					if (scan_instance_tree(child, local_pos, radius_sq, seen_parts, depth + 1))
						return true;
				}
			} catch (...) {}
		}
		return false;
	}

	void run()
	{
		bool is_clicking = false;
		bool is_first_click = true;
		auto last_click_time = std::chrono::steady_clock::now();
		int current_delay = 100;

		std::unordered_set<std::uint64_t> seen_parts;
		auto last_seen_clear = std::chrono::steady_clock::now();

		while (true)
		{
			Sleep(settings::color_detect::scan_interval_ms > 0 ? settings::color_detect::scan_interval_ms : 8);

			if (!settings::color_detect::enabled || !game::workspace.address || !game::local_character.address)
			{
				is_clicking = false;
				seen_parts.clear();
				continue;
			}

			if (!get_keybind_state())
			{
				is_clicking = false;
				continue;
			}

			auto now_ts = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::seconds>(now_ts - last_seen_clear).count() >= 2)
			{
				seen_parts.clear();
				last_seen_clear = now_ts;
			}

			math::vector3 local_pos = {};
			try {
				rbx::instance_t local_char{ game::local_character.address };
				rbx::instance_t hrp = local_char.find_first_child("HumanoidRootPart");
				if (hrp.address != 0)
				{
					rbx::part_t hrp_part{ hrp.address };
					local_pos = hrp_part.get_primitive().get_position();
				}
			} catch (...) { continue; }

			float r_sq = settings::color_detect::scan_radius * settings::color_detect::scan_radius;

			bool bullet_detected = false;
			try {
				rbx::instance_t workspace{ game::workspace.address };
				
				// Scan priority folders: Ignored, Debris, Bullets, Raycasts, Effects, Camera
				static const std::vector<std::string> priority_folders = { "Ignored", "Debris", "Bullets", "Raycasts", "Effects", "Camera" };
				for (const auto& fname : priority_folders)
				{
					rbx::instance_t fobj = workspace.find_first_child(fname);
					if (fobj.address != 0)
					{
						if (scan_instance_tree(fobj, local_pos, r_sq, seen_parts, 0))
						{
							bullet_detected = true;
							break;
						}
					}
				}

				if (!bullet_detected)
				{
					bullet_detected = scan_instance_tree(workspace, local_pos, r_sq, seen_parts, 0);
				}
			} catch (...) {}

			if (bullet_detected && !is_clicking)
			{
				is_clicking = true;
				is_first_click = true;

				if (settings::color_detect::randomize_delay)
				{
					int mn = settings::color_detect::min_delay;
					int mx = settings::color_detect::max_delay;
					if (mn > mx) std::swap(mn, mx);
					if (mn < 1) mn = 1;
					if (mx < 1) mx = 1;
					std::random_device rd;
					std::mt19937 gen(rd());
					std::uniform_int_distribution<> distrib(mn, mx);
					current_delay = distrib(gen);
				}
				else
				{
					current_delay = settings::color_detect::click_delay;
				}
				last_click_time = std::chrono::steady_clock::now();
			}

			if (is_clicking)
			{
				if (settings::color_detect::click_mode == 0) // Continuous
				{
					auto now = std::chrono::steady_clock::now();
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_click_time).count();

					int target_delay = current_delay;
					if (!settings::color_detect::randomize_delay)
					{
						if (is_first_click)
							target_delay = settings::color_detect::click_delay;
						else
						{
							int cps = settings::color_detect::cps;
							if (cps < 1) cps = 1;
							target_delay = 1000 / cps;
						}
					}
					if (duration >= target_delay)
					{
						trigger_immediate_click();
						last_click_time = now;
						is_first_click = false;
						if (settings::color_detect::randomize_delay)
						{
							int mn = settings::color_detect::min_delay;
							int mx = settings::color_detect::max_delay;
							if (mn > mx) std::swap(mn, mx);
							if (mn < 1) mn = 1; if (mx < 1) mx = 1;
							std::random_device rd;
							std::mt19937 gen(rd());
							std::uniform_int_distribution<> distrib(mn, mx);
							current_delay = distrib(gen);
						}
						else
						{
							int cps = settings::color_detect::cps;
							if (cps < 1) cps = 1;
							current_delay = 1000 / cps;
						}
					}
				}
				else // Single Click
				{
					auto now = std::chrono::steady_clock::now();
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_click_time).count();
					int target_delay = settings::color_detect::randomize_delay ? current_delay : settings::color_detect::click_delay;
					if (duration >= target_delay)
					{
						trigger_immediate_click();
						is_clicking = false;
					}
				}
			}
		}
	}
}
