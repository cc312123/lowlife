#define NOMINMAX
#include <Windows.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <unordered_map>

#include <memory/memory.h>
#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <cache/cache.h>
#include <game/game.h>
#include <settings.h>
#include <check/typing_check.h>
#include "hitbox_expander.h"

namespace rbx::hitbox_expander
{
	namespace
	{
		std::unordered_map<std::uint64_t, math::vector3> original_head_scales;
		std::unordered_map<std::uint64_t, float> original_transparencies;

		void restore_transparency(rbx::part_t& part)
		{
			if (!part.address) return;
			auto it = original_transparencies.find(part.address);
			if (it != original_transparencies.end())
			{
				memory->write<float>(part.address + Offsets::BasePart::Transparency, it->second);
				original_transparencies.erase(it);
			}
		}

		void reset_all_parts(cache::entity_t& player)
		{
			if (!player.instance.address) return;

			// Restore EvaluateStateMachine to true
			if (player.humanoid.address)
			{
				memory->write<bool>(player.humanoid.address + Offsets::Humanoid::EvaluateStateMachine, true);
			}

			// Reset Head
			auto head_it = player.parts.find("Head");
			if (head_it != player.parts.end() && head_it->second.address)
			{
				restore_transparency(head_it->second);
				rbx::primitive_t prim = head_it->second.get_primitive();
				if (prim.address)
				{
					math::vector3 normal_size = (player.rig_type == 0) ? math::vector3{ 2.0f, 1.0f, 1.0f } : math::vector3{ 1.2f, 1.2f, 1.2f };
					prim.set_size(normal_size);

					// Restore original flags (Head: CanCollide=false, CanTouch=true, CanQuery=true)
					std::uint8_t flags = memory->read<std::uint8_t>(prim.address + Offsets::Primitive::Flags);
					flags &= ~Offsets::PrimitiveFlags::CanCollide;
					flags |= Offsets::PrimitiveFlags::CanTouch;
					flags |= Offsets::PrimitiveFlags::CanQuery;
					memory->write<std::uint8_t>(prim.address + Offsets::Primitive::Flags, flags);
				}

				// Restore SpecialMesh scale if we stored it
				rbx::instance_t mesh = head_it->second.find_first_child_by_class("SpecialMesh");
				if (mesh.address)
				{
					auto it = original_head_scales.find(player.instance.address);
					if (it != original_head_scales.end())
					{
						memory->write<math::vector3>(mesh.address + Offsets::SpecialMesh::Scale, it->second);
						original_head_scales.erase(it);
					}
				}
			}

			// Reset Torso (R6)
			auto torso_it = player.parts.find("Torso");
			if (torso_it != player.parts.end() && torso_it->second.address)
			{
				restore_transparency(torso_it->second);
				rbx::primitive_t prim = torso_it->second.get_primitive();
				if (prim.address)
				{
					math::vector3 normal_size = { 2.0f, 2.0f, 1.0f };
					prim.set_size(normal_size);

					// Restore original flags (Torso: CanCollide=true, CanTouch=true, CanQuery=true)
					std::uint8_t flags = memory->read<std::uint8_t>(prim.address + Offsets::Primitive::Flags);
					flags |= Offsets::PrimitiveFlags::CanCollide;
					flags |= Offsets::PrimitiveFlags::CanTouch;
					flags |= Offsets::PrimitiveFlags::CanQuery;
					memory->write<std::uint8_t>(prim.address + Offsets::Primitive::Flags, flags);
				}
			}

			// Reset UpperTorso (R15)
			auto upper_torso_it = player.parts.find("UpperTorso");
			if (upper_torso_it != player.parts.end() && upper_torso_it->second.address)
			{
				restore_transparency(upper_torso_it->second);
				rbx::primitive_t prim = upper_torso_it->second.get_primitive();
				if (prim.address)
				{
					math::vector3 normal_size = { 2.0f, 1.6f, 1.0f };
					prim.set_size(normal_size);

					// Restore original flags (UpperTorso: CanCollide=true, CanTouch=true, CanQuery=true)
					std::uint8_t flags = memory->read<std::uint8_t>(prim.address + Offsets::Primitive::Flags);
					flags |= Offsets::PrimitiveFlags::CanCollide;
					flags |= Offsets::PrimitiveFlags::CanTouch;
					flags |= Offsets::PrimitiveFlags::CanQuery;
					memory->write<std::uint8_t>(prim.address + Offsets::Primitive::Flags, flags);
				}
			}

			// Reset HumanoidRootPart
			auto hrp_it = player.parts.find("HumanoidRootPart");
			if (hrp_it != player.parts.end() && hrp_it->second.address)
			{
				restore_transparency(hrp_it->second);
				rbx::primitive_t prim = hrp_it->second.get_primitive();
				if (prim.address)
				{
					math::vector3 normal_size = { 2.0f, 2.0f, 1.0f };
					prim.set_size(normal_size);

					// Restore original flags (HumanoidRootPart: CanCollide=true, CanTouch=true, CanQuery=true)
					std::uint8_t flags = memory->read<std::uint8_t>(prim.address + Offsets::Primitive::Flags);
					flags |= Offsets::PrimitiveFlags::CanCollide;
					flags |= Offsets::PrimitiveFlags::CanTouch;
					flags |= Offsets::PrimitiveFlags::CanQuery;
					memory->write<std::uint8_t>(prim.address + Offsets::Primitive::Flags, flags);
				}
			}
		}

