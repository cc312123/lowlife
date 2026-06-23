#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <memory/memory.h>
#include <game/game.h>
#include <algorithm>
#include <cctype>
#include <mutex>
#include <unordered_map>

std::string rbx::nameable_t::get_name()
{
	std::uint64_t name = memory->read<std::uint64_t>(this->address + Offsets::Instance::Name);

	if (name)
	{
		return memory->read_string(name);
	}

	return "unknown";
}

std::string rbx::nameable_t::get_class_name()
{
	if (this->address == 0) return "unknown";

	static std::unordered_map<std::uint64_t, std::string> class_cache;
	static std::mutex class_cache_mutex;
	static std::uint32_t last_cache_pid = 0;

	std::uint64_t class_descriptor = memory->read<std::uint64_t>(this->address + Offsets::Instance::ClassDescriptor);
	if (class_descriptor == 0) return "unknown";

	{
		std::lock_guard<std::mutex> lock(class_cache_mutex);
		std::uint32_t current_pid = memory->get_process_id();
		if (current_pid != last_cache_pid)
		{
			class_cache.clear();
			last_cache_pid = current_pid;
		}

		auto it = class_cache.find(class_descriptor);
		if (it != class_cache.end())
		{
			return it->second;
		}
	}

	std::uint64_t class_name = memory->read<std::uint64_t>(class_descriptor + Offsets::Instance::ClassName);
	std::string name = "unknown";
	if (class_name)
	{
		name = memory->read_string(class_name);
	}

	{
		std::lock_guard<std::mutex> lock(class_cache_mutex);
		class_cache[class_descriptor] = name;
	}

	return name;
}

std::vector<rbx::instance_t> rbx::interface_t::get_children()
{
	rbx::instance_t* base = static_cast<rbx::instance_t*>(this);

	std::uint64_t start{ memory->read<std::uint64_t>(base->address + Offsets::Instance::ChildrenStart) };
	if (start == 0) return {};

	std::uint64_t array_start = memory->read<std::uint64_t>(start);
	std::uint64_t array_end = memory->read<std::uint64_t>(start + Offsets::Instance::ChildrenEnd);

	if (array_start == 0 || array_end == 0 || array_start >= array_end)
	{
		return {};
	}

	std::uint64_t size_bytes = array_end - array_start;
	std::uint64_t count = size_bytes / sizeof(std::shared_ptr<void*>);

	if (count > 50000)
	{
		return {};
	}

	struct raw_shared_ptr {
		std::uint64_t ptr;
		std::uint64_t ref_count;
	};

	std::vector<raw_shared_ptr> raw_ptrs(count);
	Luck_ReadVirtualMemory(memory->get_process_handle(), reinterpret_cast<void*>(array_start), raw_ptrs.data(), static_cast<ULONG>(count * sizeof(raw_shared_ptr)), nullptr);

	std::vector<rbx::instance_t> children;
	children.reserve(count);

	for (std::uint64_t i = 0; i < count; ++i)
	{
		std::uint64_t child_address = raw_ptrs[i].ptr;
		if (child_address != 0)
		{
			children.emplace_back(child_address);
		}
	}

	return children;
}

size_t rbx::interface_t::get_children_count()
{
	rbx::instance_t* base = static_cast<rbx::instance_t*>(this);
	if (base->address == 0) return 0;

	std::uint64_t start = memory->read<std::uint64_t>(base->address + Offsets::Instance::ChildrenStart);
	if (start == 0) return 0;

	std::uint64_t array_start = memory->read<std::uint64_t>(start);
	std::uint64_t array_end = memory->read<std::uint64_t>(start + Offsets::Instance::ChildrenEnd);

	if (array_start == 0 || array_end == 0 || array_start >= array_end)
	{
		return 0;
	}

	std::uint64_t size_bytes = array_end - array_start;
	return size_bytes / sizeof(std::shared_ptr<void*>);
}

rbx::instance_t rbx::interface_t::find_first_child(std::string_view str)
{
	std::vector<rbx::instance_t> children = this->get_children();

	for (rbx::instance_t& child : children)
	{
		if (child.get_name() == str)
		{
			return child;
		}
	}

	return {};
}

