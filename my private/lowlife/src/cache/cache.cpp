#include "cache.h"
#include <thread>
#include <game/game.h>
#include <unordered_map>
#include <unordered_set>

static std::string get_equipped_tool_name(std::uint64_t character_address)
{
	if (character_address == 0) return "";

	std::uint64_t start = memory->read<std::uint64_t>(character_address + Offsets::Instance::ChildrenStart);
	if (start == 0) return "";

	std::uint64_t array_start = memory->read<std::uint64_t>(start);
	std::uint64_t array_end = memory->read<std::uint64_t>(start + Offsets::Instance::ChildrenEnd);

	if (array_start == 0 || array_end == 0 || array_start >= array_end)
	{
		return "";
	}

	std::uint64_t size_bytes = array_end - array_start;
	std::uint64_t count = size_bytes / 16; // 16 bytes per shared_ptr

	if (count == 0 || count > 1000)
	{
		return "";
	}

	struct raw_shared_ptr {
		std::uint64_t ptr;
		std::uint64_t ref_count;
	};

	std::vector<raw_shared_ptr> raw_ptrs;
	raw_shared_ptr* ptr_buf = nullptr;
	raw_shared_ptr stack_ptrs[64];
	if (count <= 64) {
		ptr_buf = stack_ptrs;
	} else {
		raw_ptrs.resize(count);
		ptr_buf = raw_ptrs.data();
	}

	Luck_ReadVirtualMemory(memory->get_process_handle(), reinterpret_cast<void*>(array_start), ptr_buf, static_cast<ULONG>(count * 16), nullptr);

	struct msvc_string_layout {
		union {
			char buf[16];
			char* ptr;
		} u;
		size_t size;
		size_t res;
	};

	for (std::uint64_t i = 0; i < count; ++i)
	{
		std::uint64_t child_address = ptr_buf[i].ptr;
		if (child_address == 0) continue;

		// Class descriptor address
		std::uint64_t class_descriptor = memory->read<std::uint64_t>(child_address + Offsets::Instance::ClassDescriptor);
		if (class_descriptor == 0) continue;

		// Class name address
		std::uint64_t class_name_addr = memory->read<std::uint64_t>(class_descriptor + Offsets::Instance::ClassName);
		if (class_name_addr == 0) continue;

		// Read class name string structure
		msvc_string_layout layout{};
		Luck_ReadVirtualMemory(memory->get_process_handle(), reinterpret_cast<void*>(class_name_addr), &layout, sizeof(msvc_string_layout), nullptr);
		if (layout.size == 0 || layout.size > 255) continue;

		char stack_buf[16];
		const char* class_str = nullptr;
		if (layout.size < 16) {
			layout.u.buf[layout.size] = '\0';
			class_str = layout.u.buf;
		} else {
			Luck_ReadVirtualMemory(memory->get_process_handle(), layout.u.ptr, stack_buf, 15, nullptr);
			stack_buf[15] = '\0';
			class_str = stack_buf;
		}

		if (strcmp(class_str, "Tool") == 0 || strcmp(class_str, "HopperBin") == 0)
		{
			std::uint64_t name_ptr = memory->read<std::uint64_t>(child_address + Offsets::Instance::Name);
			if (name_ptr) {
				return memory->read_string(name_ptr);
			}
		}
	}

	return "";
}

void cache::run()
{
	static std::unordered_map<std::uint64_t, cache::entity_t> persistent_cache;
	static std::unordered_map<std::uint64_t, std::uint64_t> cached_model_addresses;
	static std::unordered_map<std::uint64_t, size_t> cached_model_children_counts;
	static std::uint32_t last_pid = 0;

	while (true)
	{
		std::uint32_t current_pid = memory->get_process_id();
		if (current_pid != last_pid)
		{
			persistent_cache.clear();
			cached_model_addresses.clear();
			cached_model_children_counts.clear();
			last_pid = current_pid;
		}

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

			// Re-cache character parts only if model address has changed (e.g. respawn),
			// or if the character parts have finished replicating/loading since the initial cache.
			bool model_changed = (model_instance.address != cached_model_addresses[player.address]);
			bool needs_recache = model_changed;

			if (!needs_recache && model_instance.address != 0)
			{
				// Only re-cache if we are missing essential components AND new components have actually loaded
				if (cached_entity.humanoid.address == 0 || cached_entity.parts.size() < 6)
				{
					size_t current_child_count = model_instance.get_children().size();
					if (current_child_count > cached_model_children_counts[player.address])
					{
						needs_recache = true;
					}
					else if (cached_entity.humanoid.address == 0)
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
				cached_entity.parts.clear();
				cached_entity.ko_address = 0;
				cached_entity.ko_check_count = 0;

				if (model_instance.address != 0)
				{
					cached_model_children_counts[player.address] = model_instance.get_children().size();

					rbx::instance_t body_effects = model_instance.find_first_child("BodyEffects");
					if (body_effects.address != 0)
					{
						rbx::instance_t ko = body_effects.find_first_child("K.O");
						if (ko.address == 0) {
							ko = body_effects.find_first_child("KO");
						}
						if (ko.address == 0) {
							ko = body_effects.find_first_child("Dead");
						}
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
					cached_model_children_counts[player.address] = 0;
					cached_entity.humanoid = { 0 };
					cached_entity.rig_type = 0;
				}
			}

			// Active properties to update every iteration
			cached_entity.tool_name = "";
			if (model_instance.address != 0)
			{
				cached_entity.tool_name = get_equipped_tool_name(model_instance.address);

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

				if (cached_entity.ko_address == 0 && cached_entity.ko_check_count < 15)
				{
					cached_entity.ko_check_count++;
					rbx::instance_t body_effects = model_instance.find_first_child("BodyEffects");
					if (body_effects.address != 0)
					{
						rbx::instance_t ko = body_effects.find_first_child("K.O");
						if (ko.address == 0) {
							ko = body_effects.find_first_child("KO");
						}
						if (ko.address == 0) {
							ko = body_effects.find_first_child("Dead");
						}
						if (ko.address != 0)
						{
							cached_entity.ko_address = ko.address;
						}
					}
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
				if (entity.instance.address == game::local_player.address)
				{
					cached_local_player = entity;
					break;
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(30));
	}
}