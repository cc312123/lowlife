#include "memory.h"
#include <filesystem>
#include <algorithm>

extern "C" intptr_t Luck_ReadVirtualMemory(
	HANDLE ProcessHandle,
	PVOID BaseAddress,
	PVOID Buffer,
	ULONG NumberOfBytesToRead,
	PULONG NumberOfBytesRead
)
{
	SIZE_T bytesRead = 0;
	BOOL status = ReadProcessMemory(ProcessHandle, BaseAddress, Buffer, NumberOfBytesToRead, &bytesRead);
	if (NumberOfBytesRead)
	{
		*NumberOfBytesRead = static_cast<ULONG>(bytesRead);
	}
	return status ? 0 : -1;
}

extern "C" intptr_t Luck_WriteVirtualMemory(
	HANDLE ProcessHandle,
	PVOID BaseAddress,
	PVOID Buffer,
	ULONG NumberOfBytesToWrite,
	PULONG NumberOfBytesWritten
)
{
	SIZE_T bytesWritten = 0;
	BOOL status = WriteProcessMemory(ProcessHandle, BaseAddress, Buffer, NumberOfBytesToWrite, &bytesWritten);
	if (NumberOfBytesWritten)
	{
		*NumberOfBytesWritten = static_cast<ULONG>(bytesWritten);
	}
	return status ? 0 : -1;
}

std::uint32_t memory_t::find_process_id(const std::string& process_name)
{
	std::uint32_t local_process_id = 0;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (snapshot == INVALID_HANDLE_VALUE)
	{
		return local_process_id;
	}

	PROCESSENTRY32 process_entry{};
	process_entry.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(snapshot, &process_entry))
	{
		do
		{
			if (!_stricmp(process_name.c_str(), process_entry.szExeFile))
			{
				if (process_entry.th32ProcessID != GetCurrentProcessId())
				{
					
					if (!_stricmp(process_name.c_str(), "RobloxPlayerBeta.exe"))
					{
						char path[MAX_PATH] = { 0 };
						DWORD size = sizeof(path);
						HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_entry.th32ProcessID);
						if (hProcess != NULL)
						{
							if (QueryFullProcessImageNameA(hProcess, 0, path, &size))
							{
								std::string path_str(path);
								std::string path_lower = path_str;
								std::transform(path_lower.begin(), path_lower.end(), path_lower.begin(), ::tolower);

								
								if (path_lower.find("my private") != std::string::npos ||
									path_lower.find("my_private") != std::string::npos ||
									path_lower.find("tung-ware") != std::string::npos ||
									path_lower.find("updates-server") != std::string::npos ||
									path_lower.find("\\temp\\") != std::string::npos ||
									path_lower.find("\\build\\") != std::string::npos)
								{
									CloseHandle(hProcess);
									continue;
								}

								
								try
								{
									std::error_code ec;
									auto file_size = std::filesystem::file_size(path_str, ec);
									if (!ec && file_size < 20 * 1024 * 1024) 
									{
										CloseHandle(hProcess);
										continue;
									}
								}
								catch (...) {}
							}
							CloseHandle(hProcess);
						}
					}

					local_process_id = process_entry.th32ProcessID;
					process_id = local_process_id;
					break;
				}
			}
		} while (Process32Next(snapshot, &process_entry));
	}

	CloseHandle(snapshot);
	return local_process_id;
}

std::uint64_t memory_t::find_module_address(const std::string& module_name)
{
	std::uint64_t module_address = 0;

	if (!process_handle)
	{
		return module_address;
	}

	DWORD process_id = GetProcessId(process_handle);
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process_id);

	if (snapshot == INVALID_HANDLE_VALUE)
	{
		return module_address;
	}

	MODULEENTRY32 module_entry{};
	module_entry.dwSize = sizeof(MODULEENTRY32);

	if (Module32First(snapshot, &module_entry))
	{
		do
		{
			if (!_stricmp(module_name.c_str(), module_entry.szModule))
			{
				module_address = reinterpret_cast<uint64_t>(module_entry.modBaseAddr);
				base_address = module_address;
				break;
			}
		} while (Module32Next(snapshot, &module_entry));
	}

	CloseHandle(snapshot);
	return module_address;
}

bool memory_t::attach_to_process(const std::string& process_name)
{
	DWORD pid = find_process_id(process_name);
	if (pid == 0) return false;

	HANDLE process = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_LIMITED_INFORMATION, false, pid);

	if (process == NULL || process == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	if (process_handle && process_handle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(process_handle);
	}

	process_handle = process;

	return true;
}

