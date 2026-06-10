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
#include "shot_detection.h"

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
		
		if (player.instance.address != 0) {
			try {
				rbx::player_t player_instance(player.instance.address);
				rbx::model_instance_t model_instance = player_instance.get_model_instance();
				if (model_instance.address != 0) {
					static const std::vector<std::string> ko_names = { "K.O", "KO", "Knocked", "Downed", "Dead" };
					
					rbx::instance_t body_effects = model_instance.find_first_child("BodyEffects");
					if (body_effects.address != 0) {
						for (const auto& name : ko_names) {
							rbx::instance_t ko = body_effects.find_first_child(name);
							if (ko.address != 0) {
								if (memory->read<bool>(ko.address + Offsets::Misc::Value)) {
									return true;
								}
							}
						}
					}
					
					for (const auto& name : ko_names) {
						rbx::instance_t ko = model_instance.find_first_child(name);
						if (ko.address != 0) {
							if (memory->read<bool>(ko.address + Offsets::Misc::Value)) {
								return true;
							}
						}
					}
				}
			} catch (...) {}
		}
		return false;
	}

	inline std::atomic<bool> combo_running{ false };
}

namespace shot_detection
{
	namespace
	{
		bool select_key_was_pressed = false;
		int last_ammo_value = -1;
		std::uint64_t last_tool_address = 0;
		std::uint64_t last_ammo_val_address = 0;
		std::uint64_t last_target_model_address = 0;

		rbx::instance_t find_ammo_val_recursive(rbx::instance_t parent, const std::string& target_name, int depth = 0) {
			if (depth > 6 || parent.address == 0) return {};

			std::vector<rbx::instance_t> children;
			try {
				children = parent.get_children();
			} catch (...) {
				return {};
			}

			for (rbx::instance_t& child : children) {
				if (child.address == 0) continue;
				std::string cclass = child.get_class_name();
				if (cclass.find("Value") != std::string::npos) {
					std::string cname = child.get_name();
					std::string lower_cname = cname;
					std::transform(lower_cname.begin(), lower_cname.end(), lower_cname.begin(), ::tolower);

					std::string lower_target = target_name;
					std::transform(lower_target.begin(), lower_target.end(), lower_target.begin(), ::tolower);

					if (!lower_target.empty() && (lower_cname == lower_target || cname == target_name)) {
						return child;
					}
				}
			}

			for (rbx::instance_t& child : children) {
				if (child.address == 0) continue;
				std::string cclass = child.get_class_name();
				if (cclass.find("Value") != std::string::npos) {
					std::string cname = child.get_name();
					std::string lower_cname = cname;
					std::transform(lower_cname.begin(), lower_cname.end(), lower_cname.begin(), ::tolower);

					if (lower_cname.find("ammo") != std::string::npos || lower_cname.find("clip") != std::string::npos) {
						return child;
					}
				}
			}

			for (rbx::instance_t& child : children) {
				if (child.address == 0) continue;
				std::string cclass = child.get_class_name();
				if (cclass == "Part" || cclass == "MeshPart" || cclass == "Accessory" || cclass == "Humanoid" || 
					cclass == "WedgePart" || cclass == "UnionOperation" || cclass == "SpecialMesh" || 
					cclass == "Decal" || cclass == "Texture" || cclass == "TouchTransmitter") {
					continue;
				}

				rbx::instance_t found = find_ammo_val_recursive(child, target_name, depth + 1);
				if (found.address != 0) {
					return found;
				}
			}

			return {};
		}

		enum ComboState {
			COMBO_IDLE,
			COMBO_EQUIP_DB,
			COMBO_SHOOT_DB,
			COMBO_SWITCH_TO_REV,
			COMBO_SHOOT_REV,
			COMBO_FINISHED
		};

		ComboState combo_state = COMBO_IDLE;
		int db_ammo_start = -1;
		int rev_ammo_start = -1;
		int rev_shots_fired = 0;
		std::chrono::steady_clock::time_point last_click_time = std::chrono::steady_clock::now();

