#include "typing_check.h"

#include <memory/memory.h>
#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <game/game.h>
#include <Windows.h>
#include <thread>
#include <chrono>

namespace check
{
	void run()
	{
		for (;;)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));

			if (game::datamodel.address == 0)
			{
				check::textchatopen = false;
				continue;
			}

			bool is_typing = false;

			// 1. Try checking UserInputService -> WindowInputState -> CurrentTextBox
			try
			{
				rbx::instance_t uis = game::datamodel.find_first_child_by_class("UserInputService");
				if (uis.address != 0)
				{
					std::uint64_t input_state = memory->read<std::uint64_t>(uis.address + Offsets::UserInputService::WindowInputState);
					if (input_state != 0 && (input_state & 0x7) == 0 && input_state > 0x100000 && input_state < 0x7fffffffffff)
					{
						std::uint64_t current_textbox = memory->read<std::uint64_t>(input_state + Offsets::WindowInputState::CurrentTextBox);
						if (current_textbox != 0 && (current_textbox & 0x7) == 0 && current_textbox > 0x100000 && current_textbox < 0x7fffffffffff)
						{
							rbx::instance_t textbox_inst{ current_textbox };
							std::string class_name = textbox_inst.get_class_name();
							if (class_name == "TextBox")
							{
								is_typing = true;
							}
						}
					}
				}
			}
			catch (...) {}

			// 2. Fallback to ChatInputBarConfiguration (if for some reason UIS check fails)
			if (!is_typing)
			{
				try
				{
					rbx::instance_t text_chat_service = game::datamodel.find_first_child_by_class("TextChatService");
					if (text_chat_service.address != 0)
					{
						rbx::instance_t cibc = text_chat_service.find_first_child("ChatInputBarConfiguration");
						if (cibc.address != 0)
						{
							bool cibc_focused = memory->read<bool>(cibc.address + 0x156);
							if (cibc_focused)
							{
								is_typing = true;
							}
						}
					}
				}
				catch (...) {}
			}

			if (is_typing && !check::was_typing)
			{
				check::was_typing = true;
			}
			check::textchatopen = is_typing;
		}
	}
}

