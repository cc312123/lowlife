#include "accuracy.h"
#include <settings.h>
#include <game/game.h>
#include <check/typing_check.h>
#include <render/render.h>
#include <Windows.h>
#include <chrono>
#include <thread>
#include <vector>

namespace
{
	void press_key(WORD vk)
	{
		INPUT input = {};
		input.type = INPUT_KEYBOARD;
		input.ki.wVk = vk;
		input.ki.wScan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
		input.ki.dwFlags = 0;
		SendInput(1, &input, sizeof(INPUT));
	}

	void release_key(WORD vk)
	{
		INPUT input = {};
		input.type = INPUT_KEYBOARD;
		input.ki.wVk = vk;
		input.ki.wScan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
		input.ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(1, &input, sizeof(INPUT));
	}
}

namespace accuracy
{
	void run()
	{
		bool was_crouching = false;
		bool was_movement_dampened = false;
		std::vector<WORD> keys_to_restore;

		for (;;)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(5));

			if (!game::datamodel.address || !game::local_player.address)
			{
				if (was_crouching)
				{
					release_key(settings::expl::crouch_key);
					was_crouching = false;
				}
				if (was_movement_dampened)
				{
					for (WORD vk : keys_to_restore)
					{
						press_key(vk);
					}
					keys_to_restore.clear();
					was_movement_dampened = false;
				}
				continue;
			}

			if (!settings::expl::mechanical_nospread)
			{
				if (was_crouching)
				{
					release_key(settings::expl::crouch_key);
					was_crouching = false;
				}
				if (was_movement_dampened)
				{
					for (WORD vk : keys_to_restore)
					{
						press_key(vk);
					}
					keys_to_restore.clear();
					was_movement_dampened = false;
				}
				continue;
			}

			bool roblox_focused = (GetForegroundWindow() == game::wnd);
			bool menu_open = render->running;
			bool chat_open = check::textchatopen;

			if (!roblox_focused || menu_open || chat_open)
			{
				if (was_crouching)
				{
					release_key(settings::expl::crouch_key);
					was_crouching = false;
				}
				if (was_movement_dampened)
				{
					for (WORD vk : keys_to_restore)
					{
						press_key(vk);
					}
					keys_to_restore.clear();
					was_movement_dampened = false;
				}
				continue;
			}

			bool is_firing = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

			// Crouch-on-Fire Spread Reduction
			if (settings::expl::crouch_on_fire)
			{
				if (is_firing)
				{
					if (!was_crouching)
					{
						press_key(settings::expl::crouch_key);
						was_crouching = true;
					}
				}
				else
				{
					if (was_crouching)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(50));
						release_key(settings::expl::crouch_key);
						was_crouching = false;
					}
				}
			}
			else
			{
				if (was_crouching)
				{
					release_key(settings::expl::crouch_key);
					was_crouching = false;
				}
			}

			// Stop-on-Shot Spread Reduction
			if (settings::expl::stop_on_shot)
			{
				if (is_firing)
				{
					if (!was_movement_dampened)
					{
						const WORD movement_keys[] = { 'W', 'A', 'S', 'D', VK_UP, VK_LEFT, VK_DOWN, VK_RIGHT };
						for (WORD vk : movement_keys)
						{
							if (GetAsyncKeyState(vk) & 0x8000)
							{
								keys_to_restore.push_back(vk);
								release_key(vk);
							}
						}
						was_movement_dampened = true;
					}
				}
				else
				{
					if (was_movement_dampened)
					{
						for (WORD vk : keys_to_restore)
						{
							if (GetAsyncKeyState(vk) & 0x8000)
							{
								press_key(vk);
							}
						}
						keys_to_restore.clear();
						was_movement_dampened = false;
					}
				}
			}
			else
			{
				if (was_movement_dampened)
				{
					for (WORD vk : keys_to_restore)
					{
						press_key(vk);
					}
					keys_to_restore.clear();
					was_movement_dampened = false;
				}
			}
		}
	}
}