		bool is_roblox_active() {
			HWND roblox_wnd = FindWindowA(nullptr, "Roblox");
			return roblox_wnd && (GetForegroundWindow() == roblox_wnd);
		}

		bool str_contains_case_insensitive(const std::string& str, const std::string& target) {
			if (str.empty() || target.empty()) return false;
			std::string s_lower = str;
			std::string t_lower = target;
			std::transform(s_lower.begin(), s_lower.end(), s_lower.begin(), ::tolower);
			std::transform(t_lower.begin(), t_lower.end(), t_lower.begin(), ::tolower);
			return s_lower.find(t_lower) != std::string::npos;
		}

		void simulate_keypress(WORD key) {
			INPUT input = {};
			WORD scan = static_cast<WORD>(MapVirtualKeyA(key, MAPVK_VK_TO_VSC));
			
			input.type = INPUT_KEYBOARD;
			input.ki.wVk = 0;
			input.ki.wScan = scan;
			input.ki.dwFlags = KEYEVENTF_SCANCODE;
			SendInput(1, &input, sizeof(INPUT));
			
			Sleep(5); 
			
			input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
			SendInput(1, &input, sizeof(INPUT));
		}

		void trigger_single_click() {
			INPUT input = {};
			input.type = INPUT_MOUSE;
			input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
			SendInput(1, &input, sizeof(INPUT));
			
			Sleep(15); 
			
			input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
			SendInput(1, &input, sizeof(INPUT));
		}

		bool get_local_tool_info(std::string& name_out, int& ammo_out, rbx::instance_t& tool_instance_out) {
			if (!game::local_player.address) return false;
			
			rbx::player_t lp{ game::local_player.address };
			rbx::model_instance_t lp_char = lp.get_model_instance();
			if (lp_char.address == 0) return false;
			
			rbx::instance_t tool_instance = {};
			for (rbx::instance_t& child : lp_char.get_children<rbx::instance_t>()) {
				std::string child_class = child.get_class_name();
				if (child_class == "Tool" || child_class == "HopperBin") {
					tool_instance = child;
					break;
				}
			}
			
			if (tool_instance.address == 0) return false;
			
			tool_instance_out = tool_instance;
			name_out = tool_instance.get_name();
			
			rbx::instance_t ammo_val_obj = {};
			for (rbx::instance_t& child : tool_instance.get_children<rbx::instance_t>()) {
				std::string cname = child.get_name();
				std::string cclass = child.get_class_name();
				if (cclass.find("Value") != std::string::npos) {
					std::string target_ammo_name = settings::shot_detection::ammo_name;
					std::string lower_cname = cname;
					std::string lower_target_ammo = target_ammo_name;
					std::transform(lower_cname.begin(), lower_cname.end(), lower_cname.begin(), ::tolower);
					std::transform(lower_target_ammo.begin(), lower_target_ammo.end(), lower_target_ammo.begin(), ::tolower);
					if (lower_cname == lower_target_ammo || cname == target_ammo_name) {
						ammo_val_obj = child;
						break;
					}
				}
			}
			
			if (ammo_val_obj.address == 0) {
				for (rbx::instance_t& child : tool_instance.get_children<rbx::instance_t>()) {
					std::string cname = child.get_name();
					std::string cclass = child.get_class_name();
					std::transform(cname.begin(), cname.end(), cname.begin(), ::tolower);
					if (cclass.find("Value") != std::string::npos && 
						(cname.find("ammo") != std::string::npos || cname.find("clip") != std::string::npos)) {
						ammo_val_obj = child;
						break;
					}
				}
			}
			
			if (ammo_val_obj.address != 0) {
				ammo_out = memory->read<int>(ammo_val_obj.address + Offsets::Misc::Value);
				return true;
			}
			
			ammo_out = -1;
			return true;
		}

		float vector2_distance(float ax, float ay, float bx, float by) {
			float dx = ax - bx;
			float dy = ay - by;
			return std::sqrt(dx * dx + dy * dy);
		}
		int get_random_delay() {
			int min_d = settings::shot_detection::min_delay;
			int max_d = settings::shot_detection::max_delay;
			if (min_d > max_d) std::swap(min_d, max_d);
			if (min_d == max_d) return min_d;
			
			static thread_local std::mt19937 generator(std::random_device{}());
			std::uniform_int_distribution<int> distribution(min_d, max_d);
			return distribution(generator);
		}

