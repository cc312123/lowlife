#include "freezeplayer.h"

#include <memory/memory.h>
#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <game/game.h>
#include <settings.h>
#include <check/typing_check.h>
#include <cache/cache.h>
#include <Windows.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <unordered_map>
#include <mutex>

namespace freezeplayer
{
	static bool playerfreeze_thread_running = true;
	static bool waSEN2 = false;

	struct frozen_part_t
	{
		math::vector3 position;
		math::matrix3 rotation;
	};

	struct frozen_player_t
	{
		std::unordered_map<std::string, frozen_part_t> parts;
	};

	static std::unordered_map<std::uint64_t, frozen_player_t> frozen_players;

	void unfreeze_all_players()
	{
		
		
		
		frozen_players.clear();
	}

	static std::chrono::steady_clock::time_point last_lag_update = std::chrono::steady_clock::now();
	static bool is_lag_updating = false;
	static std::chrono::steady_clock::time_point lag_update_start = std::chrono::steady_clock::now();
	static constexpr int lag_interval_ms = 600; 
	static constexpr int lag_release_duration_ms = 50; 

	void freezeplayer_loop()
	{
		while (playerfreeze_thread_running)
		{
			bool key_pressed = false;
			static bool key_was_pressed = false;
			static bool toggle_state = false;
			static bool was_disabled_by_typing = false;

			if (check::textchatopen)
			{
				was_disabled_by_typing = true;
				toggle_state = false;
				if (waSEN2)
				{
					waSEN2 = false;
					unfreeze_all_players();
					is_lag_updating = false;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				continue;
			}

			if (was_disabled_by_typing && !check::textchatopen)
			{
				toggle_state = false;
				was_disabled_by_typing = false;
			}

			if (settings::expl::freeze_players_keybind_mode == 0)
			{
				key_pressed = GetAsyncKeyState(settings::expl::freeze_players_keybind) & 0x8000;
			}
			else if (settings::expl::freeze_players_keybind_mode == 1)
			{
				bool current_key_state = GetAsyncKeyState(settings::expl::freeze_players_keybind) & 0x8000;
				if (current_key_state && !key_was_pressed)
				{
					toggle_state = !toggle_state;
					key_was_pressed = true;
				}
				else if (!current_key_state)
				{
					key_was_pressed = false;
				}
				key_pressed = toggle_state;
			}
			else if (settings::expl::freeze_players_keybind_mode == 2)
			{
				key_pressed = true;
			}

			if (settings::expl::freeze_players && key_pressed)
			{
				waSEN2 = true;

				
				std::shared_ptr<std::vector<cache::entity_t>> players_snapshot;
				{
					std::lock_guard<std::mutex> lock(cache::mtx);
					players_snapshot = cache::cached_players;
				}

				if (players_snapshot)
				{
					for (auto& player : *players_snapshot)
					{
						if (cache::is_local_player(player))
							continue;

						
						auto it = frozen_players.find(player.instance.address);
						if (it == frozen_players.end())
						{
							frozen_player_t fp;
							for (auto& [part_name, part] : player.parts)
							{
								if (part.address != 0)
								{
									rbx::primitive_t prim = part.get_primitive();
									if (prim.address != 0)
									{
										frozen_part_t fpart;
										fpart.position = prim.get_position();
										fpart.rotation = prim.get_rotation();
										fp.parts[part_name] = fpart;
									}
								}
							}
							if (!fp.parts.empty())
							{
								frozen_players[player.instance.address] = fp;
							}
						}

						
						it = frozen_players.find(player.instance.address);
						if (it != frozen_players.end())
						{
							for (auto& [part_name, part] : player.parts)
							{
								if (part.address != 0)
								{
									rbx::primitive_t prim = part.get_primitive();
									if (prim.address != 0)
									{
										auto part_it = it->second.parts.find(part_name);
										if (part_it != it->second.parts.end())
										{
											
											memory->write<math::vector3>(prim.address + Offsets::Primitive::Position, part_it->second.position);
											memory->write<math::matrix3>(prim.address + Offsets::Primitive::Rotation, part_it->second.rotation);
										}
									}
								}
							}
						}
					}
				}

				
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			else
			{
				if (waSEN2)
				{
					waSEN2 = false;
					unfreeze_all_players();
					is_lag_updating = false;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
		}
	}

	void run()
	{
		playerfreeze_thread_running = true;
		freezeplayer_loop();
	}
}

