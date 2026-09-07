#include "cache.h"
#include <thread>
#include <game/game.h>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <settings.h>


static std::string get_equipped_tool_name(std::uint64_t character_address)
{
	if (character_address == 0) return "";

	rbx::instance_t model(character_address);
	for (rbx::instance_t& child : model.get_children())
	{
		std::string class_name = child.get_class_name();
		if (class_name == "Tool" || class_name == "HopperBin")
		{
			return child.get_name();
		}
	}

	return "";
}

static bool ResolveModelInstanceOffset(std::uint64_t local_player_addr)
{
	if (!local_player_addr) return false;

	rbx::player_t lp_obj{ local_player_addr };
	std::string lp_name = lp_obj.get_name();
	if (lp_name.empty() || lp_name == "unknown" || lp_name == "Unknown") return false;

	for (std::uint64_t offset = 0x100; offset <= 0x600; offset += 8)
	{
		std::uint64_t potential_char = memory->read<std::uint64_t>(local_player_addr + offset);
		if (potential_char != 0 && (potential_char & 0x7) == 0 && potential_char > 0x10000)
		{
			rbx::nameable_t inst{ potential_char };
			std::string name = inst.get_name();
			std::string class_name = inst.get_class_name();
			if (class_name == "Model" && name == lp_name)
			{
				Offsets::Player::ModelInstance = offset;
				return true;
			}
		}
	}
	return false;
}