rbx::instance_t rbx::interface_t::find_first_child_by_class(std::string_view str)
{
	std::vector<rbx::instance_t> children = this->get_children();

	for (rbx::instance_t& child : children)
	{
		if (child.get_class_name() == str)
		{
			return child;
		}
	}

	return {};
}

rbx::instance_t rbx::interface_t::find_descendant_value_by_name_substrings(const std::vector<std::string>& patterns, int max_depth)
{
	rbx::instance_t* base = static_cast<rbx::instance_t*>(this);
	if (base->address == 0 || max_depth < 0) return {};

	std::vector<rbx::instance_t> children = this->get_children();
	for (rbx::instance_t& child : children)
	{
		if (child.address == 0) continue;

		std::string cclass = child.get_class_name();
		if (cclass.find("Value") != std::string::npos)
		{
			std::string cname = child.get_name();
			std::string lower_cname = cname;
			std::transform(lower_cname.begin(), lower_cname.end(), lower_cname.begin(), ::tolower);

			for (const std::string& pattern : patterns)
			{
				if (lower_cname.find(pattern) != std::string::npos)
				{
					return child;
				}
			}
		}

		rbx::instance_t found = child.find_descendant_value_by_name_substrings(patterns, max_depth - 1);
		if (found.address != 0)
		{
			return found;
		}
	}

	return {};
}

rbx::model_instance_t rbx::player_t::get_model_instance()
{
	return { memory->read<std::uint64_t>(this->address + Offsets::Player::ModelInstance) };
}

std::int64_t rbx::player_t::get_user_id()
{
	return memory->read<std::int64_t>(this->address + Offsets::Player::UserId);
}

std::string rbx::player_t::get_crew_id()
{
	if (this->address == 0) return "";

	auto extract_crew_id = [](rbx::instance_t crew) -> std::string {
		if (crew.address == 0) return "";
		
		std::string s_val = memory->read_string(crew.address + Offsets::Misc::Value);
		if (!s_val.empty() && s_val != "Unknown") {
			s_val.erase(s_val.begin(), std::find_if(s_val.begin(), s_val.end(), [](unsigned char ch) {
				return !std::isspace(ch);
			}));
			s_val.erase(std::find_if(s_val.rbegin(), s_val.rend(), [](unsigned char ch) {
				return !std::isspace(ch);
			}).base(), s_val.end());
			return s_val;
		}
		
		std::int64_t i_val = memory->read<std::int64_t>(crew.address + Offsets::Misc::Value);
		if (i_val != 0) {
			return std::to_string(i_val);
		}
		
		return "";
	};

	rbx::instance_t data_folder = this->find_first_child("DataFolder");
	if (data_folder.address != 0) {
		rbx::instance_t info = data_folder.find_first_child("Information");
		if (info.address != 0) {
			rbx::instance_t crew = info.find_first_child("Crew");
			if (crew.address != 0) {
				std::string cid = extract_crew_id(crew);
				if (!cid.empty()) return cid;
			}
		}
	}

	rbx::model_instance_t model = this->get_model_instance();
	if (model.address != 0) {
		rbx::instance_t data_folder_char = model.find_first_child("DataFolder");
		if (data_folder_char.address != 0) {
			rbx::instance_t info = data_folder_char.find_first_child("Information");
			if (info.address != 0) {
				rbx::instance_t crew = info.find_first_child("Crew");
				if (crew.address != 0) {
					std::string cid = extract_crew_id(crew);
					if (!cid.empty()) return cid;
				}
			}
		}
	}

	rbx::instance_t direct_crew = this->find_first_child("Crew");
	if (direct_crew.address != 0) {
		std::string cid = extract_crew_id(direct_crew);
		if (!cid.empty()) return cid;
	}

	rbx::instance_t direct_crew_id = this->find_first_child("CrewID");
	if (direct_crew_id.address != 0) {
		std::string cid = extract_crew_id(direct_crew_id);
		if (!cid.empty()) return cid;
	}

	return "";
}

std::uint8_t rbx::humanoid_t::get_rig_type()
{
	return { memory->read<std::uint8_t>(this->address + Offsets::Humanoid::RigType) };
}