		void simulate_click(int delay_ms) {
			if (delay_ms > 0) {
				Sleep(delay_ms);
			}

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

	void run()
	{
		static int last_local_ammo = -1;
		static std::uint64_t last_local_tool = 0;
		while (true)
		{
			
			Sleep(2);

			if (!settings::shot_detection::enabled || !game::workspace.address) {
				settings::shot_detection::target_address = 0;
				settings::shot_detection::target_name = "None";
				last_ammo_value = -1;
				last_tool_address = 0;
				continue;
			}

			
			if (settings::shot_detection::db_revolver_combo) {
				if (!combo_running) {
					if (is_roblox_active() && !check::textchatopen) {
						std::string local_eq_name = "";
						int local_eq_ammo = -1;
						rbx::instance_t local_eq_tool = {};
						if (get_local_tool_info(local_eq_name, local_eq_ammo, local_eq_tool)) {
							if (local_eq_tool.address != last_local_tool) {
								last_local_tool = local_eq_tool.address;
								last_local_ammo = local_eq_ammo;
							} else {
								bool is_db = (str_contains_case_insensitive(local_eq_name, "double") || str_contains_case_insensitive(local_eq_name, "db") || str_contains_case_insensitive(local_eq_name, "barrel"));
								if (is_db) {
									if (last_local_ammo != -1 && local_eq_ammo < last_local_ammo && local_eq_ammo >= 0 && (last_local_ammo - local_eq_ammo) <= 10) {
										if (!combo_running.exchange(true)) {
											std::thread([]() {
												if (settings::shot_detection::switch_delay > 0) {
													Sleep(settings::shot_detection::switch_delay);
												}

												WORD rev_key = settings::shot_detection::revolver_slot_key;
												WORD scan = static_cast<WORD>(MapVirtualKeyA(rev_key, MAPVK_VK_TO_VSC));
												
												INPUT inputs[2] = {};
												inputs[0].type = INPUT_KEYBOARD;
												inputs[0].ki.wScan = scan;
												inputs[0].ki.dwFlags = KEYEVENTF_SCANCODE;

												inputs[1].type = INPUT_KEYBOARD;
												inputs[1].ki.wScan = scan;
												inputs[1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;

												SendInput(2, inputs, sizeof(INPUT));
												Sleep(200);

												// Auto-shoot the revolver while left mouse button is held down
												while (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
													if (!check::textchatopen && is_roblox_active()) {
														trigger_single_click();
													}
													int sleep_time = 1000 / settings::shot_detection::combo_cps;
													if (sleep_time < 1) sleep_time = 1;
													Sleep(sleep_time);
												}
												combo_running = false;
											}).detach();
										}
									}
								}
							}
							last_local_ammo = local_eq_ammo;
						} else {
							last_local_tool = 0;
							last_local_ammo = -1;
						}
					} else {
						last_local_tool = 0;
						last_local_ammo = -1;
					}
				}
			} else {
				last_local_tool = 0;
				last_local_ammo = -1;
			}

			
			bool select_pressed = GetAsyncKeyState(settings::shot_detection::select_key) & 0x8000;
			if (select_pressed && !select_key_was_pressed) {
				POINT cursor_pt = {};
				if (GetCursorPos(&cursor_pt)) {
					HWND roblox_wnd = FindWindowA(nullptr, "Roblox");
					if (roblox_wnd) {
						
						cache::entity_t best_player = {};
						float best_dist = std::numeric_limits<float>::max();

						std::shared_ptr<std::vector<cache::entity_t>> players_snapshot;
						{
							std::lock_guard<std::mutex> lock(cache::mtx);
							players_snapshot = cache::cached_players;
						}

						if (players_snapshot) {
							for (const auto& player : *players_snapshot) {
								if (player.instance.address == 0 || 
									player.instance.address == cache::cached_local_player.instance.address ||
									player.instance.address == game::local_player.address ||
									(player.name == cache::cached_local_player.name && !player.name.empty()))
									continue;

								if (game::local_character.address != 0 && player.model_address != 0 &&
									player.model_address == game::local_character.address)
									continue;

								if (settings::shot_detection::knocked_check && is_player_knocked(player))
									continue;

								
								rbx::part_t target_part = {};
								if (auto it = player.parts.find("Head"); it != player.parts.end()) {
									target_part = it->second;
								} else if (auto it = player.parts.find("HumanoidRootPart"); it != player.parts.end()) {
									target_part = it->second;
								}

								if (!target_part.address) continue;

								rbx::primitive_t primitive = target_part.get_primitive();
								math::vector3 world_pos = primitive.get_position();
								math::vector2 screen_pos = {};

								if (game::visengine.world_to_screen(world_pos, screen_pos, 
									game::visengine.get_dimensions(), game::visengine.get_viewmatrix())) {
									
									float dist = vector2_distance(screen_pos.x, screen_pos.y, (float)cursor_pt.x, (float)cursor_pt.y);
									if (dist < settings::shot_detection::select_hitbox && dist < best_dist) {
										best_dist = dist;
										best_player = player;
									}
								}
							}
						}

						if (best_player.instance.address != 0) {
							settings::shot_detection::target_address = best_player.instance.address;
							settings::shot_detection::target_name = best_player.name;
							last_ammo_value = -1;
							last_tool_address = 0;

							char notification_buf[256];
							sprintf_s(notification_buf, "Target Locked: %s", best_player.name.c_str());
							notifications::add(notification_buf, notifications::NotificationType::Success, 3.0f);
						}
					}
				}
			}
			select_key_was_pressed = select_pressed;

						
			if (settings::shot_detection::target_address != 0) {
				
				cache::entity_t target_entity = {};
				bool found_in_cache = false;
				std::shared_ptr<std::vector<cache::entity_t>> players_snapshot;
				{
					std::lock_guard<std::mutex> lock(cache::mtx);
					players_snapshot = cache::cached_players;
				}

				if (players_snapshot) {
					for (const auto& p : *players_snapshot) {
						if (p.instance.address == settings::shot_detection::target_address) {
							target_entity = p;
							found_in_cache = true;
							break;
						}
					}
				}

				if (!found_in_cache || (settings::shot_detection::knocked_check && is_player_knocked(target_entity))) {
					
					settings::shot_detection::target_address = 0;
					settings::shot_detection::target_name = "None";
					last_ammo_value = -1;
					last_tool_address = 0;
					last_ammo_val_address = 0;
					last_target_model_address = 0;
					continue;
				}

				
				rbx::player_t target_player_obj{ target_entity.instance.address };
				rbx::model_instance_t model = target_player_obj.get_model_instance();
				if (model.address == 0) {
					last_ammo_value = -1;
					last_tool_address = 0;
					last_ammo_val_address = 0;
					last_target_model_address = 0;
					continue;
				}

				if (model.address != last_target_model_address) {
					last_target_model_address = model.address;
					last_ammo_val_address = 0;
					last_ammo_value = -1;
				}

				static auto last_fail_scan_time = std::chrono::steady_clock::now();
				auto now_time = std::chrono::steady_clock::now();

				std::uint64_t current_ammo_val_address = 0;
				if (last_ammo_val_address != 0) {
					try {
						std::uint64_t parent = memory->read<std::uint64_t>(last_ammo_val_address + Offsets::Instance::Parent);
						if (parent != 0) {
							current_ammo_val_address = last_ammo_val_address;
						}
					} catch (...) {
						current_ammo_val_address = 0;
					}
				}

				if (current_ammo_val_address == 0) {
					if (std::chrono::duration_cast<std::chrono::milliseconds>(now_time - last_fail_scan_time).count() >= 250) {
						last_fail_scan_time = now_time;
						rbx::instance_t ammo_val_obj = find_ammo_val_recursive(model, settings::shot_detection::ammo_name);

						current_ammo_val_address = ammo_val_obj.address;
						last_ammo_val_address = current_ammo_val_address;
						last_ammo_value = -1;
					}
				}

				if (last_ammo_val_address != 0) {
					int current_ammo = memory->read<int>(last_ammo_val_address + Offsets::Misc::Value);

					if (last_ammo_value != -1) {
						if (current_ammo < last_ammo_value && current_ammo >= 0 && (last_ammo_value - current_ammo) <= 10) {
							bool key_ok = (settings::shot_detection::trigger_key == 0) || 
							              (GetAsyncKeyState(settings::shot_detection::trigger_key) & 0x8000);
							if (key_ok) {
								if (!check::textchatopen && is_roblox_active()) {
									if (settings::shot_detection::db_revolver_combo) {
										if (!combo_running.exchange(true)) {
											std::thread([]() {
												int delay_ms = get_random_delay();
												if (delay_ms > 0) {
													Sleep(delay_ms);
												}

												std::string eq_name = "";
												int eq_ammo = -1;
												rbx::instance_t eq_tool = {};
												bool has_tool = get_local_tool_info(eq_name, eq_ammo, eq_tool);

												bool has_db = has_tool && (str_contains_case_insensitive(eq_name, "double") || str_contains_case_insensitive(eq_name, "db") || str_contains_case_insensitive(eq_name, "barrel"));
												if (!has_db && settings::shot_detection::auto_switch_on_start) {
													simulate_keypress(settings::shot_detection::db_slot_key);
													Sleep(200); 
													has_tool = get_local_tool_info(eq_name, eq_ammo, eq_tool);
													has_db = has_tool && (str_contains_case_insensitive(eq_name, "double") || str_contains_case_insensitive(eq_name, "db") || str_contains_case_insensitive(eq_name, "barrel"));
												}

												if (has_db) {
													INPUT click_down = {};
													click_down.type = INPUT_MOUSE;
													click_down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
													SendInput(1, &click_down, sizeof(INPUT));

													Sleep(15); 

													INPUT click_up = {};
													click_up.type = INPUT_MOUSE;
													click_up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
													SendInput(1, &click_up, sizeof(INPUT));

													if (settings::shot_detection::switch_delay > 0) {
														Sleep(settings::shot_detection::switch_delay);
													}

													WORD rev_key = settings::shot_detection::revolver_slot_key;
													WORD scan = static_cast<WORD>(MapVirtualKeyA(rev_key, MAPVK_VK_TO_VSC));
													INPUT key_down = {};
													key_down.type = INPUT_KEYBOARD;
													key_down.ki.wScan = scan;
													key_down.ki.dwFlags = KEYEVENTF_SCANCODE;
													SendInput(1, &key_down, sizeof(INPUT));

													Sleep(5);

													INPUT key_up = {};
													key_up.type = INPUT_KEYBOARD;
													key_up.ki.wScan = scan;
													key_up.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
													SendInput(1, &key_up, sizeof(INPUT));
													
													Sleep(10);
												}

												bool is_always_on = (settings::shot_detection::trigger_key == 0);
												int shot_count = 0;
												while ((is_always_on && shot_count < 4) || (!is_always_on && (GetAsyncKeyState(settings::shot_detection::trigger_key) & 0x8000))) {
													if (!check::textchatopen && is_roblox_active()) {
														trigger_single_click();
														shot_count++;
													}
													int sleep_time = 1000 / settings::shot_detection::combo_cps;
													if (sleep_time < 1) sleep_time = 1;
													Sleep(sleep_time);
												}

												combo_running = false;
											}).detach();
										}
									} else {
										std::thread([]() {
											simulate_click(get_random_delay());
										}).detach();
									}
								}
							}
						}
					}
					last_ammo_value = current_ammo;
				} else {
					last_ammo_value = -1;
				}
			}
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
		};

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
			if (is_point_inside_obb(start, box) || is_point_inside_obb(end, box))
			{
				continue;
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
			
			float dot = wx * vx + wy * vy + wz * vz;
			float t = dot / v_sq;
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
			if (ray_intersects_obb(start, dir_norm, len, box, dist))
			{
				return true; 
			}
		}

		return false;
	}

	namespace
	{
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