void cache::run()
{
	static std::unordered_map<std::uint64_t, cache::entity_t> persistent_cache;
	static std::unordered_map<std::uint64_t, std::uint64_t> cached_model_addresses;
	static std::unordered_map<std::uint64_t, size_t> cached_model_children_counts;
	static std::uint32_t last_pid = 0;
	static std::uint64_t tick_count = 0;

	while (true)
	{
		if (!globals::roblox_valid)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		tick_count++;
		std::uint32_t current_pid = memory->get_process_id();
		if (current_pid != last_pid)
		{
			persistent_cache.clear();
			cached_model_addresses.clear();
			cached_model_children_counts.clear();
			local_player_address = 0;
			local_player_user_id = 0;
			local_player_name = "";
			last_pid = current_pid;
		}

		rbx::player_t local_player_obj = { game::local_player.address };
		if (local_player_obj.address != 0)
		{
			local_player_address = local_player_obj.address;
			if (local_player_user_id == 0)
			{
				local_player_user_id = local_player_obj.get_user_id();
			}
			if (local_player_name.empty() || local_player_name == "unknown")
			{
				local_player_name = local_player_obj.get_name();
			}

			static std::uint64_t last_resolved_lp = 0;
			static bool resolved = false;
			if (local_player_obj.address != last_resolved_lp)
			{
				last_resolved_lp = local_player_obj.address;
				resolved = false;
			}
			if (!resolved)
			{
				resolved = ResolveModelInstanceOffset(local_player_obj.address);
			}
			game::local_character = { local_player_obj.get_model_instance().address };
		}
		else
		{
			game::local_character = { 0 };
		}

		static std::vector<rbx::player_t> players;
		static size_t last_players_count = 0;
		static std::uint32_t last_players_pid = 0;

		if (current_pid != last_players_pid)
		{
			players.clear();
			last_players_count = 0;
			last_players_pid = current_pid;
		}

		if (tick_count % 30 == 0 || players.empty())
		{
			size_t current_players_count = game::players.get_children_count();
			if (current_players_count != last_players_count || players.empty())
			{
				players = game::players.get_children<rbx::player_t>();
				last_players_count = current_players_count;
			}
		}

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
				entity.model_address = 0;
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

			// Re-cache character parts only if model address has changed (e.g. respawn),
			// or if the character parts have finished replicating/loading since the initial cache.
			bool model_changed = (model_instance.address != cached_model_addresses[player.address]);
			bool needs_recache = model_changed;

			if (!needs_recache && model_instance.address != 0)
			{
				size_t current_child_count = model_instance.get_children_count();
				if (current_child_count != cached_model_children_counts[player.address])
				{
					needs_recache = true;
				}
				else if (cached_entity.humanoid.address == 0)
				{
					if ((tick_count + (player.address >> 3)) % 15 == 0)
					{
						rbx::instance_t temp_humanoid = model_instance.find_first_child("Humanoid");
						if (temp_humanoid.address != 0)
						{
							needs_recache = true;
						}
					}
				}
			}

			if (needs_recache)
			{
				cached_model_addresses[player.address] = model_instance.address;
				cached_entity.model_address = model_instance.address;
				cached_entity.parts.clear();
				cached_entity.ko_address = 0;
				cached_entity.ko_check_count = 0;

				if (model_instance.address != 0)
				{
					cached_model_children_counts[player.address] = model_instance.get_children_count();

					rbx::instance_t body_effects = model_instance.find_first_child("BodyEffects");
					if (body_effects.address != 0)
					{
						rbx::instance_t ko = body_effects.find_first_child("K.O");
						if (ko.address == 0) ko = body_effects.find_first_child("KO");
						if (ko.address == 0) ko = body_effects.find_first_child("Dead");
						if (ko.address != 0)
						{
							cached_entity.ko_address = ko.address;
						}
					}

					if (cached_entity.ko_address == 0)
					{
						rbx::instance_t ko = model_instance.find_first_child("K.O");
						if (ko.address == 0) ko = model_instance.find_first_child("KO");
						if (ko.address == 0) ko = model_instance.find_first_child("Knocked");
						if (ko.address == 0) ko = model_instance.find_first_child("Downed");
						if (ko.address == 0) ko = model_instance.find_first_child("Dead");
						if (ko.address != 0)
						{
							cached_entity.ko_address = ko.address;
						}
					}

					for (rbx::part_t& part : model_instance.get_children<rbx::part_t>())
					{
						std::string part_class = part.get_class_name();
						if (part_class.find("Part") != std::string::npos || part_class.find("Mesh") != std::string::npos || part_class.find("Base") != std::string::npos)
						{
							std::string p_name = part.get_name();
							if (!p_name.empty() && p_name != "unknown")
							{
								cached_entity.parts[p_name] = part;
							}
						}
					}

					auto head_it = cached_entity.parts.find("Head");
					if (head_it != cached_entity.parts.end() && head_it->second.address != 0)
					{
						rbx::instance_t mesh = head_it->second.find_first_child_by_class("SpecialMesh");
						cached_entity.head_mesh_address = mesh.address;
					}
					else
					{
						cached_entity.head_mesh_address = 0;
					}

					rbx::instance_t hum_inst = model_instance.find_first_child("Humanoid");
					if (hum_inst.address == 0) hum_inst = model_instance.find_first_child_by_class("Humanoid");
					cached_entity.humanoid = { hum_inst.address };
					cached_entity.rig_type = cached_entity.humanoid.get_rig_type();
				}
				else
				{
					cached_model_children_counts[player.address] = 0;
					cached_entity.humanoid = { 0 };
					cached_entity.rig_type = 0;
				}
			}

			// Active properties to update every iteration
			if (model_instance.address != 0)
			{
				if (player.address == game::local_player.address)
				{
					if (tick_count % 5 == 0 || cached_entity.tool_name.empty())
					{
						cached_entity.tool_name = get_equipped_tool_name(model_instance.address);
					}
				}
				else if (settings::visuals::tool)
				{
					if ((tick_count + (player.address >> 3)) % 15 == 0 || cached_entity.tool_name.empty())
					{
						cached_entity.tool_name = get_equipped_tool_name(model_instance.address);
					}
				}
				else
				{
					cached_entity.tool_name = "";
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

				bool perform_ko_check = false;
				if (cached_entity.ko_address == 0)
				{
					if (cached_entity.ko_check_count < 15)
					{
						cached_entity.ko_check_count++;
						perform_ko_check = true;
					}
					else if ((tick_count + (player.address >> 3)) % 100 == 0)
					{
						perform_ko_check = true;
					}
				}

				if (perform_ko_check)
				{
					rbx::instance_t body_effects = model_instance.find_first_child("BodyEffects");
					if (body_effects.address != 0)
					{
						rbx::instance_t ko = body_effects.find_first_child("K.O");
						if (ko.address == 0) ko = body_effects.find_first_child("KO");
						if (ko.address == 0) ko = body_effects.find_first_child("Dead");
						if (ko.address != 0)
						{
							cached_entity.ko_address = ko.address;
						}
					}

					if (cached_entity.ko_address == 0)
					{
						rbx::instance_t ko = model_instance.find_first_child("K.O");
						if (ko.address == 0) ko = model_instance.find_first_child("KO");
						if (ko.address == 0) ko = model_instance.find_first_child("Knocked");
						if (ko.address == 0) ko = model_instance.find_first_child("Downed");
						if (ko.address == 0) ko = model_instance.find_first_child("Dead");
						if (ko.address != 0)
						{
							cached_entity.ko_address = ko.address;
						}
					}
				}

				bool platform_stand = false;
				bool is_sitting = false;
				if (cached_entity.humanoid.address != 0)
				{
					platform_stand = memory->read<bool>(cached_entity.humanoid.address + Offsets::Humanoid::PlatformStand);
					is_sitting = memory->read<bool>(cached_entity.humanoid.address + Offsets::Humanoid::Sit);
				}

				if (cached_entity.ko_address != 0)
				{
					cached_entity.is_knocked = memory->read<bool>(cached_entity.ko_address + Offsets::Misc::Value) || (platform_stand && !is_sitting);
				}
				else
				{
					cached_entity.is_knocked = (cached_entity.health <= 20.0f && platform_stand && !is_sitting) || (cached_entity.health <= 0.0f);
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
				cached_model_children_counts.erase(it_pc->first);
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
				if (entity.instance.address == game::local_player.address ||
					(local_player_address != 0 && entity.instance.address == local_player_address) ||
					(local_player_user_id != 0 && entity.user_id == local_player_user_id) ||
					(!local_player_name.empty() && local_player_name != "unknown" && entity.name == local_player_name))
				{
					cached_local_player = entity;
					break;
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(30));
	}
}

bool cache::is_local_player(const cache::entity_t& player)
{
	if (player.instance.address == 0) return false;
	
	if (player.instance.address == game::local_player.address) return true;
	if (local_player_address != 0 && player.instance.address == local_player_address) return true;
	
	if (local_player_user_id != 0 && player.user_id == local_player_user_id) return true;
	
	if (!local_player_name.empty() && local_player_name != "unknown" && player.name == local_player_name) return true;
	
	if (game::local_character.address != 0 && player.model_address != 0 && player.model_address == game::local_character.address) return true;
	
	return false;
}