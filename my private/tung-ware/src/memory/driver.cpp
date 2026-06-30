#include "driver.h"
#include <Windows.h>
#include <algorithm>

namespace input
{
	namespace
	{
		HANDLE g_device_handle = INVALID_HANDLE_VALUE;

		#pragma pack(push, 1)
		struct MOUSE_IO {
			char button;
			char x;
			char y;
			char wheel;
			char unk1;
		};
		#pragma pack(pop)

		constexpr DWORD IOCTL_BUSENUM_PLAY_MOUSEMOVE = 0x2A2010;
	}

	bool init()
	{
		if (g_device_handle != INVALID_HANDLE_VALUE)
			return true;

		g_device_handle = CreateFileA("\\\\.\\logitech_gkey", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		return g_device_handle != INVALID_HANDLE_VALUE;
	}

	void close()
	{
		if (g_device_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(g_device_handle);
			g_device_handle = INVALID_HANDLE_VALUE;
		}
	}

	void move_mouse_relative(int dx, int dy)
	{
		if (g_device_handle != INVALID_HANDLE_VALUE)
		{
			while (dx != 0 || dy != 0)
			{
				char step_x = static_cast<char>(std::clamp(dx, -127, 127));
				char step_y = static_cast<char>(std::clamp(dy, -127, 127));
				dx -= step_x;
				dy -= step_y;

				MOUSE_IO report{};
				report.button = 0;
				report.x = step_x;
				report.y = step_y;
				report.wheel = 0;
				report.unk1 = 0;

				DWORD bytes_returned = 0;
				DeviceIoControl(g_device_handle, IOCTL_BUSENUM_PLAY_MOUSEMOVE, &report, sizeof(report), nullptr, 0, &bytes_returned, nullptr);
			}
		}
		else
		{
			// Fallback to standard SendInput
			INPUT input_event{};
			input_event.type = INPUT_MOUSE;
			input_event.mi.dx = dx;
			input_event.mi.dy = dy;
			input_event.mi.dwFlags = MOUSEEVENTF_MOVE;
			SendInput(1, &input_event, sizeof(INPUT));
		}
	}
}
