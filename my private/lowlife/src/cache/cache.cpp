#include "cache.h"
#include <thread>
#include <game/game.h>
#include <unordered_map>
#include <unordered_set>

void cache::run()
{
	static std::unordered_map<std::uint64_t, cache::entity_t> persistent_cache;
	static std::unordered_map<std::uint64_t, std::uint64_t> cached_model_addresses;

	while (true)
	{
		rbx::player_t local_player_obj = { game::local_player.address };
		game::local_character = { local_player_obj.get_model_instance().address };

		std::vector<rbx::player_t> players = game::players.get_children<rbx::player_t>();

		std::vector<cache::entity_t> temp_cache;
		std::unordered_set<std::uint64_t> active_addresses;
		
		for (rbx::player_t& player : players)
		{
			if (player.address == 0) continue;
			active_addresses.insert(player.address);

			auto it = persistent_cache.find(player.address);
			if (it == persistent_cache.end())
			{
				cache::entity_t entity{};

				entity.instance = { player.address };
				entity.name = player.get_name();
				entity.display_name = memory->read_string(player.address + Offsets::Player::DisplayName);
				if (entity.display_name.empty() || entity.display_name == "Unknown")
				{
					entity.display_name = entity.name;
				}
				entity.user_id = player.get_user_id();
				entity.crew_id = player.get_crew_id();

				persistent_cache[player.address] = entity;
				cached_model_addresses[player.address] = 0;
			}

			cache::entity_t& cached_entity = persistent_cache[player.address];
			rbx::model_instance_t model_instance = player.get_model_instance();

			// Re-cache character parts only if model address has changed (e.g. respawn)
			if (model_instance.address != cached_model_addresses[player.address])
			{
				cached_model_addresses[player.address] = model_instance.address;
				cached_entity.parts.clear();
				cached_entity.ko_address = 0;

				if (model_instance.address != 0)
				{
					rbx::instance_t body_effects = model_instance.find_first_child("BodyEffects");
					if (body_effects.address != 0)
					{
						rbx::instance_t ko = body_effects.find_first_child("K.O");
						if (ko.address != 0)
						{
							cached_entity.ko_address = ko.address;
						}
					}

					for (rbx::part_t& part : model_instance.get_children<rbx::part_t>())
					{
						std::string part_class = part.get_class_name();
						if (part_class.find("Part") != std::string::npos)
						{
							cached_entity.parts[part.get_name()] = part;
						}
					}

					cached_entity.humanoid = { model_instance.find_first_child("Humanoid").address };
					cached_entity.rig_type = cached_entity.humanoid.get_rig_type();
				}
				else
				{
					cached_entity.humanoid = { 0 };
					cached_entity.rig_type = 0;
				}
			}

			// Active properties to update every iteration
			cached_entity.tool_name = "";
			if (model_instance.address != 0)
			{
				for (rbx::instance_t& child : model_instance.get_children<rbx::instance_t>())
				{
					std::string child_class = child.get_class_name();
					if (child_class == "Tool" || child_class == "HopperBin")
					{
						cached_entity.tool_name = child.get_name();
						break;
					}
				}

				if (cached_entity.humanoid.address != 0)
				{
					cached_entity.health = cached_entity.humanoid.get_health();
					cached_entity.max_health = cached_entity.humanoid.get_max_health();
				}
				else
				{
					cached_entity.health = 0.0f;
					cached_entity.max_health = 0.0f;
				}

				if (cached_entity.ko_address != 0)
				{
					cached_entity.is_knocked = memory->read<bool>(cached_entity.ko_address + Offsets::Misc::Value);
				}
				else
				{
					cached_entity.is_knocked = (cached_entity.health <= 0.0f);
				}
			}
			else
			{
				cached_entity.health = 0.0f;
				cached_entity.max_health = 0.0f;
				cached_entity.is_knocked = false;
			}

			temp_cache.push_back(cached_entity);
		}

		// Purge players from cache who left the server
		for (auto it_pc = persistent_cache.begin(); it_pc != persistent_cache.end();)
		{
			if (active_addresses.find(it_pc->first) == active_addresses.end())
			{
				cached_model_addresses.erase(it_pc->first);
				it_pc = persistent_cache.erase(it_pc);
			}
			else
			{
				++it_pc;
			}
		}

		{
			std::lock_guard<std::mutex> lock(mtx);
			cached_players = std::make_shared<std::vector<cache::entity_t>>(std::move(temp_cache));
			
			for (cache::entity_t& entity : *cached_players)
			{
				if (entity.instance.address == game::local_player.address)
				{
					cached_local_player = entity;
					break;
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}