float rbx::humanoid_t::get_health()
{
	return memory->read<float>(this->address + Offsets::Humanoid::Health);
}

float rbx::humanoid_t::get_max_health()
{
	return memory->read<float>(this->address + Offsets::Humanoid::MaxHealth);
}

rbx::primitive_t rbx::part_t::get_primitive()
{
	static std::unordered_map<std::uint64_t, std::uint64_t> primitive_cache;
	static std::mutex primitive_cache_mutex;
	static std::chrono::steady_clock::time_point last_clear = std::chrono::steady_clock::now();

	if (this->address == 0) return { 0 };

	auto now = std::chrono::steady_clock::now();
	{
		std::lock_guard<std::mutex> lock(primitive_cache_mutex);
		if (std::chrono::duration_cast<std::chrono::seconds>(now - last_clear).count() > 5)
		{
			primitive_cache.clear();
			last_clear = now;
		}

		auto it = primitive_cache.find(this->address);
		if (it != primitive_cache.end())
		{
			return { it->second };
		}
	}

	std::uint64_t prim_address = memory->read<std::uint64_t>(this->address + Offsets::BasePart::Primitive);

	{
		std::lock_guard<std::mutex> lock(primitive_cache_mutex);
		primitive_cache[this->address] = prim_address;
	}

	return { prim_address };
}

math::vector3 rbx::primitive_t::get_size()
{
	return memory->read<math::vector3>(this->address + Offsets::Primitive::Size);
}

void rbx::primitive_t::set_size(const math::vector3& size)
{
	memory->write<math::vector3>(this->address + Offsets::Primitive::Size, size);
}

bool rbx::primitive_t::get_can_collide()
{
	std::uint64_t primitive = this->address;
	if (!primitive) return false;
	
	std::uint8_t flags = memory->read<std::uint8_t>(primitive + Offsets::Primitive::Flags);
	return (flags & Offsets::PrimitiveFlags::CanCollide) != 0;
}

bool rbx::primitive_t::set_can_collide(bool enable)
{
	std::uint64_t primitive = this->address;
	if (!primitive) return false;
	
	std::uint8_t flags = memory->read<std::uint8_t>(primitive + Offsets::Primitive::Flags);
	
	if (enable)
		flags |= Offsets::PrimitiveFlags::CanCollide;
	else
		flags &= ~Offsets::PrimitiveFlags::CanCollide;
	
	memory->write<std::uint8_t>(primitive + Offsets::Primitive::Flags, flags);
	return enable;
}

bool rbx::primitive_t::set_anchored(bool enable)
{
	std::uint64_t primitive = this->address;
	if (!primitive) return false;
	
	std::uint8_t flags = memory->read<std::uint8_t>(primitive + Offsets::Primitive::Flags);
	
	if (enable)
		flags |= Offsets::PrimitiveFlags::Anchored;
	else
		flags &= ~Offsets::PrimitiveFlags::Anchored;
	
	memory->write<std::uint8_t>(primitive + Offsets::Primitive::Flags, flags);
	return enable;
}

math::vector3 rbx::primitive_t::get_position()
{
	return memory->read<math::vector3>(this->address + Offsets::Primitive::Position);
}

math::matrix3 rbx::primitive_t::get_rotation()
{
	return memory->read<math::matrix3>(this->address + Offsets::Primitive::Rotation);
}

math::vector3 rbx::primitive_t::get_velocity()
{
	return memory->read<math::vector3>(this->address + Offsets::Primitive::AssemblyLinearVelocity);
}

math::vector2 rbx::visualengine_t::get_dimensions()
{
	return memory->read<math::vector2>(this->address + Offsets::VisualEngine::Dimensions);
}

math::matrix4 rbx::visualengine_t::get_viewmatrix()
{
	return memory->read<math::matrix4>(this->address + Offsets::VisualEngine::ViewMatrix);
}

