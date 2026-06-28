#include "walkspeed.h"

#include <memory/memory.h>
#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <game/game.h>
#include <settings.h>
#include <check/typing_check.h>
#include <mutex>
#include <cache/cache.h>
#include <chrono>
#include <cmath>

namespace walkspeed
{
    void run()
    {
        static bool original_speed_set = false;
        static float original_speed = 16.0f;
        static bool toggle_active = false;
        static bool key_was_down = false;

        auto last_time = std::chrono::high_resolution_clock::now();

        for (;;)
        {
            Sleep(1);

            
            auto current_time = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(current_time - last_time).count();
            last_time = current_time;
            if (deltaTime > 0.1f) deltaTime = 0.1f;

            if (game::local_player.address == 0)
            {
                original_speed_set = false;
                toggle_active = false;
                key_was_down = false;
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

            if (humanoid_address == 0)
            {
                original_speed_set = false;
                continue;
            }

            
            bool key_pressed = false;
            if (settings::expl::walkspeed_keybind == 0 || settings::expl::walkspeed_keybind_mode == 2)
            {
                key_pressed = true;
            }
            else if (settings::expl::walkspeed_keybind_mode == 0) 
            {
                key_pressed = GetAsyncKeyState(settings::expl::walkspeed_keybind) & 0x8000;
            }
            else if (settings::expl::walkspeed_keybind_mode == 1) 
            {
                bool key_is_down = GetAsyncKeyState(settings::expl::walkspeed_keybind) & 0x8000;
                if (key_is_down && !key_was_down)
                {
                    toggle_active = !toggle_active;
                }
                key_was_down = key_is_down;
                key_pressed = toggle_active;
            }

            bool should_activate = settings::expl::walkspeed && key_pressed && !check::textchatopen;

            
            if (should_activate)
            {
                rbx::player_t local_player_obj = { game::local_player.address };
                rbx::model_instance_t model_instance = local_player_obj.get_model_instance();

                if (model_instance.address != 0)
                {
                    switch (settings::expl::walkspeed_mode)
                    {
                    case 0: 
                        should_activate = true;
                        break;
                    case 1: 
                        {
                            should_activate = false;
                            rbx::instance_t body_effects = model_instance.find_first_child("BodyEffects");
                            if (body_effects.address != 0)
                            {
                                rbx::instance_t reload = body_effects.find_first_child("Reload");
                                if (reload.address != 0)
                                {
                                    should_activate = memory->read<bool>(reload.address + Offsets::Misc::Value);
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
                }
                else
                {
                    should_activate = false;
                }
            }

            if (should_activate)
            {
                rbx::player_t local_player_obj = { game::local_player.address };
                rbx::model_instance_t model_instance = local_player_obj.get_model_instance();

                if (settings::expl::walkspeed_method == 0) 
                {
                    if (!original_speed_set)
                    {
                        original_speed = memory->read<float>(humanoid_address + Offsets::Humanoid::Walkspeed);
                        if (original_speed <= 0.0f || original_speed > 100.0f) original_speed = 16.0f;
                        original_speed_set = true;
                    }

                    float current_speed = memory->read<float>(humanoid_address + Offsets::Humanoid::Walkspeed);
                    if (std::abs(current_speed - settings::expl::walkspeed_speed) > 0.1f)
                    {
                        
                        for (int i = 0; i < 5; i++)
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
                        original_speed_set = false;
                    }

                    math::vector3 move_dir = memory->read<math::vector3>(humanoid_address + Offsets::Humanoid::MoveDirection);
                    float dir_len = std::sqrt(move_dir.x * move_dir.x + move_dir.y * move_dir.y + move_dir.z * move_dir.z);

                    if (dir_len > 0.1f)
                    {
                        move_dir.x /= dir_len;
                        move_dir.y /= dir_len;
                        move_dir.z /= dir_len;

                        rbx::instance_t hrp = model_instance.find_first_child("HumanoidRootPart");
                        if (hrp.address != 0)
                        {
                            rbx::part_t part(hrp.address);
                            rbx::primitive_t prim = part.get_primitive();
                            if (prim.address != 0)
                            {
                                if (settings::expl::walkspeed_method == 1) 
                                {
                                    math::vector3 pos = prim.get_position();
                                    
                                    float boost = settings::expl::walkspeed_speed - 16.0f;
                                    if (boost > 0.0f)
                                    {
                                        pos.x += move_dir.x * boost * deltaTime;
                                        pos.z += move_dir.z * boost * deltaTime;
                                        
                                        memory->write<math::vector3>(prim.address + Offsets::Primitive::Position, pos);
                                    }
                                }
                                else if (settings::expl::walkspeed_method == 2) 
                                {
                                    math::vector3 vel = prim.get_velocity();
                                    vel.x = move_dir.x * settings::expl::walkspeed_speed;
                                    vel.z = move_dir.z * settings::expl::walkspeed_speed;
                                    memory->write<math::vector3>(prim.address + Offsets::Primitive::AssemblyLinearVelocity, vel);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                
                if (original_speed_set)
                {
                    memory->write<float>(humanoid_address + Offsets::Humanoid::Walkspeed, original_speed);
                    memory->write<float>(humanoid_address + Offsets::Humanoid::WalkspeedCheck, original_speed);
                    original_speed_set = false;
                }
            }
        }
    }
}