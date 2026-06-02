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
	static std::uint64_t cibc_address = 0;

	void run()
	{
		std::uint64_t last_datamodel = 0;

		for (;;)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			if (game::datamodel.address == 0)
			{
				cibc_address = 0;
				last_datamodel = 0;
				continue;
			}

			// If game restarted or datamodel changed, reset cached pointer
			if (game::datamodel.address != last_datamodel || cibc_address == 0)
			{
				last_datamodel = game::datamodel.address;
				cibc_address = 0;

				rbx::instance_t text_chat_service = game::datamodel.find_first_child_by_class("TextChatService");
				if (text_chat_service.address != 0)
				{
					rbx::instance_t cibc = text_chat_service.find_first_child("ChatInputBarConfiguration");
					if (cibc.address != 0)
					{
						cibc_address = cibc.address;
					}
				}
			}

			if (cibc_address != 0)
			{
				bool textchatopen = false;
				try
				{
					textchatopen = memory->read<bool>(cibc_address + 0x156);
				}
				catch (...)
				{
					// If the memory read failed (e.g. game closed or object freed), reset the cached pointer
					cibc_address = 0;
					continue;
				}

				if (textchatopen && !check::was_typing)
				{
					check::was_typing = true;
				}
				check::textchatopen = textchatopen;
			}
			else
			{
				check::textchatopen = false;
			}
		}
	}
}