bool rbx::visualengine_t::world_to_screen(const math::vector3& world, math::vector2& out, const math::vector2& dims, const math::matrix4& view)
{
	math::vector4 clip = view.multiply({ world.x, world.y, world.z, 1.0f });

	if (clip.w < 0.1f)
	{
		return false;
	}

	clip.x /= clip.w;
	clip.y /= clip.w;

	out.x = (dims.x * 0.5f * clip.x) + (dims.x * 0.5f);
	out.y = -(dims.y * 0.5f * clip.y) + (dims.y * 0.5f);

	HWND roblox_window = game::wnd;
	if (roblox_window)
	{
		RECT client_rect{};
		POINT client_pos{};
		if (GetClientRect(roblox_window, &client_rect))
		{
			client_pos.x = client_rect.left;
			client_pos.y = client_rect.top;
			ClientToScreen(roblox_window, &client_pos);
			out.x += (float)client_pos.x;
			out.y += (float)client_pos.y;
		}
	}

	return true;
}

bool rbx::visualengine_t::world_to_client(const math::vector3& world, math::vector2& out, const math::vector2& dims, const math::matrix4& view)
{
	math::vector4 clip = view.multiply({ world.x, world.y, world.z, 1.0f });

	if (clip.w < 0.1f)
	{
		return false;
	}

	clip.x /= clip.w;
	clip.y /= clip.w;

	out.x = (dims.x * 0.5f * clip.x) + (dims.x * 0.5f);
	out.y = -(dims.y * 0.5f * clip.y) + (dims.y * 0.5f);

	return true;
}

math::vector3 rbx::camera_t::get_position()
{
	return memory->read<math::vector3>(this->address + Offsets::Camera::Position);
}

math::matrix3 rbx::camera_t::get_rotation()
{
	return memory->read<math::matrix3>(this->address + Offsets::Camera::Rotation);
}

void rbx::camera_t::write_rotation(const math::matrix3& rotation)
{
	memory->write<math::matrix3>(this->address + Offsets::Camera::Rotation, rotation);
}

std::unordered_map<std::string, uintptr_t*>& Offsets::GetRegistry() {
    static std::unordered_map<std::string, uintptr_t*> registry;
    return registry;
}

Offset::Offset(const char* path, uintptr_t val) : value(val) {
    Offsets::GetRegistry()[path] = &value;
}

#include <winhttp.h>
#include <fstream>
#include <sstream>
#pragma comment(lib, "winhttp.lib")

