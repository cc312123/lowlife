#include "walkspeed.h"

#include <memory/memory.h>
#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <game/game.h>
#include <settings.h>
#include <check/typing_check.h>
#include <mutex>
#include <cache/cache.h>

namespace walkspeed
{
    void run()
    {
        for (;;)
        {
            static bool original_speed_set = false;
            static float original_speed = 16.0f;
            static bool was_disabled_by_typing = false;
            static bool previous_walkspeed_setting = false;

            if (check::textchatopen)
            {
                was_disabled_by_typing = true;
                original_speed_set = false;

                if (game::local_player.address != 0)
                {
                    std::uint64_t humanoid_address = 0;
                    {
                        std::lock_guard<std::mutex> lock(cache::mtx);
                        if (cache::cached_local_player.instance.address == game::local_player.address)
                        {
                            humanoid_address = cache::cached_local_player.humanoid.address;
                        }
                    }
                    if (humanoid_address != 0 && original_speed_set)
                    {
                        memory->write<float>(humanoid_address + Offsets::Humanoid::Walkspeed, original_speed);
                        memory->write<float>(humanoid_address + Offsets::Humanoid::WalkspeedCheck, original_speed);
                    }
                }
                Sleep(100);
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
                    Sleep(100);
                    continue;
                }
            }

            previous_walkspeed_setting = settings::expl::walkspeed;

            if (!settings::expl::walkspeed)
            {
                original_speed_set = false;

                if (game::local_player.address != 0)
                {
                    std::uint64_t humanoid_address = 0;
                    {
                        std::lock_guard<std::mutex> lock(cache::mtx);
                        if (cache::cached_local_player.instance.address == game::local_player.address)
                        {
                            humanoid_address = cache::cached_local_player.humanoid.address;
                        }
                    }
                    if (humanoid_address != 0 && original_speed_set)
                    {
                        memory->write<float>(humanoid_address + Offsets::Humanoid::Walkspeed, original_speed);
                        memory->write<float>(humanoid_address + Offsets::Humanoid::WalkspeedCheck, original_speed);
                    }
                }
                Sleep(100);
                continue;
            }

            rbx::player_t local_player_obj = { game::local_player.address };
            if (local_player_obj.address == 0) {
                Sleep(50);
                continue;
            }

            rbx::model_instance_t model_instance = local_player_obj.get_model_instance();
            if (model_instance.address == 0) {
                Sleep(50);
                continue;
            }

            std::uint64_t humanoid_address = 0;
            {
                std::lock_guard<std::mutex> lock(cache::mtx);
                if (cache::cached_local_player.instance.address == game::local_player.address)
                {
                    humanoid_address = cache::cached_local_player.humanoid.address;
                }
            }
            if (humanoid_address == 0) {
                Sleep(50);
                continue;
            }

            bool should_activate = false;

            switch (settings::expl::walkspeed_mode)
            {
            case 0:
                should_activate = true;
                break;
            case 1:
                {
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
                    float health = memory->read<float>(humanoid_address + Offsets::Humanoid::Health);
                    should_activate = (health < settings::expl::walkspeed_health_threshold);
                    break;
                }
            }

            if (should_activate)
            {
                if (!original_speed_set)
                {
                    original_speed = memory->read<float>(humanoid_address + Offsets::Humanoid::Walkspeed);
                    original_speed_set = true;
                }

                float current_speed = memory->read<float>(humanoid_address + Offsets::Humanoid::Walkspeed);
                if (current_speed != settings::expl::walkspeed_speed)
                {
                    for (int i = 0; i < 50; i++)
                    {
                        memory->write<float>(humanoid_address + Offsets::Humanoid::Walkspeed, settings::expl::walkspeed_speed);
                        memory->write<float>(humanoid_address + Offsets::Humanoid::WalkspeedCheck, settings::expl::walkspeed_speed);
                    }
                }
            }
            else
            {
                if (original_speed_set)
                {
                    memory->write<float>(humanoid_address + Offsets::Humanoid::Walkspeed, original_speed);
                    memory->write<float>(humanoid_address + Offsets::Humanoid::WalkspeedCheck, original_speed);
                }
            }
            Sleep(1);
        }
    }
}