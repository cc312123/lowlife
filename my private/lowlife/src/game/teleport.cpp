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
    if (hrp.address)
    {
        rbx::part_t part(hrp.address);
        rbx::primitive_t prim = part.get_primitive();
        if (prim.address)
        {
            memory->write<math::vector3>(prim.address + Offsets::Primitive::Position, pos);
        }
    }
}