static std::string trim_str(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool Offsets::Update(const std::string& current_version) {
    if (current_version.empty()) {
        printf("[Offsets] Warning: Roblox version is empty. Skipping auto-update.\n");
        return false;
    }

    if (current_version == ClientVersion) {
        printf("[Offsets] Offsets are up-to-date with current Roblox version (%s).\n", current_version.c_str());
        return true;
    }

    printf("[Offsets] Roblox update detected! (Compiled: %s | Running: %s)\n", ClientVersion.c_str(), current_version.c_str());

    static std::string ram_cache_ver = "";
    static std::string ram_cache_content = "";

    std::string hpp_content;
    bool cache_valid = false;

    if (ram_cache_ver == current_version && !ram_cache_content.empty()) {
        printf("[Offsets] Loading offsets from in-memory RAM cache...\n");
        hpp_content = ram_cache_content;
        cache_valid = true;
    }

    if (!cache_valid) {
        printf("[Offsets] Fetching latest offsets from imtheo.lol...\n");
        
        HINTERNET hSession = WinHttpOpen(L"LOWLIFE-Updater/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        
        if (hSession) {
            HINTERNET hConnect = WinHttpConnect(hSession, L"offsets.imtheo.lol", INTERNET_DEFAULT_HTTPS_PORT, 0);
            if (hConnect) {
                HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/Offsets.hpp",
                    NULL, WINHTTP_NO_REFERER,
                    WINHTTP_DEFAULT_ACCEPT_TYPES,
                    WINHTTP_FLAG_SECURE);
                
                if (hRequest) {
                    DWORD dwFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                                    SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
                                    SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                                    SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
                    WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));

                    BOOL bResults = WinHttpSendRequest(hRequest,
                        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
                    
                    if (bResults) {
                        bResults = WinHttpReceiveResponse(hRequest, NULL);
                    }
                    
                    if (bResults) {
                        DWORD dwSize = 0;
                        do {
                            dwSize = 0;
                            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                            if (dwSize == 0) break;
                            
                            std::string buffer(dwSize, '\0');
                            DWORD dwDownloaded = 0;
                            if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) {
                                buffer.resize(dwDownloaded);
                                hpp_content += buffer;
                            } else {
                                break;
                            }
                        } while (dwSize > 0);
                    }
                    WinHttpCloseHandle(hRequest);
                }
                WinHttpCloseHandle(hConnect);
            }
            WinHttpCloseHandle(hSession);
        }

        if (hpp_content.empty()) {
            printf("[Offsets] Error: Failed to fetch offsets from server. Using compile-time defaults.\n");
            return false;
        }

        // Cache in RAM memory
        ram_cache_ver = current_version;
        ram_cache_content = hpp_content;
        printf("[Offsets] Successfully cached offsets in RAM.\n");
    }

    
    std::stringstream ss(hpp_content);
    std::string line;
    std::string current_namespace = "";
    int updated_count = 0;

    auto& registry = GetRegistry();

    while (std::getline(ss, line)) {
        line = trim_str(line);
        if (line.empty()) continue;

        
        if (line.rfind("namespace ", 0) == 0) {
            size_t open_brace = line.find('{');
            if (open_brace != std::string::npos) {
                std::string ns = trim_str(line.substr(10, open_brace - 10));
                if (ns != "Offsets") {
                    current_namespace = ns;
                }
            }
            continue;
        }

        
        if (line == "}") {
            current_namespace = "";
            continue;
        }

        
        if (line.find("ClientVersion") != std::string::npos) {
            size_t first_quote = line.find('"');
            size_t last_quote = line.rfind('"');
            if (first_quote != std::string::npos && last_quote != std::string::npos && last_quote > first_quote) {
                std::string ver = line.substr(first_quote + 1, last_quote - first_quote - 1);
                ClientVersion = ver;
            }
            continue;
        }

        
        if (line.find("inline constexpr uintptr_t") != std::string::npos ||
            line.find("inline constexpr std::uintptr_t") != std::string::npos) {
            
            size_t type_pos = line.find("uintptr_t");
            if (type_pos == std::string::npos) continue;

            std::string after_type = trim_str(line.substr(type_pos + 9));
            size_t eq_pos = after_type.find('=');
            if (eq_pos == std::string::npos) continue;

            std::string var_name = trim_str(after_type.substr(0, eq_pos));
            std::string val_str = trim_str(after_type.substr(eq_pos + 1));

            
            if (!val_str.empty() && val_str.back() == ';') {
                val_str.pop_back();
                val_str = trim_str(val_str);
            }

            uintptr_t value = 0;
            try {
                if (val_str.rfind("0x", 0) == 0 || val_str.rfind("0X", 0) == 0) {
                    value = std::stoull(val_str, nullptr, 16);
                } else {
                    value = std::stoull(val_str, nullptr, 10);
                }
            } catch (...) {
                continue;
            }

            std::string registry_key = current_namespace.empty() ? var_name : (current_namespace + "::" + var_name);
            auto it = registry.find(registry_key);
            if (it != registry.end()) {
                *(it->second) = value;
                updated_count++;
            }
        }
    }

    
    if (ClientVersion != current_version) {
        printf("\n==================================================\n");
        printf("[ERROR] Roblox version mismatch detected!\n");
        printf("        Running Roblox Version:  %s\n", current_version.c_str());
        printf("        Server Offsets Version:  %s\n", ClientVersion.c_str());
        printf("        Please wait until the offsets are updated on the server,\n");
        printf("        or make sure your Roblox client is fully updated.\n");
        printf("==================================================\n\n");
        
        MessageBoxA(NULL, 
            ("Roblox version mismatch detected!\n\nRunning Roblox Version:\n" + current_version + "\n\nServer Offsets Version:\n" + ClientVersion + "\n\nPlease ensure your Roblox is updated, or wait for the server offsets to be updated.").c_str(), 
            "LOWLIFE - Version Mismatch", 
            MB_OK | MB_ICONERROR);
            
        return false;
    }

    printf("[Offsets] Parsing finished. Successfully updated %d offsets to match Roblox version %s!\n", updated_count, current_version.c_str());
    return true;
}