		void expand_target_part(cache::entity_t& player)
		{
			if (!player.instance.address) return;

			// Disable state machine to prevent engine from overriding our CanCollide flag
			if (player.humanoid.address)
			{
				memory->write<bool>(player.humanoid.address + Offsets::Humanoid::EvaluateStateMachine, false);
			}

			std::string target_part_name = "";
			if (settings::hitbox_expander::target_part == 0) // Head
			{
				target_part_name = "Head";
			}
			else if (settings::hitbox_expander::target_part == 1) // Torso
			{
				target_part_name = (player.rig_type == 0) ? "Torso" : "UpperTorso";
			}
			else if (settings::hitbox_expander::target_part == 2) // HumanoidRootPart
			{
				target_part_name = "HumanoidRootPart";
			}

			if (target_part_name.empty()) return;

			auto part_it = player.parts.find(target_part_name);
			if (part_it == player.parts.end() || !part_it->second.address) return;

			rbx::primitive_t prim = part_it->second.get_primitive();
			if (!prim.address) return;

			math::vector3 new_size = {
				settings::hitbox_expander::size_x,
				settings::hitbox_expander::size_y,
				settings::hitbox_expander::size_z
			};

			try {
				prim.set_size(new_size);
				
				// Clear CanCollide (prevent barrier/flings), keep CanTouch and CanQuery (allow shooting/hits/touched)
				std::uint8_t flags = memory->read<std::uint8_t>(prim.address + Offsets::Primitive::Flags);
				flags &= ~Offsets::PrimitiveFlags::CanCollide;
				flags |= Offsets::PrimitiveFlags::CanTouch;
				flags |= Offsets::PrimitiveFlags::CanQuery;
				memory->write<std::uint8_t>(prim.address + Offsets::Primitive::Flags, flags);

				// If target part is Head, scale SpecialMesh to keep normal visual size
				bool has_mesh = false;
				if (settings::hitbox_expander::target_part == 0)
				{
					rbx::instance_t mesh = part_it->second.find_first_child_by_class("SpecialMesh");
					if (mesh.address)
					{
						has_mesh = true;
						math::vector3 current_scale = memory->read<math::vector3>(mesh.address + Offsets::SpecialMesh::Scale);
						
						// If not stored yet, store it
						auto it = original_head_scales.find(player.instance.address);
						math::vector3 original_scale;
						if (it == original_head_scales.end())
						{
							original_head_scales[player.instance.address] = current_scale;
							original_scale = current_scale;
						}
						else
						{
							original_scale = it->second;
						}
						
						math::vector3 normal_size = (player.rig_type == 0) ? math::vector3{ 2.0f, 1.0f, 1.0f } : math::vector3{ 1.2f, 1.2f, 1.2f };
						
						math::vector3 target_scale;
						target_scale.x = original_scale.x * (normal_size.x / new_size.x);
						target_scale.y = original_scale.y * (normal_size.y / new_size.y);
						target_scale.z = original_scale.z * (normal_size.z / new_size.z);
						
						memory->write<math::vector3>(mesh.address + Offsets::SpecialMesh::Scale, target_scale);
					}
				}

				// If the part does not have a SpecialMesh, make it transparent to hide the giant physical block
				if (!has_mesh)
				{
					auto it = original_transparencies.find(part_it->second.address);
					if (it == original_transparencies.end())
					{
						float current_trans = memory->read<float>(part_it->second.address + Offsets::BasePart::Transparency);
						original_transparencies[part_it->second.address] = current_trans;
					}
					memory->write<float>(part_it->second.address + Offsets::BasePart::Transparency, 1.0f);
				}
			}
			catch (...) {}
		}

		void reset_all_players()
		{
			std::shared_ptr<std::vector<cache::entity_t>> players_snapshot;
			{
				std::lock_guard<std::mutex> lock(cache::mtx);
				players_snapshot = cache::cached_players;
			}

			if (players_snapshot)
			{
				for (auto& player : *players_snapshot)
				{
					if (player.instance.address == 0 || player.instance.address == cache::cached_local_player.instance.address)
						continue;

					reset_all_parts(player);
				}
			}
		}
	}

	void run()
	{
		static int frame_counter = 0;
		static bool was_disabled_by_typing = false;
		static int last_target_part = -1;
		static bool last_enabled = false;

		for (;;)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			frame_counter++;

			// Typing Check Logic
			if (check::textchatopen)
			{
				if (!was_disabled_by_typing)
				{
					was_disabled_by_typing = true;
					reset_all_players();
				}
				continue;
			}

			if (was_disabled_by_typing && !check::textchatopen)
			{
				was_disabled_by_typing = false;
			}

			// Clean transition checks
			bool target_part_changed = (settings::hitbox_expander::target_part != last_target_part);
			bool enabled_changed = (settings::hitbox_expander::enabled != last_enabled);

			if (target_part_changed || enabled_changed)
			{
				reset_all_players();
				last_target_part = settings::hitbox_expander::target_part;
				last_enabled = settings::hitbox_expander::enabled;
			}

			if (!settings::hitbox_expander::enabled)
			{
				continue;
			}

			if (frame_counter % 5 != 0)
			{
				continue;
			}

			if (!game::datamodel.address || !game::local_player.address)
			{
				continue;
			}

			if (settings::hitbox_expander::size_x <= 0.0f ||
				settings::hitbox_expander::size_y <= 0.0f ||
				settings::hitbox_expander::size_z <= 0.0f)
			{
				continue;
			}

			std::shared_ptr<std::vector<cache::entity_t>> players_snapshot;
			{
				std::lock_guard<std::mutex> lock(cache::mtx);
				players_snapshot = cache::cached_players;
			}

			if (players_snapshot)
			{
				for (auto& player : *players_snapshot)
				{
					if (player.instance.address == 0 || player.instance.address == cache::cached_local_player.instance.address)
						continue;

					expand_target_part(player);
				}
			}
		}
	}

	void initialize()
	{
	}
}