void memory_t::detach_from_process()
{
	if (process_handle && process_handle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(process_handle);
		process_handle = nullptr;
	}
	process_id = 0;
	base_address = 0;
}


std::string memory_t::read_string(std::uint64_t address)
{
	struct msvc_string_layout {
		union {
			char buf[16];
			char* ptr;
		} u;
		size_t size;
		size_t res;
	};

	msvc_string_layout layout{};
	Luck_ReadVirtualMemory(process_handle, reinterpret_cast<void*>(address), &layout, sizeof(msvc_string_layout), nullptr);

	if (layout.size == 0 || layout.size > 255)
	{
		return "Unknown";
	}

	if (layout.size < 16)
	{
		return std::string(layout.u.buf, layout.size);
	}

	std::vector<char> buffer(layout.size + 1, 0);
	Luck_ReadVirtualMemory(process_handle, layout.u.ptr, buffer.data(), layout.size, nullptr);

	return std::string(buffer.data(), layout.size);
}

std::uint32_t memory_t::get_process_id()
{
	return process_id;
}

std::uint64_t memory_t::get_module_address()
{
	return base_address;
}

HANDLE memory_t::get_process_handle()
{
	return process_handle;
}

std::uint64_t memory_t::scan_signature(const std::string& section_name, const std::string& signature)
{
	std::vector<uint8_t> pattern;
	std::vector<bool> mask;

	std::string current_byte;
	for (char c : signature)
	{
		if (c == ' ')
		{
			if (!current_byte.empty())
			{
				if (current_byte == "?" || current_byte == "??")
				{
					pattern.push_back(0);
					mask.push_back(false);
				}
				else
				{
					pattern.push_back(static_cast<uint8_t>(std::strtoul(current_byte.c_str(), nullptr, 16)));
					mask.push_back(true);
				}
				current_byte.clear();
			}
		}
		else
		{
			current_byte += c;
		}
	}
	if (!current_byte.empty())
	{
		if (current_byte == "?" || current_byte == "??")
		{
			pattern.push_back(0);
			mask.push_back(false);
		}
		else
		{
			pattern.push_back(static_cast<uint8_t>(std::strtoul(current_byte.c_str(), nullptr, 16)));
			mask.push_back(true);
		}
	}

	if (pattern.empty())
		return 0;

	if (!process_handle || !base_address)
		return 0;

	IMAGE_DOS_HEADER dos_header{};
	if (Luck_ReadVirtualMemory(process_handle, reinterpret_cast<void*>(base_address), &dos_header, sizeof(dos_header), nullptr) != 0)
		return 0;

	if (dos_header.e_magic != IMAGE_DOS_SIGNATURE)
		return 0;

	IMAGE_NT_HEADERS64 nt_headers{};
	if (Luck_ReadVirtualMemory(process_handle, reinterpret_cast<void*>(base_address + dos_header.e_lfanew), &nt_headers, sizeof(nt_headers), nullptr) != 0)
		return 0;

	if (nt_headers.Signature != IMAGE_NT_SIGNATURE)
		return 0;

	std::uint64_t section_header_addr = base_address + dos_header.e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + nt_headers.FileHeader.SizeOfOptionalHeader;

	for (WORD i = 0; i < nt_headers.FileHeader.NumberOfSections; ++i)
	{
		IMAGE_SECTION_HEADER section{};
		if (Luck_ReadVirtualMemory(process_handle, reinterpret_cast<void*>(section_header_addr + (i * sizeof(IMAGE_SECTION_HEADER))), &section, sizeof(section), nullptr) != 0)
			continue;

		char name[9]{};
		std::memcpy(name, section.Name, 8);
		if (std::string(name) == section_name)
		{
			std::vector<uint8_t> buffer(section.Misc.VirtualSize);
			if (Luck_ReadVirtualMemory(process_handle, reinterpret_cast<void*>(base_address + section.VirtualAddress), buffer.data(), section.Misc.VirtualSize, nullptr) == 0)
			{
				for (size_t j = 0; j <= buffer.size() - pattern.size(); ++j)
				{
					bool match = true;
					for (size_t k = 0; k < pattern.size(); ++k)
					{
						if (mask[k] && buffer[j + k] != pattern[k])
						{
							match = false;
							break;
						}
					}
					if (match)
					{
						return base_address + section.VirtualAddress + j;
					}
				}
			}
		}
	}
	return 0;
}