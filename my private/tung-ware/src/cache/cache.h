#pragma once
#include <string>
#include <mutex>
#include <unordered_map>
#include <memory>

#include <sdk/sdk.h>

namespace cache
{
	inline std::mutex mtx;

	struct entity_t final
	{
		rbx::instance_t instance;
		std::uint64_t model_address{ 0 };
		std::string name;
		std::string display_name;
		std::string tool_name;
		std::int64_t user_id;
		std::uint64_t ko_address{ 0 };
		std::string crew_id;

		std::uint8_t rig_type;
		
		rbx::humanoid_t humanoid;
		std::unordered_map<std::string, rbx::part_t> parts;
		std::uint64_t head_mesh_address{ 0 };

		
		float health{ 0.0f };
		float max_health{ 0.0f };
		bool is_knocked{ false };
		int ko_check_count{ 0 };
	};

	inline cache::entity_t cached_local_player;
	inline std::shared_ptr<std::vector<cache::entity_t>> cached_players;

	inline std::uint64_t local_player_address{ 0 };
	inline std::int64_t local_player_user_id{ 0 };
	inline std::string local_player_name{ "" };

	bool is_local_player(const cache::entity_t& player);

	void run();
}