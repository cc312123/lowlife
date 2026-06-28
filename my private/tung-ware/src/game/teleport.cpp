#include <string> 
#include <iostream> 
#include <windows.h>
#include <algorithm>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <memory/memory.h>
#include <game/game.h>
#include <settings.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cmath>

static std::atomic<bool> g_is_teleporting{ false };
static std::mutex g_teleport_mtx;
static math::vector3 g_teleport_target_pos;

void TeleportTo(const math::vector3 pos)
{
    if (!game::players.address)
        return;

    rbx::instance_t localPlayer = memory->read<rbx::instance_t>(game::players.address + Offsets::Player::LocalPlayer);
    
    if (!localPlayer.address)
        return;

    rbx::model_instance_t model_instance = memory->read<rbx::model_instance_t>(localPlayer.address + Offsets::Player::ModelInstance);
    
    if (!model_instance.address)
        return;

    rbx::instance_t hrp = model_instance.find_first_child("HumanoidRootPart");
    if (!hrp.address)
        return;

    rbx::part_t part(hrp.address);
    rbx::primitive_t prim = part.get_primitive();
    if (!prim.address)
        return;

    if (!settings::expl::legit_teleport)
    {
        memory->write<math::vector3>(prim.address + Offsets::Primitive::Position, pos);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_teleport_mtx);
        g_teleport_target_pos = pos;
    }

    if (!g_is_teleporting.exchange(true))
    {
        std::thread([]() {
            while (true)
            {
                if (!game::players.address)
                    break;

                rbx::instance_t localPlayer = memory->read<rbx::instance_t>(game::players.address + Offsets::Player::LocalPlayer);
                if (!localPlayer.address)
                    break;

                rbx::model_instance_t model_instance = memory->read<rbx::model_instance_t>(localPlayer.address + Offsets::Player::ModelInstance);
                if (!model_instance.address)
                    break;

                rbx::instance_t hrp = model_instance.find_first_child("HumanoidRootPart");
                if (!hrp.address)
                    break;

                rbx::part_t part(hrp.address);
                rbx::primitive_t prim = part.get_primitive();
                if (!prim.address)
                    break;

                math::vector3 target;
                {
                    std::lock_guard<std::mutex> lock(g_teleport_mtx);
                    target = g_teleport_target_pos;
                }

                math::vector3 current_pos = prim.get_position();

                math::vector3 delta = target - current_pos;
                float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

                float speed = settings::expl::legit_teleport_speed;
                int delay_ms = settings::expl::legit_teleport_delay;
                if (delay_ms < 1) delay_ms = 1;
                if (speed < 1.0f) speed = 1.0f;

                float dt = (float)delay_ms / 1000.0f;
                float step_dist = speed * dt;

                if (dist <= step_dist || dist < 0.1f)
                {
                    memory->write<math::vector3>(prim.address + Offsets::Primitive::Position, target);
                    break;
                }

                math::vector3 dir = delta * (1.0f / dist);
                math::vector3 next_pos = current_pos + dir * step_dist;
                memory->write<math::vector3>(prim.address + Offsets::Primitive::Position, next_pos);

                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }
            g_is_teleporting = false;
        }).detach();
    }
}

