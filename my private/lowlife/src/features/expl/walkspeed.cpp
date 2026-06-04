#include "walkspeed.h"

#include <memory/memory.h>
#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <game/game.h>
#include <settings.h>
#include <check/typing_check.h>

namespace walkspeed
{
    void run()
    {
        static std::uint64_t cached_humanoid_address = 0;
        static std::uint64_t cached_model_address = 0;
        static std::uint32_t last_pid = 0;

        for (;;)
        {
            Sleep(1);
            static bool original_speed_set = false;
            static float original_speed = 16.0f;
            static bool was_disabled_by_typing = false;
            static bool previous_walkspeed_setting = false;

            std::uint32_t current_pid = memory->get_process_id();
            if (current_pid != last_pid)
            {
                cached_humanoid_address = 0;
                cached_model_address = 0;
                last_pid = current_pid;
            }

            std::uint64_t local_player_addr = game::local_player.address;
            std::uint64_t model_addr = 0;
            if (local_player_addr != 0)
            {
                rbx::player_t local_player_obj = { local_player_addr };
                model_addr = local_player_obj.get_model_instance().address;
            }

            if (model_addr != 0 && model_addr != cached_model_address)
            {
                rbx::model_instance_t model_instance = { model_addr };
                cached_humanoid_address = model_instance.find_first_child("Humanoid").address;
                cached_model_address = model_addr;
            }
            else if (model_addr == 0)
            {
                cached_humanoid_address = 0;
                cached_model_address = 0;
            }

            if (check::textchatopen)
            {
                was_disabled_by_typing = true;
                original_speed_set = false;

                if (local_player_addr != 0 && cached_humanoid_address != 0 && original_speed_set)
                {
                    memory->write<float>(cached_humanoid_address + Offsets::Humanoid::Walkspeed, original_speed);
                    memory->write<float>(cached_humanoid_address + Offsets::Humanoid::WalkspeedCheck, original_speed);
                }
                continue;
            }

            if (was_disabled_by_typing && !check::textchatopen)
            {
                if (previous_walkspeed_setting != settings::expl::walkspeed)
                {
                    was_disabled_by_typing = false;
                }
                else
                {
                    settings::expl::walkspeed = false;
                    continue;
                }
            }

            previous_walkspeed_setting = settings::expl::walkspeed;

            if (!settings::expl::walkspeed)
            {
                original_speed_set = false;

                if (local_player_addr != 0 && cached_humanoid_address != 0 && original_speed_set)
                {
                    memory->write<float>(cached_humanoid_address + Offsets::Humanoid::Walkspeed, original_speed);
                    memory->write<float>(cached_humanoid_address + Offsets::Humanoid::WalkspeedCheck, original_speed);
                }
                continue;
            }

            if (local_player_addr == 0 || cached_model_address == 0 || cached_humanoid_address == 0)
                continue;

            rbx::humanoid_t humanoid = { cached_humanoid_address };
            bool should_activate = false;

            switch (settings::expl::walkspeed_mode)
            {
            case 0:
                should_activate = true;
                break;
            case 1:
            {
                rbx::model_instance_t model_instance = { cached_model_address };
                rbx::instance_t body_effects = model_instance.find_first_child("BodyEffects");
                if (body_effects.address != 0)
                {
                    rbx::instance_t reload = body_effects.find_first_child("Reload");
                    if (reload.address != 0)
                    {
                        bool reload_value = memory->read<bool>(reload.address + Offsets::Misc::Value);
                        should_activate = reload_value;
                    }
                }
                break;
            }
            case 2:
            {
                float health = humanoid.get_health();
                should_activate = (health < settings::expl::walkspeed_health_threshold);
                break;
            }
            }

            if (should_activate)
            {
                if (!original_speed_set)
                {
                    original_speed = memory->read<float>(humanoid.address + Offsets::Humanoid::Walkspeed);
                    original_speed_set = true;
                }

                float current_speed = memory->read<float>(humanoid.address + Offsets::Humanoid::Walkspeed);
                if (current_speed != settings::expl::walkspeed_speed)
                {
                    for (int i = 0; i < 10; i++)
                    {
                        memory->write<float>(humanoid.address + Offsets::Humanoid::Walkspeed, settings::expl::walkspeed_speed);
                        memory->write<float>(humanoid.address + Offsets::Humanoid::WalkspeedCheck, settings::expl::walkspeed_speed);
                    }
                }
            }
            else
            {
                if (original_speed_set)
                {
                    memory->write<float>(humanoid.address + Offsets::Humanoid::Walkspeed, original_speed);
                    memory->write<float>(humanoid.address + Offsets::Humanoid::WalkspeedCheck, original_speed);
                }
            }
        }
    }
}