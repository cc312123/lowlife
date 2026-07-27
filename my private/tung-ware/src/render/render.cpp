#define IMGUI_DEFINE_MATH_OPERATORS
#define NOMINMAX
#include <render/render.h>
#include <mutex>
#include <atomic>
#include <ctime>
#include "render_helpers.h"
#include "notifications.h"
#include <dwmapi.h>
#include <cstdio>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <cstring>
#include <windows.h>
#include <shellapi.h>
#include <fstream>

#ifndef WDA_NONE
#define WDA_NONE 0x00000000
#endif
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

typedef BOOL(WINAPI* SetWindowDisplayAffinityProc)(HWND, DWORD);

#include <settings.h>
#include <check/typing_check.h>
#include <features/esp/esp.h>
#include <features/aimbot/aimbot.h>
#include <features/explorer/dex_explorer.h>
#include "visitor.h"
#include "../resources/WeaponIcon.hpp"
#include "../config/config.h"
#include <features/triggerbot/triggerbot.h>
#include <features/silent/silent.h>
#include <memory/memory.h>
#include <sdk/offsets.h>
#include <game/rescan.h>
#include <game/teleport.h>
#include "../auth/keyauth_init.h"
#include "../../keyauth/keyauth.hpp"
#include "../../keyauth/utils.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef LONG NTSTATUS;
typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

static bool IsWindows11OrGreater()
{
    HMODULE hMod = GetModuleHandleA("ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (pRtlGetVersion) {
            RTL_OSVERSIONINFOW osvi = { 0 };
            osvi.dwOSVersionInfoSize = sizeof(osvi);
            if (pRtlGetVersion(&osvi) == 0) {
                return (osvi.dwMajorVersion > 10) ||
                       (osvi.dwMajorVersion == 10 && osvi.dwBuildNumber >= 22000);
            }
        }
    }
    return false;
}

static void configure_window_transparency(HWND hwnd, bool menu_open)
{
    
    
    
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    
    
    
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
}

struct CleanerLogEvent {
    std::string timestamp;
    std::string level; 
    std::string message;
    long long duration_us; 
};

static std::vector<CleanerLogEvent> cleaner_log_events;
static std::mutex cleaner_log_mtx;
static std::atomic<bool> is_cleaner_running{ false };
static std::atomic<int> cleaned_files_count{ 0 };
static std::atomic<int> cleaned_keys_count{ 0 };
static std::atomic<int> cleaned_events_count{ 0 };
static std::atomic<float> cleanup_speed_ms{ 0.0f };
static std::atomic<bool> cleanup_completed_successfully{ false };

static void add_cleaner_log(const std::string& level, const std::string& message, long long duration_us = 0) {
    std::lock_guard<std::mutex> lock(cleaner_log_mtx);
    time_t now = time(nullptr);
    tm ltm;
    localtime_s(&ltm, &now);
    char time_str[16];
    sprintf_s(time_str, "%02d:%02d:%02d", ltm.tm_hour, ltm.tm_min, ltm.tm_sec);

    CleanerLogEvent evt;
    evt.timestamp = time_str;
    evt.level = level;
    evt.message = message;
    evt.duration_us = duration_us;
    cleaner_log_events.push_back(evt);

    
    if (cleaner_log_events.size() > 150) {
        cleaner_log_events.erase(cleaner_log_events.begin());
    }
}

static void clean_directory_contents(const std::string& dir_path, int& files_deleted) {
    std::string search_path = dir_path + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
                std::string file_path = dir_path + "\\" + fd.cFileName;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    clean_directory_contents(file_path, files_deleted);
                    RemoveDirectoryA(file_path.c_str());
                } else {
                    SetFileAttributesA(file_path.c_str(), FILE_ATTRIBUTE_NORMAL);
                    if (DeleteFileA(file_path.c_str())) {
                        files_deleted++;
                    }
                }
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
}

static void clear_win_event_log(const char* log_name, int& logs_cleared) {
    HANDLE hEventLog = OpenEventLogA(NULL, log_name);
    if (hEventLog) {
        if (ClearEventLogA(hEventLog, NULL)) {
            logs_cleared++;
        }
        CloseEventLog(hEventLog);
    }
}

static bool log_contains_traces(const wchar_t* w_log_name) {
    bool has_traces = false;
    HMODULE hWevtapi = LoadLibraryA("wevtapi.dll");
    if (!hWevtapi) return false;

    auto pEvtQuery = (HANDLE(WINAPI*)(HANDLE, LPCWSTR, LPCWSTR, DWORD))GetProcAddress(hWevtapi, "EvtQuery");
    auto pEvtNext = (BOOL(WINAPI*)(HANDLE, DWORD, HANDLE*, DWORD, DWORD, DWORD*))GetProcAddress(hWevtapi, "EvtNext");
    auto pEvtRender = (BOOL(WINAPI*)(HANDLE, HANDLE, DWORD, DWORD, PVOID, DWORD*, DWORD*))GetProcAddress(hWevtapi, "EvtRender");
    auto pEvtClose = (BOOL(WINAPI*)(HANDLE))GetProcAddress(hWevtapi, "EvtClose");

    if (pEvtQuery && pEvtNext && pEvtRender && pEvtClose) {
        
        HANDLE hQuery = pEvtQuery(NULL, w_log_name, NULL, 0x201);
        if (hQuery) {
            HANDLE hEvents[50];
            DWORD dwReturned = 0;
            if (pEvtNext(hQuery, 50, hEvents, INFINITE, 0, &dwReturned)) {
                for (DWORD i = 0; i < dwReturned; i++) {
                    if (has_traces) {
                        pEvtClose(hEvents[i]);
                        continue;
                    }

                    DWORD dwBufferUsed = 0;
                    DWORD dwPropertyCount = 0;
                    
                    pEvtRender(NULL, hEvents[i], 1, 0, NULL, &dwBufferUsed, &dwPropertyCount);
                    if (dwBufferUsed > 0) {
                        std::vector<wchar_t> buffer(dwBufferUsed);
                        if (pEvtRender(NULL, hEvents[i], 1, dwBufferUsed, &buffer[0], &dwBufferUsed, &dwPropertyCount)) {
                            std::wstring xml_str(&buffer[0]);
                            
                            
                            for (auto& c : xml_str) {
                                if (c >= L'A' && c <= L'Z') {
                                    c = c - L'A' + L'a';
                                }
                            }
                            
                            
                            std::wstring search_terms[] = { L"tung-ware", L"robloxplayerbeta", L"delta", L"b332fdc6" };
                            for (const auto& term : search_terms) {
                                if (xml_str.find(term) != std::wstring::npos) {
                                    has_traces = true;
                                    break;
                                }
                            }
                        }
                    }
                    pEvtClose(hEvents[i]);
                }
            }
            pEvtClose(hQuery);
        }
    }
    FreeLibrary(hWevtapi);
    return has_traces;
}

static void run_async_cpp_cleaner(bool slow_transition = false, bool is_continuous_loop = false) {
    is_cleaner_running = true;
    cleaned_files_count = 0;
    cleaned_keys_count = 0;
    cleaned_events_count = 0;
    cleanup_speed_ms = 0.0f;
    cleanup_completed_successfully = false;

    auto start_total = std::chrono::high_resolution_clock::now();

    add_cleaner_log("INFO", "Initializing Advanced System Cleaner Engine...");
    if (slow_transition) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300)); 
    }

    
    if (settings::cleaner::clean_registry) {
        add_cleaner_log("INFO", "Wiping Registry traces of execution history...");
        auto start_step = std::chrono::high_resolution_clock::now();

        const char* reg_keys[] = {
            "Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache",
            "Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\Bags",
            "Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\BagMRU",
            "Software\\Microsoft\\Windows\\Shell\\Bags",
            "Software\\Microsoft\\Windows\\Shell\\BagMRU",
            "Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Compatibility Assistant\\Store",
            "Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Compatibility Assistant\\Persisted",
            "Software\\Microsoft\\Windows\\ShellNoRoam\\MUICache",
            "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\OpenSavePidlMRU",
            "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\LastVisitedPidlMRU",
            "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\LastVisitedPidlMRULegacy",
            "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\OpenSaveMRU",
            "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist"
        };

        int step_keys_deleted = 0;
        for (const char* subkey : reg_keys) {
            auto sub_start = std::chrono::high_resolution_clock::now();
            LSTATUS status = RegDeleteTreeA(HKEY_CURRENT_USER, subkey);
            auto sub_end = std::chrono::high_resolution_clock::now();
            long long duration = std::chrono::duration_cast<std::chrono::microseconds>(sub_end - sub_start).count();

            if (status == ERROR_SUCCESS) {
                step_keys_deleted++;
                cleaned_keys_count++;
                if (settings::cleaner::show_details) {
                    char msg[256];
                    sprintf_s(msg, "Deleted key: HKCU\\%s", subkey);
                    add_cleaner_log("SUCCESS", msg, duration);
                }
            } else if (status != ERROR_FILE_NOT_FOUND) {
                if (settings::cleaner::show_details) {
                    char msg[256];
                    sprintf_s(msg, "Failed to delete key: HKCU\\%s (Error %ld)", subkey, status);
                    add_cleaner_log("WARNING", msg, duration);
                }
            }
        }

        auto end_step = std::chrono::high_resolution_clock::now();
        long long duration_step = std::chrono::duration_cast<std::chrono::milliseconds>(end_step - start_step).count();
        char summary[128];
        sprintf_s(summary, "Registry clean completed: Wiped %d keys in %lld ms.", step_keys_deleted, duration_step);
        add_cleaner_log("SUCCESS", summary);
        if (slow_transition) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    
    if (settings::cleaner::clean_temp) {
        add_cleaner_log("INFO", "Wiping System Temp File residues...");
        auto start_step = std::chrono::high_resolution_clock::now();

        char temp_path[MAX_PATH];
        DWORD path_len = GetTempPathA(MAX_PATH, temp_path);
        int step_files_deleted = 0;

        if (path_len > 0 && path_len < MAX_PATH) {
            std::string temp_dir(temp_path);
            if (!temp_dir.empty() && temp_dir.back() == '\\') {
                temp_dir.pop_back();
            }
            
            clean_directory_contents(temp_dir, step_files_deleted);
            cleaned_files_count += step_files_deleted;
        }

        auto end_step = std::chrono::high_resolution_clock::now();
        long long duration_step = std::chrono::duration_cast<std::chrono::milliseconds>(end_step - start_step).count();
        char summary[128];
        sprintf_s(summary, "Temp files clean completed: Deleted %d file(s) in %lld ms.", step_files_deleted, duration_step);
        add_cleaner_log("SUCCESS", summary);
        if (slow_transition) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    
    if (settings::cleaner::clean_prefetch) {
        add_cleaner_log("INFO", "Wiping Windows Prefetch & Recent Items traces...");
        auto start_step = std::chrono::high_resolution_clock::now();

        int step_files_deleted = 0;

        char user_profile[MAX_PATH];
        size_t env_size;
        getenv_s(&env_size, user_profile, MAX_PATH, "USERPROFILE");
        if (env_size > 0) {
            std::string recent_path = std::string(user_profile) + "\\AppData\\Roaming\\Microsoft\\Windows\\Recent";
            clean_directory_contents(recent_path, step_files_deleted);
        }

        clean_directory_contents("C:\\Users\\Default\\AppData\\Roaming\\Microsoft\\Windows\\Recent", step_files_deleted);

        std::string prefetch_dir = "C:\\Windows\\Prefetch";
        std::string search_path = prefetch_dir + "\\*";
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(search_path.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
                    std::string file_name(fd.cFileName);
                    std::string lower_file_name = file_name;
                    std::transform(lower_file_name.begin(), lower_file_name.end(), lower_file_name.begin(), ::tolower);

                    
                    if (lower_file_name.find("roblox") != std::string::npos ||
                        lower_file_name.find("crash") != std::string::npos ||
                        lower_file_name.find("tung-ware") != std::string::npos ||
                        lower_file_name.find("loader") != std::string::npos ||
                        lower_file_name.find("injector") != std::string::npos ||
                        lower_file_name.find("cleaner") != std::string::npos ||
                        lower_file_name.find("setup") != std::string::npos ||
                        lower_file_name.find("powershell") != std::string::npos ||
                        lower_file_name.find("msbuild") != std::string::npos ||
                        lower_file_name.find("dllhost") != std::string::npos ||
                        lower_file_name.find("dll.host") != std::string::npos ||
                        (lower_file_name.find("ag") == 0 && lower_file_name.find(".db") != std::string::npos)) { 
                        
                        std::string file_path = prefetch_dir + "\\" + fd.cFileName;
                        SetFileAttributesA(file_path.c_str(), FILE_ATTRIBUTE_NORMAL);
                        if (DeleteFileA(file_path.c_str())) {
                            step_files_deleted++;
                            if (settings::cleaner::show_details) {
                                char msg[256];
                                sprintf_s(msg, "Deleted Prefetch/Superfetch trace: %s", fd.cFileName);
                                add_cleaner_log("SUCCESS", msg);
                            }
                        }
                    }
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }

        
        std::string rb_search = prefetch_dir + "\\ReadyBoot\\*";
        HANDLE hRb = FindFirstFileA(rb_search.c_str(), &fd);
        if (hRb != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
                    std::string rb_path = prefetch_dir + "\\ReadyBoot\\" + fd.cFileName;
                    SetFileAttributesA(rb_path.c_str(), FILE_ATTRIBUTE_NORMAL);
                    if (DeleteFileA(rb_path.c_str())) {
                        step_files_deleted++;
                    }
                }
            } while (FindNextFileA(hRb, &fd));
            FindClose(hRb);
        }

        cleaned_files_count += step_files_deleted;

        auto end_step = std::chrono::high_resolution_clock::now();
        long long duration_step = std::chrono::duration_cast<std::chrono::milliseconds>(end_step - start_step).count();
        char summary[128];
        sprintf_s(summary, "Prefetch & Recent clean completed: Wiped %d traces in %lld ms.", step_files_deleted, duration_step);
        add_cleaner_log("SUCCESS", summary);
        if (slow_transition) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    
    if (settings::cleaner::clean_eventlogs && !is_continuous_loop) {
        add_cleaner_log("INFO", "Executing Smart Event Log Wiping (Preserving Major Logs)...");
        auto start_step = std::chrono::high_resolution_clock::now();

        int step_logs_cleared = 0;

        
        
        const char* hidden_logs[] = {
            "Microsoft-Windows-PowerShell/Operational",
            "Microsoft-Windows-TaskScheduler/Operational",
            "Microsoft-Windows-TerminalServices-LocalSessionManager/Operational",
            "Microsoft-Windows-Windows Defender/Operational",
            "Microsoft-Windows-Windows Defender/WHC",
            "Microsoft-Windows-Application-Experience/Program-Telemetry",
            "Microsoft-Windows-Application-Experience/Program-Inventory",
            "Microsoft-Windows-Application-Experience/Program-Compatibility-Assistant",
            "Microsoft-Windows-WMI-Activity/Operational"
        };

        HMODULE hWevtapi = LoadLibraryA("wevtapi.dll");
        if (hWevtapi) {
            auto pEvtClearLog = (BOOL(WINAPI*)(HANDLE, LPCWSTR, LPCWSTR, DWORD))GetProcAddress(hWevtapi, "EvtClearLog");
            if (pEvtClearLog) {
                for (const char* log_name : hidden_logs) {
                    
                    wchar_t w_log_name[256];
                    size_t converted = 0;
                    mbstowcs_s(&converted, w_log_name, log_name, _TRUNCATE);

                    
                    if (log_contains_traces(w_log_name)) {
                        auto sub_start = std::chrono::high_resolution_clock::now();

                        pEvtClearLog(NULL, w_log_name, NULL, 0);

                        auto sub_end = std::chrono::high_resolution_clock::now();
                        long long duration = std::chrono::duration_cast<std::chrono::microseconds>(sub_end - sub_start).count();

                        step_logs_cleared++;
                        cleaned_events_count++;
                        if (settings::cleaner::show_details) {
                            char msg[256];
                            sprintf_s(msg, "Stealth Cleared Operational Log: %s", log_name);
                            add_cleaner_log("SUCCESS", msg, duration);
                        }
                    }
                }
            }
            FreeLibrary(hWevtapi);
        }

        auto end_step = std::chrono::high_resolution_clock::now();
        long long duration_step = std::chrono::duration_cast<std::chrono::milliseconds>(end_step - start_step).count();
        char summary[128];
        sprintf_s(summary, "Event Logs clean completed: Cleared %d operational log(s) in %lld ms.", step_logs_cleared, duration_step);
        add_cleaner_log("SUCCESS", summary);
        if (slow_transition) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    auto end_total = std::chrono::high_resolution_clock::now();
    long long duration_total = std::chrono::duration_cast<std::chrono::milliseconds>(end_total - start_total).count();
    cleanup_speed_ms = (float)duration_total;

    add_cleaner_log("SUCCESS", "==================================================");
    char total_summary[256];
    sprintf_s(total_summary, "SYSTEM OPTIMIZED: Removed %d files, %d registry traces, %d event logs.", 
        cleaned_files_count.load(), cleaned_keys_count.load(), cleaned_events_count.load());
    add_cleaner_log("SUCCESS", total_summary);
    
    char speed_summary[128];
    sprintf_s(speed_summary, "Total Optimization Time: %.2f ms (FAST)", cleanup_speed_ms.load());
    add_cleaner_log("SUCCESS", speed_summary);
    add_cleaner_log("SUCCESS", "==================================================");

    cleanup_completed_successfully = true;
    is_cleaner_running = false;
}

static std::atomic<bool> continuous_cleaner_should_exit{ false };

static void run_continuous_cleaner_loop() {
    
    for (int i = 0; i < 20; ++i) {
        if (continuous_cleaner_should_exit) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    while (!continuous_cleaner_should_exit) {
        if (!is_cleaner_running) {
            run_async_cpp_cleaner(false, true);
        }
        for (int i = 0; i < 100; ++i) {
            if (continuous_cleaner_should_exit) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

static int selected_tab_index = 0;
static bool test_label_1 = false;
static bool test_label_2 = false;
static bool test_label_3 = false;
static bool test_label_4 = false;
static float niggerKyzo = 0.0f;
static float test_slider = 0.0f;
static float color_array[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static bool waiting_for_keybind = false;
static std::unordered_map<std::string, bool> waiting_for_keybind_map;

static const char* get_key_name(int vk_code)
{
    switch (vk_code)
    {
    case 0: return "None";
    case VK_LBUTTON: return "LM";
    case VK_RBUTTON: return "RM";
    case VK_MBUTTON: return "MM";
    case VK_XBUTTON1: return "MB1";
    case VK_XBUTTON2: return "MB2";
    case VK_BACK: return "Backspace";
    case VK_TAB: return "Tab";
    case VK_RETURN: return "Enter";
    case VK_SHIFT: return "Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_MENU: return "Alt";
    case VK_CAPITAL: return "Caps";
    case VK_ESCAPE: return "Esc";
    case VK_SPACE: return "Space";
    case VK_PRIOR: return "PgUp";
    case VK_NEXT: return "PgDown";
    case VK_END: return "End";
    case VK_HOME: return "Home";
    case VK_LEFT: return "Left";
    case VK_UP: return "Up";
    case VK_RIGHT: return "Right";
    case VK_DOWN: return "Down";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_F1: return "F1";
    case VK_F2: return "F2";
    case VK_F3: return "F3";
    case VK_F4: return "F4";
    case VK_F5: return "F5";
    case VK_F6: return "F6";
    case VK_F7: return "F7";
    case VK_F8: return "F8";
    case VK_F9: return "F9";
    case VK_F10: return "F10";
    case VK_F11: return "F11";
    case VK_F12: return "F12";
    default:
        if (vk_code >= 'A' && vk_code <= 'Z') { static char buf[2]; buf[0] = (char)vk_code; buf[1] = 0; return buf; }
        if (vk_code >= '0' && vk_code <= '9') { static char buf[2]; buf[0] = (char)vk_code; buf[1] = 0; return buf; }
        return "None";
    }
}

static std::unordered_map<std::string, float> keybind_tween_progress;
static std::unordered_map<std::string, bool> keybind_popup_open;
static std::unordered_map<std::string, ImVec2> keybind_popup_pos;
static std::unordered_map<std::string, bool> multiselect_popup_open;
static std::unordered_map<std::string, ImVec2> multiselect_popup_pos;

static bool inline_keybind_button(const char* label, int* key, int* mode = nullptr)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    std::string key_id = label;
    if (waiting_for_keybind_map.find(key_id) == waiting_for_keybind_map.end())
        waiting_for_keybind_map[key_id] = false;

    bool is_waiting = waiting_for_keybind_map[key_id];
    const char* display_text;
    if (is_waiting)
        display_text = "..";
    else if (*key == 0)
        display_text = "-";
    else
        display_text = get_key_name(*key);

    ImVec2 label_size = ImGui::CalcTextSize(display_text, nullptr, true);
    float checkbox_height = ImGui::GetFrameHeight();
    ImVec2 button_size = ImVec2(label_size.x + style.FramePadding.x * 2.0f + 16.0f, checkbox_height);
    if (button_size.x < 40.0f) button_size.x = 40.0f;

    ImVec2 pos = window->DC.CursorPos;

    ImRect bb(pos, ImVec2(pos.x + button_size.x, pos.y + button_size.y));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, window->GetID(label)))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, window->GetID(label), &hovered, &held);

    if (keybind_popup_open.find(key_id) == keybind_popup_open.end())
        keybind_popup_open[key_id] = false;

    if (mode != nullptr && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        keybind_popup_open[key_id] = !keybind_popup_open[key_id];
        keybind_popup_pos[key_id] = ImVec2(pos.x + button_size.x + 5, pos.y);
    }

    if (pressed)
        waiting_for_keybind_map[key_id] = !waiting_for_keybind_map[key_id];

    is_waiting = waiting_for_keybind_map[key_id];
    if (is_waiting)
    {
        for (int i = 1; i < 256; i++)
        {
            if (i == VK_ESCAPE)
            {
                if (GetAsyncKeyState(i) & 0x8000)
                {
                    *key = 0;
                    waiting_for_keybind_map[key_id] = false;
                    break;
                }
                continue;
            }

            if (GetAsyncKeyState(i) & 0x8000)
            {
                *key = i;
                waiting_for_keybind_map[key_id] = false;
                break;
            }
        }
    }

    if (keybind_tween_progress.find(key_id) == keybind_tween_progress.end())
        keybind_tween_progress[key_id] = 0.0f;

    float& tween_progress = keybind_tween_progress[key_id];

    ImVec4 base_color = ImVec4(60.0f / 255.0f, 60.0f / 255.0f, 60.0f / 255.0f, 1.0f);
    ImVec4 target_color = menu::accent_color;

    float target_progress = 0.0f;
    if (is_waiting || held)
        target_progress = 1.0f;
    else if (hovered)
        target_progress = 0.5f;

    float tween_speed = 4.0f;
    tween_progress += (target_progress - tween_progress) * g.IO.DeltaTime * tween_speed;

    ImVec4 tweened_color = ImVec4(
        base_color.x + (target_color.x - base_color.x) * tween_progress,
        base_color.y + (target_color.y - base_color.y) * tween_progress,
        base_color.z + (target_color.z - base_color.z) * tween_progress,
        base_color.w + (target_color.w - base_color.w) * tween_progress
    );

    ImDrawList* dl = ImGui::GetWindowDrawList();

    char bracket_text[64];
    sprintf_s(bracket_text, "[ %s ]", display_text);
    ImVec2 bracket_text_size = ImGui::CalcTextSize(bracket_text);
    float text_y = pos.y + style.FramePadding.y - 4.0f;
    ImVec2 text_pos = ImVec2(bb.Min.x + (button_size.x - bracket_text_size.x) * 0.5f, text_y);

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            if (x == 0 && y == 0) continue;
            dl->AddText(ImVec2(text_pos.x + x * 1.f, text_pos.y + y * 1.f), IM_COL32_BLACK, bracket_text);
        }
    }
    dl->AddText(text_pos, IM_COL32_WHITE, bracket_text);

    if (mode != nullptr && keybind_popup_open[key_id])
    {
        ImGui::SetNextWindowPos(keybind_popup_pos[key_id], ImGuiCond_Always);

        bool is_walkspeed = (strcmp(label, "walkspeed_keybind") == 0);
        bool is_jumppower = (strcmp(label, "jumppower_keybind") == 0);
        int mode_count = (is_walkspeed || is_jumppower) ? 3 : 2;
        ImVec2 popup_size = ImVec2(80.0f, mode_count == 3 ? 100.0f : 80.0f);
        ImGui::SetNextWindowSize(popup_size);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(25, 25, 25, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertFloat4ToU32(menu::accent_color));

        char popup_name[128];
        sprintf_s(popup_name, "##keybind_mode_%s", label);

        ImGui::Begin(popup_name, nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_AlwaysAutoResize);

        ImVec2 popup_pos = ImGui::GetWindowPos();
        popup_size = ImGui::GetWindowSize();
        ImDrawList* popup_dl = ImGui::GetWindowDrawList();

        const char* modes[3];
        if (is_walkspeed || is_jumppower)
        {
            modes[0] = "Hold";
            modes[1] = "Toggle";
            modes[2] = "Always";
        }
        else
        {
            modes[0] = "Hold";
            modes[1] = "Toggle";
        }

        for (int i = 0; i < mode_count; i++)
        {
            ImVec2 item_pos = ImGui::GetCursorScreenPos();
            ImVec2 item_size = ImVec2(popup_size.x - 12, 20);
            ImRect item_bb(item_pos, ImVec2(item_pos.x + item_size.x, item_pos.y + item_size.y));

            bool item_hovered = ImGui::IsMouseHoveringRect(item_bb.Min, item_bb.Max);
            bool item_clicked = item_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

            ImU32 item_bg = (*mode == i) ? ImGui::ColorConvertFloat4ToU32(ImVec4(menu::accent_color.x * 0.3f, menu::accent_color.y * 0.3f, menu::accent_color.z * 0.3f, 1.0f)) :
                (item_hovered ? IM_COL32(45, 45, 45, 255) : IM_COL32(35, 35, 35, 255));
            popup_dl->AddRectFilled(item_bb.Min, item_bb.Max, item_bg);

            ImVec2 mode_text_size = ImGui::CalcTextSize(modes[i]);
            ImVec2 mode_text_pos = ImVec2(item_bb.Min.x + (item_size.x - mode_text_size.x) * 0.5f, item_bb.Min.y + (item_size.y - mode_text_size.y) * 0.5f);

            for (int x = -1; x <= 1; x++) {
                for (int y = -1; y <= 1; y++) {
                    if (x == 0 && y == 0) continue;
                    popup_dl->AddText(ImVec2(mode_text_pos.x + x, mode_text_pos.y + y), IM_COL32(0, 0, 0, 255), modes[i]);
                }
            }

            ImU32 text_col = (*mode == i) ? ImGui::ColorConvertFloat4ToU32(menu::accent_color) : IM_COL32(255, 255, 255, 255);
            popup_dl->AddText(mode_text_pos, text_col, modes[i]);

            if (item_clicked)
            {
                *mode = i;
                keybind_popup_open[key_id] = false;
            }

            ImGui::Dummy(item_size);
        }

        ImVec2 separator_start = ImGui::GetCursorScreenPos();
        separator_start.y += 2;
        ImVec2 separator_end = ImVec2(separator_start.x + popup_size.x - 12, separator_start.y);
        popup_dl->AddLine(separator_start, separator_end, IM_COL32(60, 60, 60, 255));
        ImGui::Dummy(ImVec2(0, 6));

        ImVec2 clear_item_pos = ImGui::GetCursorScreenPos();
        ImVec2 clear_item_size = ImVec2(popup_size.x - 12, 20);
        ImRect clear_item_bb(clear_item_pos, ImVec2(clear_item_pos.x + clear_item_size.x, clear_item_pos.y + clear_item_size.y));

        bool clear_hovered = ImGui::IsMouseHoveringRect(clear_item_bb.Min, clear_item_bb.Max);
        bool clear_clicked = clear_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

        ImU32 clear_bg = clear_hovered ? IM_COL32(45, 45, 45, 255) : IM_COL32(35, 35, 35, 255);
        popup_dl->AddRectFilled(clear_item_bb.Min, clear_item_bb.Max, clear_bg);

        const char* clear_text = "Clear";
        ImVec2 clear_text_size = ImGui::CalcTextSize(clear_text);
        ImVec2 clear_text_pos = ImVec2(clear_item_bb.Min.x + (clear_item_size.x - clear_text_size.x) * 0.5f, clear_item_bb.Min.y + (clear_item_size.y - clear_text_size.y) * 0.5f);

        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                if (x == 0 && y == 0) continue;
                popup_dl->AddText(ImVec2(clear_text_pos.x + x, clear_text_pos.y + y), IM_COL32(0, 0, 0, 255), clear_text);
            }
        }

        popup_dl->AddText(clear_text_pos, IM_COL32(255, 255, 255, 255), clear_text);

        if (clear_clicked)
        {
            *key = 0;
            keybind_popup_open[key_id] = false;
        }

        ImGui::Dummy(clear_item_size);

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }

    return pressed;
}

static bool keybind_button(const char* label, int* key, int* mode = nullptr)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    const char* display_text = waiting_for_keybind ? "..." : get_key_name(*key);

    ImVec2 label_size = ImGui::CalcTextSize(display_text, nullptr, true);
    ImVec2 button_size = ImVec2(label_size.x + style.FramePadding.x * 4.0f, label_size.y + style.FramePadding.y * 2.0f);
    if (button_size.x < 50.0f) button_size.x = 50.0f;

    ImVec2 pos = window->DC.CursorPos;
    pos.x += 2;

    ImRect bb(pos, ImVec2(pos.x + button_size.x, pos.y + button_size.y));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, window->GetID(label)))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, window->GetID(label), &hovered, &held);

    std::string key_id = label;

    if (keybind_popup_open.find(key_id) == keybind_popup_open.end())
        keybind_popup_open[key_id] = false;

    if (mode != nullptr && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        keybind_popup_open[key_id] = !keybind_popup_open[key_id];
        keybind_popup_pos[key_id] = ImVec2(pos.x + button_size.x + 5, pos.y);
    }

    if (pressed)
        waiting_for_keybind = !waiting_for_keybind;

    if (waiting_for_keybind)
    {
        for (int i = 1; i < 256; i++)
        {
            if (i == VK_ESCAPE)
            {
                if (GetAsyncKeyState(i) & 0x8000)
                {
                    *key = 0;
                    waiting_for_keybind = false;
                    break;
                }
                continue;
            }

            if (GetAsyncKeyState(i) & 0x8000)
            {
                *key = i;
                waiting_for_keybind = false;
                break;
            }
        }
    }

    ImU32 col = IM_COL32(35, 35, 35, 255);

    if (keybind_tween_progress.find(key_id) == keybind_tween_progress.end())
        keybind_tween_progress[key_id] = 0.0f;

    float& tween_progress = keybind_tween_progress[key_id];

    ImVec4 base_color = ImVec4(60.0f / 255.0f, 60.0f / 255.0f, 60.0f / 255.0f, 1.0f);
    ImVec4 target_color = menu::accent_color;

    float target_progress = 0.0f;
    if (waiting_for_keybind || held)
        target_progress = 1.0f;
    else if (hovered)
        target_progress = 0.5f;

    float tween_speed = 4.0f;
    tween_progress += (target_progress - tween_progress) * g.IO.DeltaTime * tween_speed;

    ImVec4 tweened_color = ImVec4(
        base_color.x + (target_color.x - base_color.x) * tween_progress,
        base_color.y + (target_color.y - base_color.y) * tween_progress,
        base_color.z + (target_color.z - base_color.z) * tween_progress,
        base_color.w + (target_color.w - base_color.w) * tween_progress
    );

    ImDrawList* dl = ImGui::GetWindowDrawList();

    char bracket_text[64];
    const char* display_text_bracket;
    if (waiting_for_keybind)
        display_text_bracket = "..";
    else if (*key == 0)
        display_text_bracket = "-";
    else
        display_text_bracket = display_text;

    sprintf_s(bracket_text, "[ %s ]", display_text_bracket);
    ImVec2 bracket_text_size = ImGui::CalcTextSize(bracket_text);
    ImVec2 text_pos = ImVec2(bb.Min.x + (button_size.x - bracket_text_size.x) * 0.5f, bb.Min.y + (button_size.y - bracket_text_size.y) * 0.5f);

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            if (x == 0 && y == 0) continue;
            dl->AddText(ImVec2(text_pos.x + x * 1.f, text_pos.y + y * 1.f), IM_COL32_BLACK, bracket_text);
        }
    }
    dl->AddText(text_pos, IM_COL32_WHITE, bracket_text);

    if (mode != nullptr && keybind_popup_open[key_id])
    {
        ImGui::SetNextWindowPos(keybind_popup_pos[key_id], ImGuiCond_Always);

        bool is_walkspeed = (strcmp(label, "walkspeed_keybind") == 0);
        bool is_jumppower = (strcmp(label, "jumppower_keybind") == 0);
        int mode_count = (is_walkspeed || is_jumppower) ? 3 : 2;
        ImVec2 popup_size = ImVec2(80.0f, mode_count == 3 ? 100.0f : 80.0f);
        ImGui::SetNextWindowSize(popup_size);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(25, 25, 25, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertFloat4ToU32(menu::accent_color));

        char popup_name[128];
        sprintf_s(popup_name, "##keybind_mode_%s", label);

        ImGui::Begin(popup_name, nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_AlwaysAutoResize);

        ImVec2 popup_pos = ImGui::GetWindowPos();
        popup_size = ImGui::GetWindowSize();
        ImDrawList* popup_dl = ImGui::GetWindowDrawList();

        const char* modes[3];
        if (is_walkspeed || is_jumppower)
        {
            modes[0] = "Hold";
            modes[1] = "Toggle";
            modes[2] = "Always";
        }
        else
        {
            modes[0] = "Hold";
            modes[1] = "Toggle";
        }

        for (int i = 0; i < mode_count; i++)
        {
            ImVec2 item_pos = ImGui::GetCursorScreenPos();
            ImVec2 item_size = ImVec2(popup_size.x - 12, 20);
            ImRect item_bb(item_pos, ImVec2(item_pos.x + item_size.x, item_pos.y + item_size.y));

            bool item_hovered = ImGui::IsMouseHoveringRect(item_bb.Min, item_bb.Max);
            bool item_clicked = item_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

            ImU32 item_bg = (*mode == i) ? ImGui::ColorConvertFloat4ToU32(ImVec4(menu::accent_color.x * 0.3f, menu::accent_color.y * 0.3f, menu::accent_color.z * 0.3f, 1.0f)) :
                (item_hovered ? IM_COL32(45, 45, 45, 255) : IM_COL32(35, 35, 35, 255));
            popup_dl->AddRectFilled(item_bb.Min, item_bb.Max, item_bg);

            ImVec2 mode_text_size = ImGui::CalcTextSize(modes[i]);
            ImVec2 mode_text_pos = ImVec2(item_bb.Min.x + (item_size.x - mode_text_size.x) * 0.5f, item_bb.Min.y + (item_size.y - mode_text_size.y) * 0.5f);

            for (int x = -1; x <= 1; x++) {
                for (int y = -1; y <= 1; y++) {
                    if (x == 0 && y == 0) continue;
                    popup_dl->AddText(ImVec2(mode_text_pos.x + x, mode_text_pos.y + y), IM_COL32(0, 0, 0, 255), modes[i]);
                }
            }

            ImU32 text_col = (*mode == i) ? ImGui::ColorConvertFloat4ToU32(menu::accent_color) : IM_COL32(255, 255, 255, 255);
            popup_dl->AddText(mode_text_pos, text_col, modes[i]);

            if (item_clicked)
            {
                *mode = i;
                keybind_popup_open[key_id] = false;
            }

            ImGui::Dummy(item_size);
        }

        ImVec2 separator_start = ImGui::GetCursorScreenPos();
        separator_start.y += 2;
        ImVec2 separator_end = ImVec2(separator_start.x + popup_size.x - 12, separator_start.y);
        popup_dl->AddLine(separator_start, separator_end, IM_COL32(60, 60, 60, 255));
        ImGui::Dummy(ImVec2(0, 6));

        ImVec2 clear_item_pos = ImGui::GetCursorScreenPos();
        ImVec2 clear_item_size = ImVec2(popup_size.x - 12, 20);
        ImRect clear_item_bb(clear_item_pos, ImVec2(clear_item_pos.x + clear_item_size.x, clear_item_pos.y + clear_item_size.y));

        bool clear_hovered = ImGui::IsMouseHoveringRect(clear_item_bb.Min, clear_item_bb.Max);
        bool clear_clicked = clear_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

        ImU32 clear_bg = clear_hovered ? IM_COL32(45, 45, 45, 255) : IM_COL32(35, 35, 35, 255);
        popup_dl->AddRectFilled(clear_item_bb.Min, clear_item_bb.Max, clear_bg);

        const char* clear_text = "Clear";
        ImVec2 clear_text_size = ImGui::CalcTextSize(clear_text);
        ImVec2 clear_text_pos = ImVec2(clear_item_bb.Min.x + (clear_item_size.x - clear_text_size.x) * 0.5f, clear_item_bb.Min.y + (clear_item_size.y - clear_text_size.y) * 0.5f);

        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                if (x == 0 && y == 0) continue;
                popup_dl->AddText(ImVec2(clear_text_pos.x + x, clear_text_pos.y + y), IM_COL32(0, 0, 0, 255), clear_text);
            }
        }

        popup_dl->AddText(clear_text_pos, IM_COL32(255, 255, 255, 255), clear_text);

        if (clear_clicked)
        {
            *key = 0;
            keybind_popup_open[key_id] = false;
        }

        ImGui::Dummy(clear_item_size);

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }

    return pressed;
}

static bool styled_button(const char* label, const ImVec2& size = ImVec2(0, 0))
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    ImVec2 button_size = size;
    if (button_size.x == 0) button_size.x = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
    if (button_size.y == 0) button_size.y = ImGui::GetFrameHeight();

    ImVec2 pos = window->DC.CursorPos;
    pos.x += 2;

    ImRect bb(pos, ImVec2(pos.x + button_size.x, pos.y + button_size.y));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, window->GetID(label)))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, window->GetID(label), &hovered, &held);

    static std::unordered_map<std::string, float> button_tween_progress;
    std::string button_id = label;

    if (button_tween_progress.find(button_id) == button_tween_progress.end())
        button_tween_progress[button_id] = 0.0f;

    float& tween_progress = button_tween_progress[button_id];

    ImVec4 base_color = ImVec4(45.0f / 255.0f, 45.0f / 255.0f, 54.0f / 255.0f, 1.0f);
    ImVec4 target_color = menu::accent_color;

    float target_progress = 0.0f;
    if (held)
        target_progress = 1.0f;
    else if (hovered)
        target_progress = 0.5f;

    float tween_speed = 8.0f;
    tween_progress += (target_progress - tween_progress) * g.IO.DeltaTime * tween_speed;

    ImVec4 tweened_color = ImVec4(
        base_color.x + (target_color.x - base_color.x) * tween_progress,
        base_color.y + (target_color.y - base_color.y) * tween_progress,
        base_color.z + (target_color.z - base_color.z) * tween_progress,
        base_color.w + (target_color.w - base_color.w) * tween_progress
    );

    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 bg_col = IM_COL32(30, 30, 36, 255);

    dl->AddRectFilled(bb.Min, bb.Max, bg_col, 5.0f);
    dl->AddRect(bb.Min, bb.Max, ImGui::ColorConvertFloat4ToU32(tweened_color), 5.0f, 0, 1.0f);

    ImVec2 text_size = ImGui::CalcTextSize(label);
    ImVec2 text_pos = ImVec2(bb.Min.x + (button_size.x - text_size.x) * 0.5f, bb.Min.y + (button_size.y - text_size.y) * 0.5f);

    dl->AddText(ImVec2(text_pos.x + 1.f, text_pos.y + 1.f), IM_COL32(0, 0, 0, 180), label);
    dl->AddText(text_pos, IM_COL32_WHITE, label);

    return pressed;
}

static void multiselect_combo(const char* label, bool* fov_check, bool* knocked_check)
{
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiStyle& style = g.Style;

    std::string popup_id = std::string("##multiselect_") + label;
    std::string key_id = std::string(label);

    if (multiselect_popup_open.find(key_id) == multiselect_popup_open.end())
        multiselect_popup_open[key_id] = false;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 button_size = ImVec2(ImGui::GetContentRegionAvail().x - 13.f, 20.f);
    ImRect bb(pos, ImVec2(pos.x + button_size.x, pos.y + button_size.y));

    bool hovered = ImGui::IsMouseHoveringRect(bb.Min, bb.Max);
    bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    if (clicked)
    {
        multiselect_popup_open[key_id] = !multiselect_popup_open[key_id];
        if (multiselect_popup_open[key_id])
        {
            multiselect_popup_pos[key_id] = ImVec2(bb.Min.x, bb.Max.y + 2);
        }
    }

    ImU32 col = hovered ? IM_COL32(45, 45, 45, 255) : IM_COL32(35, 35, 35, 255);

    std::string display_text = "check";
    std::vector<std::string> selected_items;
    if (*fov_check) selected_items.push_back("fov check");
    if (*knocked_check) selected_items.push_back("knocked check");

    if (!selected_items.empty())
    {
        display_text = selected_items[0];
        for (size_t i = 1; i < selected_items.size(); i++)
        {
            display_text += ", " + selected_items[i];
        }
    }

    ImVec2 label_size = ImGui::CalcTextSize(display_text.c_str());
    if (label_size.x > button_size.x - 20)
    {
        display_text = std::to_string(selected_items.size()) + " selected";
        label_size = ImGui::CalcTextSize(display_text.c_str());
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(bb.Min, bb.Max, col, style.FrameRounding);

    ImVec2 text_pos = ImVec2(bb.Min.x + 8, bb.Min.y + (button_size.y - label_size.y) * 0.5f);
    dl->AddText(text_pos, IM_COL32(255, 255, 255, 255), display_text.c_str());

    ImVec2 arrow_pos = ImVec2(bb.Max.x - 15, bb.Min.y + (button_size.y - 8) * 0.5f);
    dl->AddTriangleFilled(
        arrow_pos,
        ImVec2(arrow_pos.x + 8, arrow_pos.y),
        ImVec2(arrow_pos.x + 4, arrow_pos.y + 8),
        IM_COL32(255, 255, 255, 255)
    );

    ImGui::Dummy(button_size);

    if (multiselect_popup_open[key_id])
    {
        ImGui::SetNextWindowPos(multiselect_popup_pos[key_id], ImGuiCond_Always);
        ImVec2 popup_size = ImVec2(button_size.x, 50);
        ImGui::SetNextWindowSize(popup_size);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(24, 24, 28, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertFloat4ToU32(menu::accent_color));

        ImGui::Begin(popup_id.c_str(), nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Checkbox("FOV Check", fov_check);
        ImGui::Checkbox("Knocked Check", knocked_check);

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }

    if (multiselect_popup_open[key_id] && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        ImVec2 popup_min = multiselect_popup_pos[key_id];
        ImVec2 popup_max = ImVec2(popup_min.x + button_size.x, popup_min.y + 50);

        if (!(mouse_pos.x >= popup_min.x && mouse_pos.x <= popup_max.x &&
            mouse_pos.y >= popup_min.y && mouse_pos.y <= popup_max.y) &&
            !(mouse_pos.x >= bb.Min.x && mouse_pos.x <= bb.Max.x &&
                mouse_pos.y >= bb.Min.y && mouse_pos.y <= bb.Max.y))
        {
            multiselect_popup_open[key_id] = false;
        }
    }
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
    {
        return true;
    }

    switch (msg)
    {
    case WM_NCHITTEST:
    {
        if (render && render->running)
        {
            if (render->menu_size.x == 0.0f || render->menu_size.y == 0.0f)
            {
                return HTCLIENT;
            }

            POINT pt;
            pt.x = (int)(short)LOWORD(lParam);
            pt.y = (int)(short)HIWORD(lParam);
            ScreenToClient(hwnd, &pt);

            if (pt.x >= render->menu_pos.x && pt.x <= render->menu_pos.x + render->menu_size.x &&
                pt.y >= render->menu_pos.y && pt.y <= render->menu_pos.y + render->menu_size.y)
            {
                return HTCLIENT;
            }

            if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse)
            {
                return HTCLIENT;
            }
        }
        return HTTRANSPARENT;
    }

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
        {
            return 0;
        }
        break;

    case WM_SYSKEYDOWN:
        if (wParam == VK_F4) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

render_t::render_t()
{
    detail = std::make_unique<detail_t>();
}

render_t::~render_t()
{
    continuous_cleaner_should_exit = true;
    run_async_cpp_cleaner(false, false); 
    destroy_imgui();
    destroy_window();
    destroy_device();
}

bool render_t::create_window()
{
    detail->window_class.cbSize = sizeof(detail->window_class);
    detail->window_class.style = CS_CLASSDC;
    detail->window_class.lpszClassName = "T4";
    detail->window_class.hInstance = GetModuleHandleA(0);
    detail->window_class.lpfnWndProc = wnd_proc;

    RegisterClassExA(&detail->window_class);

    detail->window = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        detail->window_class.lpszClassName,
        "T4",
        WS_POPUP,
        0,
        0,
        GetSystemMetrics(SM_CXSCREEN) - 1,
        GetSystemMetrics(SM_CYSCREEN) - 1,
        0,
        0,
        detail->window_class.hInstance,
        0
    );

    if (!detail->window)
    {
        return false;
    }

    configure_window_transparency(detail->window, running);

    ShowWindow(detail->window, SW_SHOW);
    UpdateWindow(detail->window);

    return true;
}

bool render_t::create_device()
{
    DXGI_SWAP_CHAIN_DESC swap_chain_desc{};

    swap_chain_desc.BufferCount = 1;

    swap_chain_desc.BufferDesc.Width = 0;
    swap_chain_desc.BufferDesc.Height = 0;
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    swap_chain_desc.OutputWindow = detail->window;

    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swap_chain_desc.Windowed = 1;

    swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;

    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

    D3D_FEATURE_LEVEL feature_level;
    D3D_FEATURE_LEVEL feature_level_list[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        feature_level_list,
        2,
        D3D11_SDK_VERSION,
        &swap_chain_desc,
        &detail->swap_chain,
        &detail->device,
        &feature_level,
        &detail->device_context
    );

    if (result == DXGI_ERROR_UNSUPPORTED)
    {
        result = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            0,
            feature_level_list,
            2,
            D3D11_SDK_VERSION,
            &swap_chain_desc,
            &detail->swap_chain,
            &detail->device,
            &feature_level,
            &detail->device_context
        );
    }

    if (result != S_OK)
    {
        MessageBoxA(nullptr, "This software can not run on your computer.", "Critical Problem", MB_ICONERROR | MB_OK);
    }

    ID3D11Texture2D* back_buffer{ nullptr };
    detail->swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));

    if (back_buffer)
    {
        detail->device->CreateRenderTargetView(back_buffer, nullptr, &detail->render_target_view);
        back_buffer->Release();

        return true;
    }

    return false;
}

bool render_t::create_imgui()
{
    using namespace ImGui;
    CreateContext();
    StyleColorsDark();

    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    
    
    style.WindowRounding = 12.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 6.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;

    style.ItemSpacing = ImVec2(8.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    style.FramePadding = ImVec2(8.0f, 4.0f);

    
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.98f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.25f, 0.35f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_CheckMark] = menu::accent_color;
    style.Colors[ImGuiCol_SliderGrab] = menu::accent_color;
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(menu::accent_color.x * 1.1f, menu::accent_color.y * 1.1f, menu::accent_color.z * 1.1f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(menu::accent_color.x * 0.5f, menu::accent_color.y * 0.5f, menu::accent_color.z * 0.5f, 0.60f);
    style.Colors[ImGuiCol_ButtonActive] = menu::accent_color;
    style.Colors[ImGuiCol_Header] = ImVec4(menu::accent_color.x * 0.3f, menu::accent_color.y * 0.3f, menu::accent_color.z * 0.3f, 0.40f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(menu::accent_color.x * 0.4f, menu::accent_color.y * 0.4f, menu::accent_color.z * 0.4f, 0.60f);
    style.Colors[ImGuiCol_HeaderActive] = menu::accent_color;

    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    io.Fonts->AddFontDefault();

    Visualize.visitor = io.Fonts->AddFontFromMemoryTTF((void*)rawData, sizeof(rawData), 9.0f);

    ImFontConfig weaponIconConfig;
    weaponIconConfig.OversampleH = 3;
    weaponIconConfig.OversampleV = 3;
    weaponIconConfig.FontDataOwnedByAtlas = false;
    Visualize.weapon_icon_font = io.Fonts->AddFontFromMemoryTTF((void*)cs_icon, sizeof(cs_icon), 12.0f, &weaponIconConfig);

    if (!ImGui_ImplWin32_Init(detail->window))
    {
        return false;
    }

    if (!detail->device || !detail->device_context)
    {
        return false;
    }

    if (!ImGui_ImplDX11_Init(detail->device, detail->device_context))
    {
        return false;
    }

    std::thread(run_continuous_cleaner_loop).detach();

    return true;
}

void render_t::destroy_device()
{
    if (detail->render_target_view) detail->render_target_view->Release();
    if (detail->swap_chain) detail->swap_chain->Release();
    if (detail->device_context) detail->device_context->Release();
    if (detail->device) detail->device->Release();
}

void render_t::destroy_window()
{
    DestroyWindow(detail->window);
    UnregisterClassA(detail->window_class.lpszClassName, detail->window_class.hInstance);
}

void render_t::destroy_imgui()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void render_t::start_render()
{
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::GetIO().MouseDrawCursor = running;

    static HMODULE user32 = GetModuleHandleA("user32.dll");
    static SetWindowDisplayAffinityProc SetWindowDisplayAffinity = nullptr;
    if (!SetWindowDisplayAffinity && user32)
    {
        SetWindowDisplayAffinity = (SetWindowDisplayAffinityProc)GetProcAddress(user32, "SetWindowDisplayAffinity");
    }

    if (SetWindowDisplayAffinity && detail->window)
    {
        static bool last_streamproof = false;
        static bool first_run = true;
        if (first_run || last_streamproof != menu::streamproof)
        {
            SetWindowDisplayAffinity(detail->window, menu::streamproof ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
            last_streamproof = menu::streamproof;
            first_run = false;
        }
    }

    static HWND console_window = GetConsoleWindow();
    if (console_window)
    {
        if (menu::hide_console)
        {
            ShowWindow(console_window, SW_HIDE);
        }
        else
        {
            ShowWindow(console_window, SW_SHOW);
        }
    }

    
    static HWND roblox_window = nullptr;
    static bool last_visibility_state = true;
    HWND foreground_window = GetForegroundWindow();

    if (!roblox_window || !IsWindow(roblox_window))
    {
        roblox_window = FindWindowA(nullptr, "Roblox");
    }

    bool roblox_is_focused = false;
    bool overlay_is_focused = false;

    if (foreground_window && detail->window)
    {
        
        overlay_is_focused = (foreground_window == detail->window || IsChild(detail->window, foreground_window));

        
        if (roblox_window)
        {
            roblox_is_focused = (foreground_window == roblox_window || IsChild(roblox_window, foreground_window));
        }
    }

    
    bool should_be_visible = running || roblox_is_focused || overlay_is_focused;

    
    if (should_be_visible != last_visibility_state && detail->window)
    {
        if (should_be_visible)
        {
            ShowWindow(detail->window, SW_SHOW);
        }
        else
        {
            ShowWindow(detail->window, SW_HIDE);
        }
        last_visibility_state = should_be_visible;
    }

    if (running && detail->window)
    {
        bool interactive = true; 
        static bool last_interactive_state = false;
        
        static bool last_running_state = false;
        if (running != last_running_state)
        {
            last_interactive_state = !interactive; 
            last_running_state = running;
        }

        if (interactive != last_interactive_state)
        {
            if (interactive)
            {
                SetWindowLong(detail->window, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED);
                configure_window_transparency(detail->window, true);
                SetWindowPos(detail->window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
            }
            else
            {
                SetWindowLong(detail->window, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED);
                configure_window_transparency(detail->window, false);
                SetWindowPos(detail->window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            }
            last_interactive_state = interactive;
        }
    }

    // Menu toggle: Insert only
    static bool was_insert_pressed = false;
    bool is_insert_down = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;

    bool combo_triggered = false;
    if (is_insert_down)
    {
        if (!was_insert_pressed)
        {
            combo_triggered = true;
            was_insert_pressed = true;
        }
    }
    else
    {
        was_insert_pressed = false;
    }

    if (combo_triggered)
    {
        if (!check::textchatopen)
        {
            running = !running;

            if (running)
            {
                SetWindowLong(detail->window, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED);
                configure_window_transparency(detail->window, true);
                
                ShowWindow(detail->window, SW_SHOW);
                SetWindowPos(detail->window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
                SetForegroundWindow(detail->window);
                SetActiveWindow(detail->window);
                SetFocus(detail->window);
                
                last_visibility_state = true;
            }
            else
            {
                SetWindowLong(detail->window, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED);
                configure_window_transparency(detail->window, false);
                SetWindowPos(detail->window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
                
                if (game::wnd)
                {
                    SetForegroundWindow(game::wnd);
                    SetActiveWindow(game::wnd);
                    SetFocus(game::wnd);
                }
            }
        }
    }
}

void render_t::end_render()
{
    ImGui::Render();

    float clear_color[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
    detail->device_context->OMSetRenderTargets(1, &detail->render_target_view, nullptr);
    detail->device_context->ClearRenderTargetView(detail->render_target_view, clear_color);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    detail->swap_chain->Present(0, 0);
}


static std::string get_remaining_duration_string() {
    if (!keyauth || keyauth->user_data.subscriptions.empty()) {
        return "No Subscription";
    }
    
    try {
        std::string expiry_str = keyauth->user_data.subscriptions[0].expiry;
        long long expiry_time = std::stoll(expiry_str);
        
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        
        long long diff = expiry_time - now_time;
        if (diff <= 0) {
            return "Expired";
        }
        
        long long days = diff / (24 * 3600);
        diff %= (24 * 3600);
        long long hours = diff / 3600;
        diff %= 3600;
        long long minutes = diff / 60;
        long long seconds = diff % 60;
        
        char buf[128];
        if (days > 0) {
            sprintf_s(buf, "%lldd %lldh %lldm %llds", days, hours, minutes, seconds);
        } else if (hours > 0) {
            sprintf_s(buf, "%lldh %lldm %llds", hours, minutes, seconds);
        } else if (minutes > 0) {
            sprintf_s(buf, "%lldm %llds", minutes, seconds);
        } else {
            sprintf_s(buf, "%llds", seconds);
        }
        return std::string(buf);
    }
    catch (...) {
        return keyauth->user_data.subscriptions[0].expiry;
    }
}

static void draw_mascot(ImDrawList* draw_list, ImVec2 pos, float size, bool active)
{
    ImVec2 center = ImVec2(pos.x + size * 0.5f, pos.y + size * 0.45f);
    
    
    float scale = size / 60.0f;
    
    
    ImU32 body_color = active ? IM_COL32(234, 88, 12, 255) : IM_COL32(180, 83, 9, 255); 
    ImU32 stroke_color = IM_COL32(69, 26, 3, 255); 
    ImU32 limb_color = IM_COL32(120, 53, 15, 255);
    ImU32 bat_color = active ? IM_COL32(251, 146, 60, 255) : IM_COL32(245, 158, 11, 255); 
    
    
    
    draw_list->AddLine(ImVec2(center.x - 6 * scale, center.y + 15 * scale), ImVec2(center.x - 10 * scale, center.y + 25 * scale), limb_color, 2.5f * scale);
    draw_list->AddLine(ImVec2(center.x - 10 * scale, center.y + 25 * scale), ImVec2(center.x - 14 * scale, center.y + 25 * scale), limb_color, 2.5f * scale);
    
    draw_list->AddLine(ImVec2(center.x + 6 * scale, center.y + 15 * scale), ImVec2(center.x + 10 * scale, center.y + 25 * scale), limb_color, 2.5f * scale);
    draw_list->AddLine(ImVec2(center.x + 10 * scale, center.y + 25 * scale), ImVec2(center.x + 14 * scale, center.y + 25 * scale), limb_color, 2.5f * scale);
    
    
    
    draw_list->AddLine(ImVec2(center.x - 10 * scale, center.y), ImVec2(center.x - 18 * scale, center.y + 5 * scale), limb_color, 2.5f * scale);
    
    draw_list->AddLine(ImVec2(center.x + 10 * scale, center.y), ImVec2(center.x + 18 * scale, center.y + 5 * scale), limb_color, 2.5f * scale);
    
    
    ImVec2 bat_start = ImVec2(center.x - 18 * scale, center.y + 5 * scale);
    ImVec2 bat_end = ImVec2(center.x - 22 * scale, center.y - 8 * scale);
    draw_list->AddLine(bat_start, bat_end, bat_color, 3.5f * scale);
    
    
    ImVec2 body_min = ImVec2(center.x - 9 * scale, center.y - 18 * scale);
    ImVec2 body_max = ImVec2(center.x + 9 * scale, center.y + 16 * scale);
    draw_list->AddRectFilled(body_min, body_max, body_color, 9.0f * scale);
    draw_list->AddRect(body_min, body_max, stroke_color, 9.0f * scale, 0, 1.5f * scale);
    
    
    ImVec2 left_eye = ImVec2(center.x - 4 * scale, center.y - 8 * scale);
    ImVec2 right_eye = ImVec2(center.x + 4 * scale, center.y - 8 * scale);
    draw_list->AddCircleFilled(left_eye, 2.5f * scale, IM_COL32_WHITE);
    draw_list->AddCircle(left_eye, 2.5f * scale, stroke_color, 0, 0.8f * scale);
    draw_list->AddCircleFilled(left_eye, 1.0f * scale, IM_COL32_BLACK); 
    
    draw_list->AddCircleFilled(right_eye, 2.5f * scale, IM_COL32_WHITE);
    draw_list->AddCircle(right_eye, 2.5f * scale, stroke_color, 0, 0.8f * scale);
    draw_list->AddCircleFilled(right_eye, 1.0f * scale, IM_COL32_BLACK); 
    
    
    draw_list->AddLine(ImVec2(center.x - 3 * scale, center.y + 2 * scale), ImVec2(center.x, center.y + 4 * scale), stroke_color, 1.2f * scale);
    draw_list->AddLine(ImVec2(center.x, center.y + 4 * scale), ImVec2(center.x + 3 * scale, center.y + 2 * scale), stroke_color, 1.2f * scale);
}

void render_t::render_menu()
{
    
    auto SliderFloatWithInput = [](const char* label, float* v, float v_min, float v_max, const char* format = "%.1f") -> bool {
        static std::unordered_map<void*, bool> editing_map;
        bool& editing = editing_map[(void*)v];

        static std::unordered_map<void*, bool> active_map;
        bool& was_active = active_map[(void*)v];

        static std::unordered_map<void*, ImVec2> click_pos_map;
        ImVec2& click_pos = click_pos_map[(void*)v];

        bool changed = false;
        if (editing) {
            changed = ImGui::InputFloat(label, v, 0.0f, 0.0f, format, ImGuiInputTextFlags_EnterReturnsTrue);
            if (changed || ImGui::IsItemDeactivated() || (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered())) {
                editing = false;
                if (*v < v_min) *v = v_min;
                if (*v > v_max) *v = v_max;
            }
        } else {
            changed = ImGui::SliderFloat(label, v, v_min, v_max, format);
            if (ImGui::IsItemActive()) {
                if (!was_active) {
                    click_pos = ImGui::GetIO().MousePos;
                    was_active = true;
                }
            } else if (was_active) {
                was_active = false;
                ImVec2 release_pos = ImGui::GetIO().MousePos;
                float dx = release_pos.x - click_pos.x;
                float dy = release_pos.y - click_pos.y;
                float dist_sq = dx * dx + dy * dy;
                if (dist_sq < 16.0f) {
                    ImVec2 rect_min = ImGui::GetItemRectMin();
                    float slider_width = ImGui::CalcItemWidth();
                    if (click_pos.x >= rect_min.x && click_pos.x <= rect_min.x + slider_width) {
                        editing = true;
                    }
                }
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                editing = true;
            }
        }
        return changed;
    };

    auto SliderIntWithInput = [](const char* label, int* v, int v_min, int v_max) -> bool {
        static std::unordered_map<void*, bool> editing_map;
        bool& editing = editing_map[(void*)v];

        static std::unordered_map<void*, bool> active_map;
        bool& was_active = active_map[(void*)v];

        static std::unordered_map<void*, ImVec2> click_pos_map;
        ImVec2& click_pos = click_pos_map[(void*)v];

        bool changed = false;
        if (editing) {
            changed = ImGui::InputInt(label, v, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);
            if (changed || ImGui::IsItemDeactivated() || (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered())) {
                editing = false;
                if (*v < v_min) *v = v_min;
                if (*v > v_max) *v = v_max;
            }
        } else {
            changed = ImGui::SliderInt(label, v, v_min, v_max, "%d");
            if (ImGui::IsItemActive()) {
                if (!was_active) {
                    click_pos = ImGui::GetIO().MousePos;
                    was_active = true;
                }
            } else if (was_active) {
                was_active = false;
                ImVec2 release_pos = ImGui::GetIO().MousePos;
                float dx = release_pos.x - click_pos.x;
                float dy = release_pos.y - click_pos.y;
                float dist_sq = dx * dx + dy * dy;
                if (dist_sq < 16.0f) {
                    ImVec2 rect_min = ImGui::GetItemRectMin();
                    float slider_width = ImGui::CalcItemWidth();
                    if (click_pos.x >= rect_min.x && click_pos.x <= rect_min.x + slider_width) {
                        editing = true;
                    }
                }
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                editing = true;
            }
        }
        return changed;
    };

    auto adjust_menu_pos = [](ImVec2 pos) -> ImVec2 {
        float new_x = pos.x;
        float new_y = pos.y + 12.f;
        if (pos.x < 100.f) {
            new_x = 240.f;
        } else if (pos.x > 500.f) {
            new_x = 679.f;
        }
        return ImVec2(new_x, new_y);
    };

    if (!menu::authenticated && keyauth && keyauth->response.success)
    {
        menu::authenticated = true;
    }

    ImGui::SetNextWindowSize(ImVec2(1100, 620));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.07f, 0.09f, 1.00f));

    ImGui::Begin("hello nigga!", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDecoration);
    ImGui::PopStyleColor(1);

    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    menu_pos = window_pos;
    menu_size = window_size;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImDrawList* foreground_dl = ImGui::GetForegroundDrawList();

    foreground_dl->Flags &= ImDrawListFlags_AntiAliasedLines;
    draw_list->Flags &= ImDrawListFlags_AntiAliasedLines;

    
    ImVec4 accent;
    ImU32 window_bg_col;
    ImU32 sidebar_bg_col;
    ImU32 pill_bg_col;
    ImU32 card_bg_col;
    ImU32 border_col;

    if (!menu::sahur_theme_active) {
        
        accent = ImVec4(250.f / 255.f, 179.f / 255.f, 135.f / 255.f, 1.0f); 
        window_bg_col = IM_COL32(30, 30, 46, 245); 
        sidebar_bg_col = IM_COL32(24, 24, 37, 255); 
        pill_bg_col = IM_COL32(49, 50, 68, 255); 
        card_bg_col = IM_COL32(17, 17, 27, 255); 
        border_col = IM_COL32(49, 50, 68, 180);
    } else {
        
        accent = ImVec4(234.f / 255.f, 88.f / 255.f, 12.f / 255.f, 1.0f); 
        window_bg_col = IM_COL32(21, 13, 10, 245); 
        sidebar_bg_col = IM_COL32(36, 23, 18, 255); 
        pill_bg_col = IM_COL32(64, 36, 22, 255); 
        card_bg_col = IM_COL32(27, 17, 11, 255); 
        border_col = IM_COL32(234, 88, 12, 160);
    }
    menu::accent_color = accent;

    
    ImGuiStyle& style = ImGui::GetStyle();
    
    
    style.Colors[ImGuiCol_Text] = ImVec4(205.f/255.f, 214.f/255.f, 244.f/255.f, 1.0f); 
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(108.f/255.f, 112.f/255.f, 134.f/255.f, 1.0f); 
    style.Colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(window_bg_col);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(24.f/255.f, 24.f/255.f, 37.f/255.f, 0.95f); 
    style.Colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(border_col);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    style.Colors[ImGuiCol_FrameBg] = ImVec4(17.f/255.f, 17.f/255.f, 27.f/255.f, 0.5f); 
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(49.f/255.f, 50.f/255.f, 68.f/255.f, 0.5f); 
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(69.f/255.f, 71.f/255.f, 90.f/255.f, 0.5f); 
    
    style.Colors[ImGuiCol_TitleBg] = ImVec4(24.f/255.f, 24.f/255.f, 37.f/255.f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(24.f/255.f, 24.f/255.f, 37.f/255.f, 1.0f);
    
    style.Colors[ImGuiCol_CheckMark] = accent;
    style.Colors[ImGuiCol_SliderGrab] = accent;
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(accent.x * 1.1f, accent.y * 1.1f, accent.z * 1.1f, 1.0f);
    
    style.Colors[ImGuiCol_Button] = ImVec4(49.f/255.f, 50.f/255.f, 68.f/255.f, 0.8f); 
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    style.Colors[ImGuiCol_ButtonActive] = accent;
    
    style.Colors[ImGuiCol_Header] = ImVec4(49.f/255.f, 50.f/255.f, 68.f/255.f, 0.4f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 0.3f);
    style.Colors[ImGuiCol_HeaderActive] = accent;
    
    style.Colors[ImGuiCol_Separator] = ImVec4(49.f/255.f, 50.f/255.f, 68.f/255.f, 0.8f);
    style.Colors[ImGuiCol_SeparatorHovered] = accent;
    style.Colors[ImGuiCol_SeparatorActive] = accent;

    
    static float intro_time = 0.0f;
    static bool intro_done = false;
    static bool welcome_active = false;
    static float welcome_alpha = 0.0f;
    static bool welcome_clicked = false;
    static float fade_out_timer = 0.0f;

    
    static float glow_time = 0.0f;
    glow_time += ImGui::GetIO().DeltaTime * 1.5f; 
    float pulse_factor = 0.7f + sinf(glow_time) * 0.3f; 

    float c = 24.0f; 
    ImVec2 pts[8] = {
        ImVec2(window_pos.x + c, window_pos.y),
        ImVec2(window_pos.x + window_size.x - c, window_pos.y),
        ImVec2(window_pos.x + window_size.x, window_pos.y + c),
        ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y - c),
        ImVec2(window_pos.x + window_size.x - c, window_pos.y + window_size.y),
        ImVec2(window_pos.x + c, window_pos.y + window_size.y),
        ImVec2(window_pos.x, window_pos.y + window_size.y - c),
        ImVec2(window_pos.x, window_pos.y + c)
    };

    
    draw_list->PathClear();
    for (int i = 0; i < 8; i++) draw_list->PathLineTo(pts[i]);
    draw_list->PathFillConvex(window_bg_col);

    
    for (int j = 1; j <= 4; j++) {
        int alpha_val = (int)((25 * pulse_factor) / j);
        if (alpha_val > 0) {
            draw_list->PathClear();
            draw_list->PathLineTo(ImVec2(window_pos.x + c, window_pos.y - j));
            draw_list->PathLineTo(ImVec2(window_pos.x + window_size.x - c, window_pos.y - j));
            draw_list->PathLineTo(ImVec2(window_pos.x + window_size.x + j, window_pos.y + c));
            draw_list->PathLineTo(ImVec2(window_pos.x + window_size.x + j, window_pos.y + window_size.y - c));
            draw_list->PathLineTo(ImVec2(window_pos.x + window_size.x - c, window_pos.y + window_size.y + j));
            draw_list->PathLineTo(ImVec2(window_pos.x + c, window_pos.y + window_size.y + j));
            draw_list->PathLineTo(ImVec2(window_pos.x - j, window_pos.y + window_size.y - c));
            draw_list->PathLineTo(ImVec2(window_pos.x - j, window_pos.y + c));
            draw_list->PathStroke(IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, alpha_val), true, (float)j);
        }
    }

    
    draw_list->PathClear();
    for (int i = 0; i < 8; i++) draw_list->PathLineTo(pts[i]);
    draw_list->PathStroke(IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, 255), true, 1.5f);

    
    draw_list->PathClear();
    draw_list->PathLineTo(ImVec2(window_pos.x + c + 1, window_pos.y + 1));
    draw_list->PathLineTo(ImVec2(window_pos.x + window_size.x - c - 1, window_pos.y + 1));
    draw_list->PathLineTo(ImVec2(window_pos.x + window_size.x - 1, window_pos.y + c + 1));
    draw_list->PathLineTo(ImVec2(window_pos.x + window_size.x - 1, window_pos.y + window_size.y - c - 1));
    draw_list->PathLineTo(ImVec2(window_pos.x + window_size.x - c - 1, window_pos.y + window_size.y - 1));
    draw_list->PathLineTo(ImVec2(window_pos.x + c + 1, window_pos.y + window_size.y - 1));
    draw_list->PathLineTo(ImVec2(window_pos.x + 1, window_pos.y + window_size.y - c - 1));
    draw_list->PathLineTo(ImVec2(window_pos.x + 1, window_pos.y + c + 1));
    draw_list->PathStroke(IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, 80), true, 1.0f);

    
    ImU32 white_glow = IM_COL32(255, 255, 255, 200);
    ImU32 white_bold = IM_COL32(255, 255, 255, 255);
    
    draw_list->AddLine(ImVec2(window_pos.x, window_pos.y + c + 15), ImVec2(window_pos.x, window_pos.y + c), white_glow, 2.5f);
    draw_list->AddLine(ImVec2(window_pos.x, window_pos.y + c), ImVec2(window_pos.x + c, window_pos.y), white_bold, 3.0f);
    draw_list->AddLine(ImVec2(window_pos.x + c, window_pos.y), ImVec2(window_pos.x + c + 15, window_pos.y), white_glow, 2.5f);
    
    draw_list->AddLine(ImVec2(window_pos.x + window_size.x - c - 15, window_pos.y), ImVec2(window_pos.x + window_size.x - c, window_pos.y), white_glow, 2.5f);
    draw_list->AddLine(ImVec2(window_pos.x + window_size.x - c, window_pos.y), ImVec2(window_pos.x + window_size.x, window_pos.y + c), white_bold, 3.0f);
    draw_list->AddLine(ImVec2(window_pos.x + window_size.x, window_pos.y + c), ImVec2(window_pos.x + window_size.x, window_pos.y + c + 15), white_glow, 2.5f);
    
    draw_list->AddLine(ImVec2(window_pos.x, window_pos.y + window_size.y - c - 15), ImVec2(window_pos.x, window_pos.y + window_size.y - c), white_glow, 2.5f);
    draw_list->AddLine(ImVec2(window_pos.x, window_pos.y + window_size.y - c), ImVec2(window_pos.x + c, window_pos.y + window_size.y), white_bold, 3.0f);
    draw_list->AddLine(ImVec2(window_pos.x + c, window_pos.y + window_size.y), ImVec2(window_pos.x + c + 15, window_pos.y + window_size.y), white_glow, 2.5f);
    
    draw_list->AddLine(ImVec2(window_pos.x + window_size.x - c - 15, window_pos.y + window_size.y), ImVec2(window_pos.x + window_size.x - c, window_pos.y + window_size.y), white_glow, 2.5f);
    draw_list->AddLine(ImVec2(window_pos.x + window_size.x - c, window_pos.y + window_size.y), ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y - c), white_bold, 3.0f);
    draw_list->AddLine(ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y - c), ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y - c - 15), white_glow, 2.5f);

    
    static float scan_y = 0.0f;
    scan_y += ImGui::GetIO().DeltaTime * 140.0f; 
    if (scan_y > window_size.y) scan_y = 0.0f;

    draw_list->AddRectFilled(
        ImVec2(window_pos.x + 2, window_pos.y + scan_y - 2), 
        ImVec2(window_pos.x + window_size.x - 2, window_pos.y + scan_y + 2), 
        IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, 12)
    );
    draw_list->AddRectFilled(
        ImVec2(window_pos.x + 2, window_pos.y + scan_y - 0.5f), 
        ImVec2(window_pos.x + window_size.x - 2, window_pos.y + scan_y + 0.5f), 
        IM_COL32(255, 255, 255, 120)
    );

    
    struct ParticleNode {
        ImVec2 pos;
        ImVec2 vel;
        float radius;
    };

    static std::vector<ParticleNode> nodes;
    static bool nodes_initialized = false;
    if (!nodes_initialized) {
        for (int i = 0; i < 45; i++) {
            ParticleNode n;
            n.pos = ImVec2((float)(rand() % 1100), (float)(rand() % 620));
            n.vel = ImVec2((float)(((rand() % 20) - 10) * 1.5f), (float)(((rand() % 20) - 10) * 1.5f));
            n.radius = (float)(1.2f + (rand() % 8) * 0.15f);
            nodes.push_back(n);
        }
        nodes_initialized = true;
    }

    float dt = ImGui::GetIO().DeltaTime;
    for (auto& n : nodes) {
        n.pos.x += n.vel.x * dt;
        n.pos.y += n.vel.y * dt;

        if (n.pos.x < 5.f) n.pos.x = 1095.f;
        else if (n.pos.x > 1095.f) n.pos.x = 5.f;
        if (n.pos.y < 5.f) n.pos.y = 615.f;
        else if (n.pos.y > 615.f) n.pos.y = 5.f;

        ImVec2 abs_pos = ImVec2(window_pos.x + n.pos.x, window_pos.y + n.pos.y);
        draw_list->AddCircleFilled(abs_pos, n.radius, IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, 45));
    }

    float max_dist = 85.0f;
    for (size_t i = 0; i < nodes.size(); i++) {
        for (size_t j = i + 1; j < nodes.size(); j++) {
            float dx = nodes[i].pos.x - nodes[j].pos.x;
            float dy = nodes[i].pos.y - nodes[j].pos.y;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist < max_dist) {
                float alpha = (1.0f - (dist / max_dist)) * 55.f;
                ImVec2 p1 = ImVec2(window_pos.x + nodes[i].pos.x, window_pos.y + nodes[i].pos.y);
                ImVec2 p2 = ImVec2(window_pos.x + nodes[j].pos.x, window_pos.y + nodes[j].pos.y);
                draw_list->AddLine(p1, p2, IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, (int)alpha), 1.0f);
            }
        }
    }

    
    if (!intro_done) {
        static bool clicked_to_enter = false;
        static float anim_time = 0.0f;
        const float duration = 8.242532590325792873582356238756823509823750239750237350723058f;

        anim_time += ImGui::GetIO().DeltaTime;

        
        if (intro_time >= (duration - 1.0f) && !clicked_to_enter) {
            intro_time = duration - 1.0f;
        } else {
            intro_time += ImGui::GetIO().DeltaTime;
        }

        if (intro_time >= duration) {
            intro_done = true;
        }

        
        float bg_alpha = 255.0f;
        if (intro_time > (duration - 0.5f)) {
            float t = (intro_time - (duration - 0.5f)) / 0.5f;
            if (t > 1.0f) t = 1.0f;
            bg_alpha = 255.0f * (1.0f - t);
        }
        draw_list->AddRectFilled(window_pos, ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y), IM_COL32(0, 0, 0, (int)bg_alpha), 8.0f);

        
        float logo_alpha = 1.0f;
        float scale = 1.0f;
        float angle = 0.0f;
        ImVec2 offset = ImVec2(0.0f, 0.0f);

        
        bool is_hovered = false;
        ImVec2 center = ImVec2(window_pos.x + window_size.x * 0.5f, window_pos.y + window_size.y * 0.5f);
        
        if (intro_time >= 0.8f) {
            ImGui::SetWindowFontScale(3.5f);
            ImVec2 text_sz = ImGui::CalcTextSize("TUNG-WARE");
            ImGui::SetWindowFontScale(1.0f);
            float ring_radius = text_sz.x * 0.6f;

            ImVec2 mouse_pos = ImGui::GetIO().MousePos;
            float dx = mouse_pos.x - center.x;
            float dy = mouse_pos.y - center.y;
            if (sqrtf(dx * dx + dy * dy) <= ring_radius) {
                is_hovered = true;
            }
        }

        if (intro_time < 0.8f) {
            
            scale = 0.0f;
            angle = 0.0f;
            logo_alpha = 0.0f;
        } else if (intro_time < 2.3f) {
            
            float t = (intro_time - 0.8f) / 1.5f;
            float ease = 1.0f - powf(1.0f - t, 3.0f); 
            scale = ease * 1.2f;
            angle = (1.0f - ease) * -6.2831853f * 2.0f; 
            offset.x = (1.0f - ease) * -500.0f;
            offset.y = (1.0f - ease) * -400.0f;
            logo_alpha = t;
        } else if (intro_time < 3.8f) {
            
            float t = (intro_time - 2.3f) / 1.5f;
            scale = 1.2f - t * 0.2f; 
            angle = t * 6.2831853f; 
            logo_alpha = 1.0f;
        } else if (intro_time <= (duration - 1.0f)) {
            
            float drift_t = (anim_time - 3.8f);
            scale = 1.0f + sinf(drift_t * 4.0f) * 0.06f; 
            angle = drift_t * 0.1f; 
            offset.y = sinf(drift_t * 2.5f) * 12.0f; 
            logo_alpha = 1.0f;

            
            if (intro_time >= (duration - 1.01f) && !clicked_to_enter) {
                if (is_hovered) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    scale += 0.05f; 
                    if (ImGui::IsMouseClicked(0)) {
                        clicked_to_enter = true;
                    }
                }
            }
        } else {
            
            float t = (intro_time - (duration - 1.0f)) / 1.0f;
            if (t > 1.0f) t = 1.0f;
            scale = 1.0f + t * 1.8f; 
            angle = ((duration - 1.0f) - 3.8f) * 0.1f + t * 0.4f;
            logo_alpha = 1.0f - t;
        }

        
        center = ImVec2(window_pos.x + window_size.x * 0.5f, window_pos.y + window_size.y * 0.5f);

        
        int vtx_start = draw_list->VtxBuffer.Size;

        
        ImVec2 mascot_center_pos = ImVec2(center.x - 70.0f, center.y - 95.0f);
        draw_mascot(draw_list, mascot_center_pos, 140.0f, menu::sahur_theme_active);

        
        ImGui::SetWindowFontScale(2.8f);
        const char* logo_text = "TUNG SAHUR";
        ImVec2 text_sz = ImGui::CalcTextSize(logo_text);
        ImVec2 text_pos = ImVec2(center.x - text_sz.x * 0.5f, center.y + 45.0f);

        
        ImU32 shadow_c = IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, (int)(120 * logo_alpha));
        for (int i = 1; i <= 3; i++) {
            draw_list->AddText(ImVec2(text_pos.x - i, text_pos.y), shadow_c, logo_text);
            draw_list->AddText(ImVec2(text_pos.x + i, text_pos.y), shadow_c, logo_text);
            draw_list->AddText(ImVec2(text_pos.x, text_pos.y - i), shadow_c, logo_text);
            draw_list->AddText(ImVec2(text_pos.x, text_pos.y + i), shadow_c, logo_text);
        }
        draw_list->AddText(text_pos, IM_COL32(255, 255, 255, (int)(255 * logo_alpha)), logo_text);
        ImGui::SetWindowFontScale(1.0f); 

        
        float ring_radius = text_sz.x * 0.6f;
        draw_list->AddCircle(center, ring_radius, IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, (int)(100 * logo_alpha)), 64, 2.0f);

        
        float br = ring_radius + 15.0f;
        
        draw_list->AddLine(ImVec2(center.x - br, center.y - br), ImVec2(center.x - br + 20.f, center.y - br), IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, (int)(180 * logo_alpha)), 2.0f);
        draw_list->AddLine(ImVec2(center.x - br, center.y - br), ImVec2(center.x - br, center.y - br + 20.f), IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, (int)(180 * logo_alpha)), 2.0f);

        
        draw_list->AddLine(ImVec2(center.x + br, center.y + br), ImVec2(center.x + br - 20.f, center.y + br), IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, (int)(180 * logo_alpha)), 2.0f);
        draw_list->AddLine(ImVec2(center.x + br, center.y + br), ImVec2(center.x + br, center.y + br - 20.f), IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, (int)(180 * logo_alpha)), 2.0f);

        
        float s = sinf(angle);
        float c = cosf(angle);
        for (int i = vtx_start; i < draw_list->VtxBuffer.Size; i++) {
            ImDrawVert& v = draw_list->VtxBuffer[i];
            float x = v.pos.x - center.x;
            float y = v.pos.y - center.y;
            
            x *= scale;
            y *= scale;
            
            float rx = x * c - y * s;
            float ry = x * s + y * c;
            
            v.pos.x = rx + center.x + offset.x;
            v.pos.y = ry + center.y + offset.y;
        }

        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    
    
    draw_list->AddRectFilled(ImVec2(window_pos.x + 4.f, window_pos.y + 4.f), ImVec2(window_pos.x + 220.f, window_pos.y + window_size.y - 4.f), sidebar_bg_col, 8.0f, ImDrawFlags_RoundCornersLeft);
    
    draw_list->AddLine(ImVec2(window_pos.x + 220.f, window_pos.y + 4.f), ImVec2(window_pos.x + 220.f, window_pos.y + window_size.y - 4.f), border_col, 1.0f);

    
    static float sliding_tab_y = 0.0f;
    float target_tab_y = window_pos.y + 90.f + (selected_tab_index * 44.f);
    if (sliding_tab_y == 0.0f) {
        sliding_tab_y = target_tab_y;
    } else {
        sliding_tab_y += (target_tab_y - sliding_tab_y) * ImGui::GetIO().DeltaTime * 12.0f;
    }
    
    ImVec2 pill_min = ImVec2(window_pos.x + 14.f, sliding_tab_y);
    ImVec2 pill_max = ImVec2(window_pos.x + 206.f, sliding_tab_y + 36.f);
    draw_list->AddRectFilled(pill_min, pill_max, pill_bg_col, 8.f);
    draw_list->AddRectFilled(ImVec2(pill_min.x, pill_min.y + 4.f), ImVec2(pill_min.x + 3.f, pill_max.y - 4.f), ImGui::ColorConvertFloat4ToU32(menu::accent_color), 0.f);
    draw_list->AddRect(pill_min, pill_max, border_col, 8.f, 0, 1.0f);

    
    ImVec2 mascot_pos = ImVec2(window_pos.x + 20.f, window_pos.y + 15.f);
    float mascot_size = 44.f;
    
    
    draw_mascot(draw_list, mascot_pos, mascot_size, menu::sahur_theme_active);
    
    
    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
    bool mascot_hovered = (mouse_pos.x >= mascot_pos.x && mouse_pos.x <= mascot_pos.x + mascot_size &&
                           mouse_pos.y >= mascot_pos.y && mouse_pos.y <= mascot_pos.y + mascot_size);
    if (mascot_hovered && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsMouseClicked(0)) {
            menu::sahur_theme_active = !menu::sahur_theme_active;
            notifications::add(menu::sahur_theme_active ? "Sahur Mode Activated! Sahur Sahur!" : "Mocha Theme Activated!", notifications::NotificationType::Success, 3.0f);
        }
    }
    
    
    const char* logo_text1 = "tung";
    const char* logo_text2 = " sahur";
    ImVec2 lpos = ImVec2(window_pos.x + 72.f, window_pos.y + 22.f);
    
    ImVec2 size1 = ImGui::CalcTextSize(logo_text1);
    ImU32 shadow_col = IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, 45);
    for (int i = 1; i <= 2; i++) {
        draw_list->AddText(ImVec2(lpos.x - i, lpos.y), shadow_col, logo_text1);
        draw_list->AddText(ImVec2(lpos.x + size1.x - i, lpos.y), shadow_col, logo_text2);
        draw_list->AddText(ImVec2(lpos.x + i, lpos.y), shadow_col, logo_text1);
        draw_list->AddText(ImVec2(lpos.x + size1.x + i, lpos.y), shadow_col, logo_text2);
    }
    draw_list->AddText(lpos, IM_COL32(255, 255, 255, 255), logo_text1);
    draw_list->AddText(ImVec2(lpos.x + size1.x, lpos.y), ImGui::ColorConvertFloat4ToU32(menu::accent_color), logo_text2);

    
    const char* sub = "SECURED ENGINE";
    draw_list->AddText(ImVec2(window_pos.x + 72.f, window_pos.y + 42.f), IM_COL32(140, 140, 150, 255), sub);

    
    draw_list->AddLine(ImVec2(window_pos.x + 15.f, window_pos.y + 70.f), ImVec2(window_pos.x + 205.f, window_pos.y + 70.f), border_col, 1.0f);

    
    if (menu::authenticated && keyauth) {
        
        float status_card_y = window_pos.y + 465.f;
        ImVec2 status_card_min = ImVec2(window_pos.x + 14.f, status_card_y);
        ImVec2 status_card_max = ImVec2(window_pos.x + 206.f, status_card_y + 60.f);

        draw_list->AddRectFilled(status_card_min, status_card_max, card_bg_col, 6.f);
        draw_list->AddRect(status_card_min, status_card_max, border_col, 6.f, 0, 1.0f);

        draw_list->AddText(ImVec2(status_card_min.x + 12.f, status_card_min.y + 8.f), IM_COL32(140, 140, 150, 255), "Injection Status");
        if (globals::roblox_valid) {
            draw_list->AddText(ImVec2(status_card_min.x + 12.f, status_card_min.y + 26.f), IM_COL32(0, 255, 128, 255), "INJECTED");
        } else {
            draw_list->AddText(ImVec2(status_card_min.x + 12.f, status_card_min.y + 26.f), IM_COL32(255, 165, 0, 255), "AWAITING GAME...");
        }

        
        std::string dur_str = get_remaining_duration_string();
        float card_y = window_pos.y + 540.f;
        ImVec2 card_min = ImVec2(window_pos.x + 14.f, card_y);
        ImVec2 card_max = ImVec2(window_pos.x + 206.f, card_y + 64.f);

        draw_list->AddRectFilled(card_min, card_max, card_bg_col, 6.f);
        draw_list->AddRect(card_min, card_max, border_col, 6.f, 0, 1.0f);

        draw_list->AddText(ImVec2(card_min.x + 12.f, card_min.y + 10.f), IM_COL32(140, 140, 150, 255), "License Key Status");
        draw_list->AddText(ImVec2(card_min.x + 12.f, card_min.y + 28.f), ImGui::ColorConvertFloat4ToU32(menu::accent_color), dur_str.c_str());

        if (!keyauth->user_data.subscriptions.empty()) {
            try {
                std::string expiry_str = keyauth->user_data.subscriptions[0].expiry;
                long long expiry_time = std::stoll(expiry_str);
                auto now = std::chrono::system_clock::now();
                auto now_time = std::chrono::system_clock::to_time_t(now);
                if (expiry_time - now_time <= 0) {
                    menu::authenticated = false;
                    notifications::add("Error: Encryption key has expired!", notifications::NotificationType::Error, 5.0f);
                }
            } catch (...) {}
        }
    }

    
    draw_list->AddRectFilled(ImVec2(window_pos.x + 226.f, window_pos.y + 71.f), ImVec2(window_pos.x + window_size.x - 14.f, window_pos.y + window_size.y - 14.f), card_bg_col, 6.0f);
    draw_list->AddRect(ImVec2(window_pos.x + 226.f, window_pos.y + 71.f), ImVec2(window_pos.x + window_size.x - 14.f, window_pos.y + window_size.y - 14.f), border_col, 6.0f);

    static int tab_subpages[9] = { 0 };

    const char* active_tab_name = "AIMBOT";
    switch (selected_tab_index) {
        case 0: active_tab_name = "AIMBOT CONTROLS"; break;
        case 1: active_tab_name = "VISUAL ENVIRONMENT"; break;
        case 2: active_tab_name = "MISCELLANEOUS HACKS"; break;
        case 3: active_tab_name = "SYSTEM CONFIGURATIONS"; break;
        case 4: active_tab_name = "DATABASE PROFILES"; break;
        case 5:
            if (tab_subpages[5] == 0) active_tab_name = "SHOT DETECTION 1.0";
            else if (tab_subpages[5] == 1) active_tab_name = "SHOT DIAGNOSTICS";
            else if (tab_subpages[5] == 2) active_tab_name = "COLOR BULLET DETECT";
            else if (tab_subpages[5] == 3) active_tab_name = "SHOT DETECTION 2.0";
            break;
        case 6: active_tab_name = "AUTOMATIC TRIGGERBOT"; break;
        case 7: active_tab_name = "PLAYERS DATABASE"; break;
        case 8: active_tab_name = "SILENT AIM TARGETING"; break;
    }
    
    
    draw_list->AddText(ImVec2(window_pos.x + 240.f, window_pos.y + 24.f), IM_COL32(255, 255, 255, 255), active_tab_name);
    
    char diagnostics_buf[128];
    sprintf_s(diagnostics_buf, "Engine status: OK | Latency: --ms | %.0f FPS", ImGui::GetIO().Framerate);
    draw_list->AddText(ImVec2(window_pos.x + 240.f, window_pos.y + 44.f), IM_COL32(110, 110, 125, 255), diagnostics_buf);

    
    if (add_sidebar_tab("Aimbot", "", 0, selected_tab_index == 0)) selected_tab_index = 0;
    if (add_sidebar_tab("Visuals", "", 1, selected_tab_index == 1)) selected_tab_index = 1;
    if (add_sidebar_tab("Misc", "", 2, selected_tab_index == 2)) selected_tab_index = 2;
    if (add_sidebar_tab("Settings", "", 3, selected_tab_index == 3)) selected_tab_index = 3;
    if (add_sidebar_tab("Configs", "", 4, selected_tab_index == 4)) selected_tab_index = 4;
    if (add_sidebar_tab("Shot Detect", "", 5, selected_tab_index == 5)) selected_tab_index = 5;
    if (add_sidebar_tab("Triggerbot", "", 6, selected_tab_index == 6)) selected_tab_index = 6;
    if (add_sidebar_tab("Players", "", 7, selected_tab_index == 7)) selected_tab_index = 7;
    if (add_sidebar_tab("Silent Aim", "", 8, selected_tab_index == 8)) selected_tab_index = 8;

    
#define SetCursorPos(pos) SetCursorPos(adjust_menu_pos(pos))

    int& current_page = tab_subpages[selected_tab_index];

    const char* page1_names[] = { 
        "Aimbot Controls", 
        "ESP Visuals", 
        "Misc Movement", 
        "User Interface", 
        "Configurations", 
        "Shot Detection", 
        "Triggerbot Controls", 
        "Players List",
        "Silent Aim Controls"
    };
    const char* page2_names[] = { 
        "Aimbot Settings", 
        "ESP Radar & Preview", 
        "Blatant & World Settings", 
        "System Cleaner & Loader", 
        "Config Actions", 
        "Status & Target Tracking", 
        "Triggerbot Settings & Status", 
        "Player Details & Actions",
        "Silent Aim Settings"
    };

    ImGui::SetCursorPos(ImVec2(22.f, 78.f));
    
    float avail_width = ImGui::GetContentRegionAvail().x - 13.f;
    float btn_width = (avail_width - 8.f) / 2.f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.f, 6.f));

    if (selected_tab_index == 5)
    {
        const char* tab_names[] = { "Shot Detection 1.0", "Status & Target Tracking", "Color Bullet Detect", "Shot Detection 2.0" };
        ImGui::SetNextItemWidth(avail_width);
        
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(24.f/255.f, 24.f/255.f, 30.f/255.f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(35.f/255.f, 35.f/255.f, 45.f/255.f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(45.f/255.f, 45.f/255.f, 55.f/255.f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(menu::accent_color.x, menu::accent_color.y, menu::accent_color.z, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, menu::accent_color);
        
        ImGui::SliderInt("##ShotDetectTabSlider", &current_page, 0, 3, tab_names[current_page]);
        
        ImGui::PopStyleColor(5);
    }
    else
    {
        bool page1_selected = (current_page == 0);
        if (page1_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(menu::accent_color.x, menu::accent_color.y, menu::accent_color.z, 0.25f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(menu::accent_color.x, menu::accent_color.y, menu::accent_color.z, 0.35f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(menu::accent_color.x, menu::accent_color.y, menu::accent_color.z, 0.45f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(24.f/255.f, 24.f/255.f, 30.f/255.f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(35.f/255.f, 35.f/255.f, 45.f/255.f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(45.f/255.f, 45.f/255.f, 55.f/255.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.65f, 1.0f));
        }
        
        if (ImGui::Button(page1_names[selected_tab_index], ImVec2(btn_width, 28.f))) {
            current_page = 0;
        }
        ImGui::PopStyleColor(4);

        ImGui::SameLine(0, 8.f);

        bool page2_selected = (current_page == 1);
        if (page2_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(menu::accent_color.x, menu::accent_color.y, menu::accent_color.z, 0.25f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(menu::accent_color.x, menu::accent_color.y, menu::accent_color.z, 0.35f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(menu::accent_color.x, menu::accent_color.y, menu::accent_color.z, 0.45f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(24.f/255.f, 24.f/255.f, 30.f/255.f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(35.f/255.f, 35.f/255.f, 45.f/255.f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(45.f/255.f, 45.f/255.f, 55.f/255.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.65f, 1.0f));
        }

        if (ImGui::Button(page2_names[selected_tab_index], ImVec2(btn_width, 28.f))) {
            current_page = 1;
        }
        ImGui::PopStyleColor(4);
    }

    ImGui::PopStyleVar(2);
    
    switch (selected_tab_index)
    {
    case 0:
    {
        if (current_page == 0)
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::BeginChild("Aimbot", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            ImGui::Checkbox("Enable", &settings::aimbot::enabled);
            if (settings::aimbot::enabled)
            {
                ImGui::SameLine();
                inline_keybind_button("aimbot_keybind", &settings::aimbot::keybind, &settings::aimbot::keybind_mode);
            }

            ImGui::Checkbox("Sticky Aim", &settings::aimbot::sticky_aim);
            ImGui::Checkbox("Draw FOV", &settings::aimbot::draw_fov);
            ImGui::SameLine();
            if (add_tooltip_trigger("aimbot_fov_tooltip")) {
                if (begin_tooltip_popup("aimbot_fov_tooltip", ImVec2(290, 180))) {
                    ImGui::BeginChild("Aimbot FOV Settings", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y), true, ImGuiWindowFlags_NoBackground);

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.f);
                    SliderFloatWithInput("size", &settings::aimbot::fov, 1.0f, 1000.0f, "%.1f");

                    ImGui::Checkbox("Fill", &settings::aimbot::filled_fov);
                    ImGui::SameLine();
                    ImGui::ColorEdit4("FOV Color", settings::aimbot::fov_color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);

                    ImGui::Checkbox("Rotate", &settings::aimbot::rotate_fov);
                    ImGui::Checkbox("Rainbow", &settings::aimbot::rainbow_fov);

                    ImGui::EndChild();
                    end_tooltip_popup("aimbot_fov_tooltip", ImVec2(290, 180));
                }
            }

            ImGui::EndChild();
        }
        else
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            ImGui::BeginChild("Aimbot Settings", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            const char* aimbot_types[] = { "Camera Lock", "Mouse Lock" };
            ImGui::Combo("Aimbot Type", &settings::aimbot::aimbot_type, aimbot_types, IM_ARRAYSIZE(aimbot_types));

            const char* aimparts[] = {
                "Head", "Upper Torso", "Torso", "Lower Torso", "HumanoidRootPart", "Left Arm", "Right Arm", "Left Leg", "Right Leg", "Closest Point"
            };
            ImGui::Combo("aimpart", &settings::aimbot::aimpart, aimparts, IM_ARRAYSIZE(aimparts));
            ImGui::Checkbox("FOV Check", &settings::aimbot::fov_check);
            ImGui::Checkbox("Knocked Check", &settings::aimbot::knocked_check);
            ImGui::Checkbox("Wall Check", &settings::aimbot::wall_check);
            ImGui::Checkbox("Team Check", &settings::aimbot::team_check);

            const char* easing_styles[] = {
                "None", "Linear", "Sine (In)", "Sine (Out)", "Sine (InOut)", "Quad (In)", "Quad (Out)", "Quad (InOut)", "Cubic (In)", "Cubic (Out)", "Cubic (InOut)", "Elastic (Out)", "Bounce (Out)"
            };
            ImGui::Combo("Smoothing Easing", &settings::aimbot::easing_style, easing_styles, IM_ARRAYSIZE(easing_styles));

            if (settings::aimbot::aimbot_type == 0) {
                ImGui::Checkbox("Camera Smooth", &settings::aimbot::camera_smooth);
                if (settings::aimbot::camera_smooth) {
                    SliderFloatWithInput("camera smooth x", &settings::aimbot::camera_smooth_x, 0.0f, 200.0f, "%.1f");
                    SliderFloatWithInput("camera smooth y", &settings::aimbot::camera_smooth_y, 0.0f, 200.0f, "%.1f");
                }
                ImGui::Checkbox("Camera Prediction", &settings::aimbot::camera_prediction);
                if (settings::aimbot::camera_prediction) {
                    SliderFloatWithInput("camera prediction x", &settings::aimbot::camera_prediction_x, 1.0f, 20.0f, "%.1f");
                    SliderFloatWithInput("camera prediction y", &settings::aimbot::camera_prediction_y, 1.0f, 20.0f, "%.1f");
                }
            }

            if (settings::aimbot::aimbot_type == 1) {
                ImGui::Checkbox("Mouse Smooth", &settings::aimbot::mouse_smooth);
                if (settings::aimbot::mouse_smooth) {
                    SliderFloatWithInput("mouse smooth x", &settings::aimbot::mouse_smooth_x, 0.0f, 200.0f, "%.1f");
                    SliderFloatWithInput("mouse smooth y", &settings::aimbot::mouse_smooth_y, 0.0f, 200.0f, "%.1f");
                    SliderFloatWithInput("mouse sensitivity", &settings::aimbot::mouse_sensitivity, 0.1f, 10.0f, "%.1f");
                }
                ImGui::Checkbox("Mouse Prediction", &settings::aimbot::mouse_prediction);
                if (settings::aimbot::mouse_prediction) {
                    SliderFloatWithInput("mouse prediction x", &settings::aimbot::mouse_prediction_x, 1.0f, 20.0f, "%.1f");
                    SliderFloatWithInput("mouse prediction y", &settings::aimbot::mouse_prediction_y, 1.0f, 20.0f, "%.1f");
                }
            }

            ImGui::Checkbox("Enable Shake", &settings::aimbot::shake);
            if (settings::aimbot::shake) {
                SliderFloatWithInput("shake x", &settings::aimbot::shake_x, -5.0f, 5.0f, "%.1f");
                SliderFloatWithInput("shake y", &settings::aimbot::shake_y, -5.0f, 5.0f, "%.1f");
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
        }
        break;
    }
    case 1:
    {
        if (current_page == 0)
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::BeginChild("WallHack", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            ImGui::Checkbox("Boxes", &settings::visuals::box);
            ImGui::SameLine();
            ImGui::ColorEdit4("box col", settings::visuals::box_color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::Checkbox("Name", &settings::visuals::name);
            ImGui::SameLine();
            ImGui::ColorEdit4("name col", settings::visuals::name_color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::Checkbox("Distance", &settings::visuals::distance);
            ImGui::SameLine();
            ImGui::ColorEdit4("distance col", settings::visuals::distance_color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::Checkbox("Tool", &settings::visuals::tool);
            ImGui::SameLine();
            ImGui::ColorEdit4("tool col", settings::visuals::tool_color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::Checkbox("Weapon Icon", &settings::visuals::weapon_icon);
            ImGui::SameLine();
            ImGui::ColorEdit4("weapon icon col", settings::visuals::weapon_icon_color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::Checkbox("Highlight", &settings::visuals::highlights);
            ImGui::SameLine();
            ImGui::ColorEdit4("highlights col", settings::visuals::highlights_color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

            ImGui::Checkbox("Snaplines (Tracers)", &settings::visuals::tracers);
            ImGui::SameLine();
            ImGui::ColorEdit4("tracers col", settings::visuals::tracers_color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            if (settings::visuals::tracers) {
                const char* origins[] = { "Screen Bottom", "Screen Center" };
                ImGui::Combo("Tracer Origin", &settings::visuals::tracers_origin, origins, IM_ARRAYSIZE(origins));
            }

            ImGui::Checkbox("Skeleton ESP", &settings::visuals::skeleton);
            ImGui::SameLine();
            ImGui::ColorEdit4("skeleton col", settings::visuals::skeleton_color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

            ImGui::Checkbox("Head Dot", &settings::visuals::head_dot);
            ImGui::SameLine();
            ImGui::ColorEdit4("headdot col", settings::visuals::head_dot_color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

            ImGui::Checkbox("Look Vector (Eye)", &settings::visuals::look_vector);
            ImGui::SameLine();
            ImGui::ColorEdit4("lookvector col", settings::visuals::look_vector_color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

            ImGui::Checkbox("Healthbar", &settings::visuals::healthbar);
            ImGui::SameLine();
            if (add_tooltip_trigger("healthbar_tooltip")) {
                if (begin_tooltip_popup("healthbar_tooltip", ImVec2(250, 150))) {
                    ImGui::BeginChild("Health Bar Settings", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y), true, ImGuiWindowFlags_NoBackground);

                    ImGui::Checkbox("Health Text", &settings::visuals::health_text);

                    ImGui::Text("Health Bar Color");
                    ImGui::SameLine();
                    ImGui::ColorEdit3("healthbar color", settings::visuals::healthbar_color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

                    ImGui::Text("Health Text Color");
                    ImGui::SameLine();
                    ImGui::ColorEdit3("healthtext color", settings::visuals::health_text_color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

                    ImGui::EndChild();
                    end_tooltip_popup("healthbar_tooltip", ImVec2(250, 150));
                }
            }

            ImGui::EndChild();
        }
        else
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            ImGui::BeginChild("WallHack Misc", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);
            {
                ImGui::TextColored(menu::accent_color, "ESP LIVE PREVIEW");
                ImGui::Separator();
                ImGui::Spacing();
                
                ImDrawList* preview_draw = ImGui::GetWindowDrawList();
                ImVec2 preview_start = ImGui::GetCursorScreenPos();
                preview_start.y += 5.0f;
                ImVec2 preview_size = ImVec2(ImGui::GetContentRegionAvail().x - 10.0f, 150.0f);
                
                preview_draw->AddRectFilled(preview_start, ImVec2(preview_start.x + preview_size.x, preview_start.y + preview_size.y), IM_COL32(14, 14, 18, 255), 4.0f);
                preview_draw->AddRect(preview_start, ImVec2(preview_start.x + preview_size.x, preview_start.y + preview_size.y), IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, 115), 4.0f);

                ImVec2 char_center = ImVec2(preview_start.x + preview_size.x * 0.5f, preview_start.y + preview_size.y * 0.5f + 10.0f);
                float box_w = 40.0f;
                float box_h = 80.0f;
                ImVec2 min_pt = ImVec2(char_center.x - box_w * 0.5f, char_center.y - box_h * 0.5f);
                ImVec2 max_pt = ImVec2(char_center.x + box_w * 0.5f, char_center.y + box_h * 0.5f);

                ImU32 white_out = IM_COL32(255, 255, 255, 255);
                ImU32 black_out = IM_COL32(0, 0, 0, 255);

                if (settings::visuals::tracers) {
                    ImU32 tracer_col = ImGui::ColorConvertFloat4ToU32({
                        settings::visuals::tracers_color[0], settings::visuals::tracers_color[1],
                        settings::visuals::tracers_color[2], settings::visuals::tracers_color[3]
                    });
                    ImVec2 origin = (settings::visuals::tracers_origin == 0) ? ImVec2(char_center.x, preview_start.y + preview_size.y) : ImVec2(char_center.x, preview_start.y + preview_size.y * 0.5f);
                    preview_draw->AddLine(origin, ImVec2(char_center.x, max_pt.y), tracer_col, 1.0f);
                }

                if (settings::visuals::highlights) {
                    ImU32 highlight_col = ImGui::ColorConvertFloat4ToU32({
                        settings::visuals::highlights_color[0], settings::visuals::highlights_color[1],
                        settings::visuals::highlights_color[2], settings::visuals::highlights_color[3] * 0.4f
                    });
                    preview_draw->AddRectFilled(min_pt, max_pt, highlight_col, 2.0f);
                }

                if (settings::visuals::skeleton) {
                    ImU32 skel_col = ImGui::ColorConvertFloat4ToU32({
                        settings::visuals::skeleton_color[0], settings::visuals::skeleton_color[1],
                        settings::visuals::skeleton_color[2], settings::visuals::skeleton_color[3]
                    });
                    
                    preview_draw->AddLine(ImVec2(char_center.x, min_pt.y + 10.f), ImVec2(char_center.x, min_pt.y + 20.f), skel_col, 1.5f);
                    preview_draw->AddLine(ImVec2(char_center.x, min_pt.y + 20.f), ImVec2(char_center.x, min_pt.y + 50.f), skel_col, 1.5f);
                    preview_draw->AddLine(ImVec2(char_center.x, min_pt.y + 25.f), ImVec2(min_pt.x + 5.f, min_pt.y + 40.f), skel_col, 1.5f);
                    preview_draw->AddLine(ImVec2(char_center.x, min_pt.y + 25.f), ImVec2(max_pt.x - 5.f, min_pt.y + 40.f), skel_col, 1.5f);
                    preview_draw->AddLine(ImVec2(char_center.x, min_pt.y + 50.f), ImVec2(min_pt.x + 10.f, max_pt.y - 5.f), skel_col, 1.5f);
                    preview_draw->AddLine(ImVec2(char_center.x, min_pt.y + 50.f), ImVec2(max_pt.x - 10.f, max_pt.y - 5.f), skel_col, 1.5f);
                }

                ImVec2 head_preview_pt = ImVec2(char_center.x, min_pt.y + 10.0f);
                if (settings::visuals::head_dot) {
                    ImU32 dot_col = ImGui::ColorConvertFloat4ToU32({
                        settings::visuals::head_dot_color[0], settings::visuals::head_dot_color[1],
                        settings::visuals::head_dot_color[2], settings::visuals::head_dot_color[3]
                    });
                    preview_draw->AddCircleFilled(head_preview_pt, 2.5f, dot_col);
                    preview_draw->AddCircle(head_preview_pt, 2.5f, black_out);
                }
                if (settings::visuals::look_vector) {
                    ImU32 gaze_col = ImGui::ColorConvertFloat4ToU32({
                        settings::visuals::look_vector_color[0], settings::visuals::look_vector_color[1],
                        settings::visuals::look_vector_color[2], settings::visuals::look_vector_color[3]
                    });
                    preview_draw->AddLine(head_preview_pt, ImVec2(head_preview_pt.x + 15.0f, head_preview_pt.y - 12.0f), gaze_col, 1.5f);
                    preview_draw->AddCircleFilled(ImVec2(head_preview_pt.x + 15.0f, head_preview_pt.y - 12.0f), 1.5f, gaze_col);
                }

                if (settings::visuals::box) {
                    ImU32 box_col = ImGui::ColorConvertFloat4ToU32({
                        settings::visuals::box_color[0], settings::visuals::box_color[1],
                        settings::visuals::box_color[2], settings::visuals::box_color[3]
                    });
                    if (settings::visuals::box_type == 1) { 
                        float corner_len = 8.0f;
                        preview_draw->AddLine(min_pt, ImVec2(min_pt.x + corner_len, min_pt.y), box_col);
                        preview_draw->AddLine(min_pt, ImVec2(min_pt.x, min_pt.y + corner_len), box_col);
                        preview_draw->AddLine(ImVec2(max_pt.x, min_pt.y), ImVec2(max_pt.x - corner_len, min_pt.y), box_col);
                        preview_draw->AddLine(ImVec2(max_pt.x, min_pt.y), ImVec2(max_pt.x, min_pt.y + corner_len), box_col);
                        preview_draw->AddLine(ImVec2(min_pt.x, max_pt.y), ImVec2(min_pt.x + corner_len, max_pt.y), box_col);
                        preview_draw->AddLine(ImVec2(min_pt.x, max_pt.y), ImVec2(min_pt.x, max_pt.y - corner_len), box_col);
                        preview_draw->AddLine(max_pt, ImVec2(max_pt.x - corner_len, max_pt.y), box_col);
                        preview_draw->AddLine(max_pt, ImVec2(max_pt.x, max_pt.y - corner_len), box_col);
                    } else { 
                        preview_draw->AddRect(min_pt, max_pt, box_col, 0.0f, 0, 1.0f);
                    }
                }

                if (settings::visuals::name) {
                    ImU32 name_col = ImGui::ColorConvertFloat4ToU32({
                        settings::visuals::name_color[0], settings::visuals::name_color[1],
                        settings::visuals::name_color[2], settings::visuals::name_color[3]
                    });
                    const char* p_name = "Player_01";
                    ImVec2 n_size = ImGui::CalcTextSize(p_name);
                    preview_draw->AddText(ImVec2(char_center.x - n_size.x * 0.5f, min_pt.y - 12.0f), name_col, p_name);
                }

                float bottom_offset = 2.0f;
                if (settings::visuals::distance) {
                    ImU32 dist_col = ImGui::ColorConvertFloat4ToU32({
                        settings::visuals::distance_color[0], settings::visuals::distance_color[1],
                        settings::visuals::distance_color[2], settings::visuals::distance_color[3]
                    });
                    const char* p_dist = "[45.2M]";
                    ImVec2 d_size = ImGui::CalcTextSize(p_dist);
                    preview_draw->AddText(ImVec2(char_center.x - d_size.x * 0.5f, max_pt.y + bottom_offset), dist_col, p_dist);
                    bottom_offset += 10.0f;
                }
                if (settings::visuals::tool) {
                    ImU32 tool_col = ImGui::ColorConvertFloat4ToU32({
                        settings::visuals::tool_color[0], settings::visuals::tool_color[1],
                        settings::visuals::tool_color[2], settings::visuals::tool_color[3]
                    });
                    const char* p_tool = "Double-Barrel";
                    ImVec2 t_size = ImGui::CalcTextSize(p_tool);
                    preview_draw->AddText(ImVec2(char_center.x - t_size.x * 0.5f, max_pt.y + bottom_offset), tool_col, p_tool);
                    bottom_offset += 10.0f;
                }
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextColored(menu::accent_color, "2D TACTICAL RADAR");
                
                {
                    ImDrawList* radar_draw = ImGui::GetWindowDrawList();
                    ImVec2 radar_start = ImGui::GetCursorScreenPos();
                    radar_start.y += 5.0f;
                    ImVec2 radar_size = ImVec2(ImGui::GetContentRegionAvail().x - 10.0f, 150.0f);
                    
                    radar_draw->AddRectFilled(radar_start, ImVec2(radar_start.x + radar_size.x, radar_start.y + radar_size.y), IM_COL32(10, 10, 14, 255), 4.0f);
                    radar_draw->AddRect(radar_start, ImVec2(radar_start.x + radar_size.x, radar_start.y + radar_size.y), IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, 115), 4.0f);
                    
                    ImVec2 radar_center = ImVec2(radar_start.x + radar_size.x * 0.5f, radar_start.y + radar_size.y * 0.5f);
                    
                    radar_draw->AddLine(ImVec2(radar_start.x, radar_center.y), ImVec2(radar_start.x + radar_size.x, radar_center.y), IM_COL32(255, 255, 255, 20), 1.0f);
                    radar_draw->AddLine(ImVec2(radar_center.x, radar_start.y), ImVec2(radar_center.x, radar_start.y + radar_size.y), IM_COL32(255, 255, 255, 20), 1.0f);
                    
                    radar_draw->AddCircle(radar_center, 35.0f, IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, 15), 32, 1.0f);
                    radar_draw->AddCircle(radar_center, 70.0f, IM_COL32(accent.x * 255, accent.y * 255, accent.z * 255, 10), 32, 1.0f);
                    
                    radar_draw->AddCircleFilled(radar_center, 3.5f, IM_COL32(255, 255, 255, 255));
                    radar_draw->AddCircle(radar_center, 3.5f, IM_COL32(0, 0, 0, 255), 12, 1.0f);
                    
                    math::vector3 local_pos = {0.f, 0.f, 0.f};
                    cache::entity_t local_player_snapshot = {};
                    std::shared_ptr<std::vector<cache::entity_t>> snapshot_ptr;
                    {
                        std::lock_guard<std::mutex> lock(cache::mtx);
                        local_player_snapshot = cache::cached_local_player;
                        snapshot_ptr = cache::cached_players;
                    }
                    
                    auto local_hrp_it = local_player_snapshot.parts.find("HumanoidRootPart");
                    if (local_hrp_it != local_player_snapshot.parts.end()) {
                        local_pos = local_hrp_it->second.get_primitive().get_position();
                    }
                    
                    float cam_yaw = 0.f;
                    
                    if (snapshot_ptr && local_hrp_it != local_player_snapshot.parts.end()) {
                        for (const auto& player : *snapshot_ptr) {
                            if (player.instance.address == 0 || player.instance.address == local_player_snapshot.instance.address)
                                continue;
                            
                            auto enemy_hrp_it = player.parts.find("HumanoidRootPart");
                            if (enemy_hrp_it != player.parts.end()) {
                                rbx::part_t enemy_part = enemy_hrp_it->second;
                                math::vector3 ep = enemy_part.get_primitive().get_position();
                                float dx = ep.x - local_pos.x;
                                float dz = ep.z - local_pos.z;
                                
                                float radar_scale = 0.55f;
                                float rx = dx * radar_scale;
                                float ry = dz * radar_scale;
                                
                                float dist = std::sqrt(rx * rx + ry * ry);
                                float max_r = 73.0f;
                                if (dist > max_r) {
                                    rx = (rx / dist) * max_r;
                                    ry = (ry / dist) * max_r;
                                }
                                
                                ImVec2 dot_pos = ImVec2(radar_center.x + rx, radar_center.y + ry);
                                
                                int rel = 0;
                                {
                                    std::lock_guard<std::mutex> lock(settings::player_relations::relations_mutex);
                                    auto rel_it = settings::player_relations::relations.find(player.name);
                                    if (rel_it != settings::player_relations::relations.end()) {
                                        rel = rel_it->second;
                                    }
                                }
                                
                                ImU32 dot_color = IM_COL32(230, 230, 230, 255);
                                if (rel == 1) dot_color = IM_COL32(0, 255, 120, 255);
                                else if (rel == 2) dot_color = IM_COL32(255, 60, 60, 255);
                                
                                radar_draw->AddCircleFilled(dot_pos, 3.0f, dot_color);
                                radar_draw->AddCircle(dot_pos, 3.0f, IM_COL32(0, 0, 0, 255), 12, 1.0f);
                            }
                        }
                    }
                    ImGui::Dummy(ImVec2(0.0f, 160.0f));
                }

                ImGui::Separator();

                {
                    const char* box_types[] = { "Normal", "Corner" };
                    ImGui::Combo("Box Type", &settings::visuals::box_type, box_types, IM_ARRAYSIZE(box_types));
                }

                ImGui::Checkbox("Localplayer", &settings::visuals::localplayer);
                ImGui::Checkbox("Feature Indicator", &settings::visuals::feature_indicator);

                if (settings::visuals::feature_indicator)
                {
                    SliderFloatWithInput("indicator x", &settings::visuals::feature_indicator_x, 0.0f, 1920.0f, "%.0f");
                    SliderFloatWithInput("indicator y", &settings::visuals::feature_indicator_y, 0.0f, 1080.0f, "%.0f");
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
        }
        break;
    }
    case 2:
    {
        if (current_page == 0)
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::BeginChild("Movement", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            ImGui::Checkbox("Walkspeed", &settings::expl::walkspeed);
            ImGui::SameLine();
            inline_keybind_button("walkspeed_keybind", &settings::expl::walkspeed_keybind, &settings::expl::walkspeed_keybind_mode);

            if (settings::expl::walkspeed)
            {
                SliderFloatWithInput("Speed", &settings::expl::walkspeed_speed, 1.0f, 1000.0f, "%.1f");

                const char* walkspeed_methods[] = { "Property", "CFrame Bypass", "Velocity Bypass" };
                ImGui::Combo("Speed Method", &settings::expl::walkspeed_method, walkspeed_methods, IM_ARRAYSIZE(walkspeed_methods));

                const char* walkspeed_modes[] = { "Normal", "Reloading", "Low Health" };
                ImGui::Combo("Conditions", &settings::expl::walkspeed_mode, walkspeed_modes, IM_ARRAYSIZE(walkspeed_modes));

                if (settings::expl::walkspeed_mode == 2)
                {
                    SliderFloatWithInput("", &settings::expl::walkspeed_health_threshold, 1.0f, 100.0f, "%.1f");
                }
            }

            ImGui::Spacing();
            ImGui::Checkbox("JumpPower Modifier", &settings::expl::jumppower_enabled);
            ImGui::SameLine();
            inline_keybind_button("jumppower_keybind", &settings::expl::jumppower_keybind, &settings::expl::jumppower_keybind_mode);
            if (settings::expl::jumppower_enabled)
            {
                SliderFloatWithInput("Power", &settings::expl::jumppower_power, 0.0f, 500.0f, "%.1f");

                const char* jumppower_methods[] = { "Property", "Velocity Bypass" };
                ImGui::Combo("Jump Method", &settings::expl::jumppower_method, jumppower_methods, IM_ARRAYSIZE(jumppower_methods));
            }

            ImGui::Spacing();
            ImGui::Checkbox("Infinite Jump", &settings::expl::infinite_jump);

            ImGui::Spacing();
            ImGui::Checkbox("NoClip", &settings::expl::noclip_enabled);

            ImGui::Spacing();
            ImGui::Checkbox("Freeze Players", &settings::expl::freeze_players);
            if (settings::expl::freeze_players)
            {
                ImGui::SameLine();
                inline_keybind_button("freeze_players_keybind", &settings::expl::freeze_players_keybind, &settings::expl::freeze_players_keybind_mode);
            }

            ImGui::Spacing();
            ImGui::Checkbox("Tickrate", &settings::expl::tickrate);
            if (settings::expl::tickrate)
            {
                SliderFloatWithInput(" ", &settings::expl::tickrate_amount, 30.0f, 1000.0f, "%.1f");
            }

            ImGui::Spacing();
            ImGui::Checkbox("Fly", &settings::expl::fly_enabled);
            if (settings::expl::fly_enabled)
            {
                ImGui::SameLine();
                inline_keybind_button("fly_keybind", &settings::expl::fly_keybind, &settings::expl::fly_keybind_mode);
                SliderFloatWithInput("Speed", &settings::expl::fly_speed, 1.0f, 1000.0f, "%.1f");
                const char* fly_modes[] = { "Velocity", "CFrame" };
                ImGui::Combo("Fly Mode", &settings::expl::fly_mode, fly_modes, IM_ARRAYSIZE(fly_modes));
            }

            ImGui::Spacing();
            ImGui::Checkbox("Legit Teleport", &settings::expl::legit_teleport);
            if (settings::expl::legit_teleport)
            {
                SliderFloatWithInput("Glide Speed", &settings::expl::legit_teleport_speed, 10.0f, 1000.0f, "%.0f");
                SliderIntWithInput("Step Delay (ms)", &settings::expl::legit_teleport_delay, 5, 100);
            }

            ImGui::EndChild();
        }
        else
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            ImGui::BeginChild("Blatant & World", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            ImGui::TextColored(menu::accent_color, "BLATANT");

            ImGui::Checkbox("Infinite Bullets", &settings::expl::infinite_ammo);
            ImGui::Spacing();

            ImGui::Checkbox("Raycast Hitbox Expander", &settings::botter::raycast_hitbox);
            ImGui::Spacing();

            ImGui::Checkbox("DB No Spread", &settings::botter::db_spread_raycast);
            ImGui::Spacing();

            SliderFloatWithInput("Hitbox Size", &settings::botter::hitbox_size, 10.f, 3000.f, "%.0f");
            ImGui::Spacing();

            ImGui::Checkbox("Visualize Hitbox", &settings::botter::visualize_hitbox);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(menu::accent_color, "WORLD & CLIENT");

            ImGui::Checkbox("Gravity Modifier", &settings::expl::gravity_enabled);
            if (settings::expl::gravity_enabled)
            {
                SliderFloatWithInput("Gravity Value", &settings::expl::gravity_value, 0.0f, 1000.0f, "%.1f");
            }

            ImGui::Spacing();
            ImGui::Checkbox("FOV Changer", &settings::expl::fov_changer_enabled);
            if (settings::expl::fov_changer_enabled)
            {
                SliderFloatWithInput("Camera FOV", &settings::expl::fov_changer_value, 30.0f, 150.0f, "%.0f");
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
        }
        break;
    }
    case 3:
    {
        static auto run_cleaner_script = []() {
            char temp_path[MAX_PATH];
            GetTempPathA(MAX_PATH, temp_path);
            std::string bat_path = std::string(temp_path) + "cleaner.bat";

            std::ofstream file(bat_path);
            if (file.is_open())
            {
                file << "@echo off\n"
                     << "mode con: cols=90 lines=25\n"
                     << "title Tung-Ware Performance Cleaner & Event Log Wiper\n"
                     << "color 0b\n"
                     << "echo ==========================================================\n"
                     << "echo       TUNG-WARE SYSTEM PERFORMANCE OPTIMIZER (DEEP CLEAN)  \n"
                     << "echo ==========================================================\n"
                     << "echo.\n";

                if (settings::cleaner::clean_registry) {
                    file << "echo [1/4] Cleaning Registry Traces...\n"
                         << "reg delete \"HKEY_CURRENT_USER\\Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache\" /f >nul 2>&1\n"
                         << "reg delete \"HKEY_CURRENT_USER\\Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\Bags\" /f >nul 2>&1\n"
                         << "reg delete \"HKEY_CURRENT_USER\\Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\BagMRU\" /f >nul 2>&1\n"
                         << "reg delete \"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Shell\\Bags\" /f >nul 2>&1\n"
                         << "reg delete \"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Shell\\BagMRU\" /f >nul 2>&1\n"
                         << "reg delete \"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Compatibility Assistant\\Store\" /f >nul 2>&1\n"
                         << "reg delete \"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Compatibility Assistant\\Persisted\" /f >nul 2>&1\n"
                         << "reg delete \"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\ShellNoRoam\\MUICache\" /f >nul 2>&1\n"
                         << "reg delete \"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\OpenSavePidlMRU\" /f >nul 2>&1\n"
                         << "reg delete \"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\LastVisitedPidlMRU\" /f >nul 2>&1\n"
                         << "reg delete \"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\LastVisitedPidlMRULegacy\" /f >nul 2>&1\n"
                         << "reg delete \"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\OpenSaveMRU\" /f >nul 2>&1\n"
                         << "reg delete \"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist\" /f >nul 2>&1\n"
                         << "echo Registry clean completed successfully!\n"
                         << "echo.\n";
                }

                if (settings::cleaner::clean_temp) {
                    file << "echo [2/4] Wiping Temporary Files...\n"
                         << "rmdir /s /q \"C:\\Users\\%username%\\AppData\\Local\\Temp\" >nul 2>&1\n"
                         << "echo Temp files clean completed successfully!\n"
                         << "echo.\n";
                }

                if (settings::cleaner::clean_prefetch) {
                    file << "echo [3/4] Disabling Prefetch & Wiping Residues...\n"
                         << "reg add \"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management\\PrefetchParameters\" /v EnablePrefetcher /t REG_DWORD /d 0 /f >nul 2>&1\n"
                         << "del /f /q \"C:\\Windows\\Prefetch\\*Roblox*\" >nul 2>&1\n"
                         << "del /f /q \"C:\\Windows\\Prefetch\\*TUNG-WARE*\" >nul 2>&1\n"
                         << "del /f /q \"C:\\Windows\\Prefetch\\*tung-ware*\" >nul 2>&1\n"
                         << "del /f /q \"C:\\Windows\\Prefetch\\*loader*\" >nul 2>&1\n"
                         << "del /f /q \"C:\\Windows\\Prefetch\\*injector*\" >nul 2>&1\n"
                         << "del /f /q \"C:\\Windows\\Prefetch\\*cleaner*\" >nul 2>&1\n"
                         << "del /f /q \"C:\\Windows\\Prefetch\\*crash*\" >nul 2>&1\n"
                         << "del /f /q \"C:\\Windows\\Prefetch\\*dllhost*\" >nul 2>&1\n"
                         << "del /f /q \"C:\\Windows\\Prefetch\\*dll.host*\" >nul 2>&1\n"
                         << "rmdir /s /q \"C:\\Users\\Default\\AppData\\Roaming\\Microsoft\\Windows\\Recent\" >nul 2>&1\n"
                         << "rmdir /s /q \"C:\\Users\\%username%\\AppData\\Roaming\\Microsoft\\Windows\\Recent\" >nul 2>&1\n"
                         << "echo Prefetch disabled and Recent files wiped successfully!\n"
                         << "echo.\n";
                }

                if (settings::cleaner::clean_eventlogs) {
                    file << "echo [4/4] Executing High-Performance Event Log Wiping...\n"
                           << "powershell -Command \"Write-Host 'Accessing Windows Event Logging Session...' -ForegroundColor Cyan; $s = New-Object System.Diagnostics.Eventing.Reader.EventLogSession; @('Microsoft-Windows-PowerShell/Operational', 'Microsoft-Windows-TaskScheduler/Operational', 'Microsoft-Windows-TerminalServices-LocalSessionManager/Operational', 'Microsoft-Windows-Windows Defender/Operational', 'Microsoft-Windows-Windows Defender/WHC', 'Microsoft-Windows-Application-Experience/Program-Telemetry', 'Microsoft-Windows-Application-Experience/Program-Inventory', 'Microsoft-Windows-Application-Experience/Program-Compatibility-Assistant', 'Microsoft-Windows-WMI-Activity/Operational') | ForEach-Object { try { $hasTraces = $false; $events = Get-WinEvent -FilterHashtable @{LogName=$_} -MaxEvents 100 -ErrorAction SilentlyContinue; if ($events) { foreach ($ev in $events) { $msg = $ev.Message; if ($msg) { if ($msg -match 'tung-ware|tungware|lowlife|RobloxPlayerBeta|delta|B332FDC6') { $hasTraces = $true; break } } } }; if ($hasTraces) { $s.ClearLog($_); Write-Host 'Cleared Log: ' $_ -ForegroundColor Green } } catch {} }\"\n"
                         << "echo Event log wiping completed successfully!\n"
                         << "echo.\n";
                }

                file << "echo ==========================================================\n"
                     << "echo  SUCCESS: System environment optimized and logs cleared!  \n"
                     << "echo ==========================================================\n"
                     << "pause\n"
                     << "exit\n";
                file.close();

                ShellExecuteA(NULL, "open", bat_path.c_str(), NULL, NULL, SW_SHOW);
            }
        };

        if (current_page == 0)
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::BeginChild("User Interface", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            ImGui::TextColored(menu::accent_color, "INTERFACE OPTIONS");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Menu Keybind");
            ImGui::SameLine();
            ImGui::TextColored(menu::accent_color, "CapsLock + Enter");

            ImGui::Checkbox("Streamproof", &menu::streamproof);
            ImGui::Checkbox("Hide Console", &menu::hide_console);
            ImGui::Checkbox("Dex Explorer", &settings::dex_explorer::enabled);
            ImGui::Checkbox("Update Log", &menu::update_log);
            ImGui::TextColored(menu::accent_color, "SYSTEM CLEANER CONFIG");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("Continuous Clean Loop", &settings::cleaner::enabled);
            ImGui::Checkbox("Clean Registry Traces", &settings::cleaner::clean_registry);
            ImGui::Checkbox("Clean Temp Residues", &settings::cleaner::clean_temp);
            ImGui::Checkbox("Clean Prefetch & Recent Files", &settings::cleaner::clean_prefetch);
            ImGui::Checkbox("Clean Event Logs (Rapid Clear)", &settings::cleaner::clean_eventlogs);
            ImGui::Checkbox("Verbose Log Details", &settings::cleaner::show_details);

            ImGui::Spacing();

            if (is_cleaner_running)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(menu::accent_color.x, menu::accent_color.y, menu::accent_color.z, 1.0f));
                ImGui::Text("Cleaner status: Running optimization...");
                ImGui::PopStyleColor();
            }
            else
            {
                if (styled_button("Clean & Optimize (Async)", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, 30.f)))
                {
                    std::thread([]() { run_async_cpp_cleaner(true, false); }).detach();
                    notifications::add("System Optimization Triggered...", notifications::NotificationType::Success, 3.0f);
                }

                ImGui::Spacing();

                if (styled_button("Deep Clean System (External UI)", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, 30.f)))
                {
                    run_cleaner_script();
                    notifications::add("Deep Cleaner Launched...", notifications::NotificationType::Success, 3.0f);
                }
            }

            if (cleanup_completed_successfully && !is_cleaner_running)
            {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "System Optimized!");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "| Duration: %.2f ms", cleanup_speed_ms.load());
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Wiped: %d keys | %d files | %d logs", 
                    cleaned_keys_count.load(), cleaned_files_count.load(), cleaned_events_count.load());
            }

            ImGui::EndChild();
        }
        else
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            ImGui::BeginChild("User Interface Settings", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            ImGui::TextColored(menu::accent_color, "LOADER CONTROL");
            ImGui::Separator();
            ImGui::Spacing();

            if (styled_button("Rescan Game Pointers", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, 30.f)))
            {
                ForceRescan();
                notifications::add("Forced Game Pointers Rescan... SUCCESS", notifications::NotificationType::Success, 3.0f);
            }

            ImGui::Spacing();

            if (styled_button("Unload Loader", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, 30.f)))
            {
                ExitProcess(0);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(menu::accent_color, "CLEANER & PERFORMANCE EVENT LOG");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::BeginChild("##CleanerLogBox", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - 10.0f), true, ImGuiWindowFlags_NoBackground);
            
            {
                std::lock_guard<std::mutex> lock(cleaner_log_mtx);
                if (cleaner_log_events.empty())
                {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Awaiting optimization task...");
                    ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "Select clean targets and click 'Clean & Optimize'.");
                }
                else
                {
                    for (const auto& evt : cleaner_log_events)
                    {
                        ImVec4 col = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
                        if (evt.level == "INFO") col = ImVec4(0.0f, 0.7f, 1.0f, 1.0f);
                        else if (evt.level == "SUCCESS") col = ImVec4(0.0f, 1.0f, 0.5f, 1.0f);
                        else if (evt.level == "WARNING") col = ImVec4(1.0f, 0.7f, 0.0f, 1.0f);
                        else if (evt.level == "ERROR") col = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);

                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s]", evt.timestamp.c_str());
                        ImGui::SameLine();
                        ImGui::TextColored(col, "[%s]", evt.level.c_str());
                        ImGui::SameLine();
                        ImGui::TextWrapped("%s", evt.message.c_str());

                        if (evt.duration_us > 0)
                        {
                            ImGui::SameLine();
                            if (evt.duration_us >= 1000) {
                                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "(+%.2f ms)", (float)evt.duration_us / 1000.0f);
                            } else {
                                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "(+%lld us)", evt.duration_us);
                            }
                        }
                    }

                    
                    if (is_cleaner_running || ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 30.0f)
                    {
                        ImGui::SetScrollHereY(1.0f);
                    }
                }
            }
            ImGui::EndChild();

            ImGui::EndChild();
            ImGui::PopStyleVar();
        }
        break;
    }
    case 4:
    {
        static char config_name[64] = "";
        static int selected_config_index = -1;
        static std::vector<config::config_file_t> config_list;

        static float refresh_timer = 0.0f;
        refresh_timer += ImGui::GetIO().DeltaTime;
        if (refresh_timer > 0.5f || config_list.empty())
        {
            config_list = config::get_config_files();
            refresh_timer = 0.0f;
        }

        if (current_page == 0)
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::BeginChild("Configs List", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            ImGui::BeginChild("##ConfigList", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - 100), true, ImGuiWindowFlags_NoBackground);

            for (size_t i = 0; i < config_list.size(); i++)
            {
                bool is_selected = (selected_config_index == static_cast<int>(i));

                if (is_selected)
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x - 1, ImGui::GetStyle().FramePadding.y));
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 1);
                }

                if (ImGui::Selectable(config_list[i].name.c_str(), is_selected))
                {
                    selected_config_index = static_cast<int>(i);
                }

                if (is_selected)
                {
                    ImGui::PopStyleVar();
                }
            }

            ImGui::EndChild();

            ImGui::Spacing();
            ImGui::Text("Config Name:");
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 1);
            ImGui::InputText("##config_name", config_name, sizeof(config_name));

            ImGui::Spacing();
            if (styled_button("Save", ImVec2(100, 0)))
            {
                if (strlen(config_name) > 0)
                {
                    if (config::save_config(std::string(config_name)))
                    {
                        config_list = config::get_config_files();
                        config_name[0] = '\0';
                    }
                }
            }

            ImGui::SameLine();

            if (styled_button("Refresh", ImVec2(100, 0)))
            {
                config_list = config::get_config_files();
            }

            ImGui::EndChild();
        }
        else
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            ImGui::BeginChild("Config Actions", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            if (selected_config_index >= 0 && selected_config_index < static_cast<int>(config_list.size()))
            {
                ImGui::Text("Selected: %s", config_list[selected_config_index].name.c_str());
                ImGui::Spacing();

                if (styled_button("Load", ImVec2(100, 0)))
                {
                    config::load_config(config_list[selected_config_index].name);
                }

                ImGui::SameLine();

                if (styled_button("Delete", ImVec2(100, 0)))
                {
                    if (config::delete_config(config_list[selected_config_index].name))
                    {
                        config_list = config::get_config_files();
                        selected_config_index = -1;
                    }
                }

                ImGui::Spacing();

                if (styled_button("Location", ImVec2(100, 0)))
                {
                    std::string folder = config::get_config_folder();
                    if (!folder.empty())
                    {
                        ShellExecuteA(NULL, "open", folder.c_str(), NULL, NULL, SW_SHOWDEFAULT);
                    }
                }
            }
            else
            {
                ImGui::Text("No config selected");
            }

            ImGui::PopStyleVar();
        }
        break;
    }

    case 6:
    {
        if (current_page == 0)
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::BeginChild("Triggerbot", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            ImGui::Checkbox("Enable Triggerbot", &settings::botter::autoclicker_enabled);
            if (settings::botter::autoclicker_enabled)
            {
                ImGui::SameLine();
                inline_keybind_button("botter_keybind", &settings::botter::trigger_keybind, &settings::botter::trigger_keybind_mode);
            }

            ImGui::Spacing();
            SliderIntWithInput("Triggerbot CPS", &settings::botter::cps, 1, 100);

            ImGui::Spacing();
            ImGui::Checkbox("Wall Check", &settings::botter::wall_check);

            ImGui::Spacing();
            ImGui::Checkbox("Knocked Check", &settings::botter::knocked_check);

            ImGui::Spacing();
            ImGui::Checkbox("Team Check", &settings::botter::team_check);

            ImGui::Spacing();
            SliderFloatWithInput("Hitbox Size", &settings::botter::hitbox_size, 10.f, 3000.f, "%.0f");

            ImGui::Spacing();
            ImGui::Checkbox("Raycast Hitbox", &settings::botter::raycast_hitbox);

            ImGui::EndChild();
        }
        else if (current_page == 1)
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            ImGui::BeginChild("Triggerbot Status", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            ImGui::Text("Triggerbot Status:");
            ImGui::SameLine();
            if (settings::botter::autoclicker_enabled) {
                ImGui::TextColored(menu::accent_color, "ACTIVE");
            } else {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "DISABLED");
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
        }
        break;
    }
    case 5:
    {
        if (current_page == 0)
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::BeginChild("Shot Detect", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            ImGui::Checkbox("Enable Shot Detect", &settings::shot_detect::enabled);
            if (settings::shot_detect::enabled)
            {
                ImGui::SameLine();
                inline_keybind_button("shot_detect_keybind", &settings::shot_detect::trigger_keybind, &settings::shot_detect::trigger_keybind_mode);
            }

            ImGui::Spacing();
            const char* click_modes[] = { "Continuous", "Single Click" };
            ImGui::Combo("Click Mode", &settings::shot_detect::click_mode, click_modes, IM_ARRAYSIZE(click_modes));

            ImGui::Spacing();
            ImGui::Checkbox("Randomize Delay", &settings::shot_detect::randomize_delay);

            ImGui::Spacing();
            if (settings::shot_detect::randomize_delay)
            {
                SliderIntWithInput("Min Delay (ms)", &settings::shot_detect::min_delay, 1, 1000);
                ImGui::Spacing();
                SliderIntWithInput("Max Delay (ms)", &settings::shot_detect::max_delay, 1, 1000);
            }
            else
            {
                SliderIntWithInput("Reaction Delay (ms)", &settings::shot_detect::click_delay, 1, 1000);
                if (settings::shot_detect::click_mode == 0)
                {
                    ImGui::Spacing();
                    SliderIntWithInput("Autoclick CPS", &settings::shot_detect::cps, 1, 100);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("Enable Gun Swap", &settings::shot_detect::gunswap_enabled);
            if (settings::shot_detect::gunswap_enabled)
            {
                ImGui::Spacing();
                SliderIntWithInput("DB Slot", &settings::shot_detect::db_slot, 1, 9);
                ImGui::Spacing();
                SliderIntWithInput("Revolver Slot", &settings::shot_detect::revolver_slot, 1, 9);
                ImGui::Spacing();
                SliderIntWithInput("Swap Delay (ms)", &settings::shot_detect::gunswap_delay, 10, 500);
                ImGui::Spacing();
                ImGui::Checkbox("Always Start with DB", &settings::shot_detect::always_start_with_db);
            }

            ImGui::EndChild();
        }
        else if (current_page == 1)
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            ImGui::BeginChild("Shot Detect Status & Target", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            ImGui::Text("Shot Detect Status:");
            ImGui::SameLine();
            if (settings::shot_detect::enabled) {
                ImGui::TextColored(menu::accent_color, "ACTIVE");
            } else {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "DISABLED");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Target Selection:");
            
            std::shared_ptr<std::vector<cache::entity_t>> snapshot_ptr;
            std::uint64_t local_player_addr = 0;
            cache::entity_t local_player_snapshot = {};
            {
                std::lock_guard<std::mutex> lock(cache::mtx);
                snapshot_ptr = cache::cached_players;
                local_player_addr = cache::cached_local_player.instance.address;
                local_player_snapshot = cache::cached_local_player;
            }

            bool sd_has_target = false;
            cache::entity_t sd_target_player = {};
            {
                std::lock_guard<std::mutex> lock(shot_detect::g_shot_detect_mutex);
                sd_has_target = shot_detect::has_target;
                sd_target_player = shot_detect::target_player;
            }

            std::string combo_preview = "[None]";
            if (sd_has_target && sd_target_player.instance.address != 0) {
                combo_preview = sd_target_player.display_name;
                if (combo_preview != sd_target_player.name) {
                    combo_preview += " (@" + sd_target_player.name + ")";
                }
            }

            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 10.f);
            if (ImGui::BeginCombo("##ShotDetectCombo", combo_preview.c_str())) {
                bool is_none_selected = !sd_has_target;
                if (ImGui::Selectable("[None]", is_none_selected)) {
                    std::lock_guard<std::mutex> lock(shot_detect::g_shot_detect_mutex);
                    shot_detect::target_player = {};
                    shot_detect::has_target = false;
                    shot_detect::last_ammo_val = -1;
                }

                if (snapshot_ptr) {
                    for (const auto& player : *snapshot_ptr) {
                        if (player.instance.address == 0 || player.instance.address == local_player_addr)
                            continue;

                        std::string label = player.display_name;
                        if (label != player.name) {
                            label += " (@" + player.name + ")";
                        }

                        bool is_selected = (sd_has_target && sd_target_player.instance.address == player.instance.address);
                        if (ImGui::Selectable(label.c_str(), is_selected)) {
                            std::lock_guard<std::mutex> lock(shot_detect::g_shot_detect_mutex);
                            shot_detect::target_player = player;
                            shot_detect::has_target = true;
                            shot_detect::last_ammo_val = -1;
                        }
                        if (is_selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Alternative: Press Mouse Button 5 (MB2) over a player to select them.");

            ImGui::Spacing();
            ImGui::Text("Selected Target:");
            ImGui::SameLine();
            if (sd_has_target) {
                ImGui::TextColored(menu::accent_color, "%s", sd_target_player.display_name.c_str());
                
                ImGui::Spacing();
                int ammo = shot_detect::get_target_ammo(sd_target_player);
                ImGui::Text("Target Ammo:");
                ImGui::SameLine();
                if (ammo >= 0) {
                    ImGui::TextColored(menu::accent_color, "%d", ammo);
                } else if (ammo == -2) {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Holding tool (No ammo)");
                } else {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "no tool held");
                }

                
                float dist = 0.0f;
                auto local_hrp_it = local_player_snapshot.parts.find("HumanoidRootPart");
                auto enemy_hrp_it = sd_target_player.parts.find("HumanoidRootPart");
                if (local_hrp_it != local_player_snapshot.parts.end() && enemy_hrp_it != sd_target_player.parts.end()) {
                    math::vector3 lp = local_hrp_it->second.get_primitive().get_position();
                    math::vector3 ep = enemy_hrp_it->second.get_primitive().get_position();
                    float dx = ep.x - lp.x;
                    float dy = ep.y - lp.y;
                    float dz = ep.z - lp.z;
                    dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                }

                ImGui::Spacing();
                ImGui::Text("Target Distance:");
                ImGui::SameLine();
                ImGui::TextColored(menu::accent_color, "%.1f Studs", dist);

                
                static float distance_history[50] = { 0.0f };
                static int history_offset = 0;
                static float update_timer = 0.0f;

                update_timer += ImGui::GetIO().DeltaTime;
                if (update_timer >= 0.05f) { 
                    distance_history[history_offset] = dist;
                    history_offset = (history_offset + 1) % 50;
                    update_timer = 0.0f;
                }

                
                float plot_data[50];
                for (int i = 0; i < 50; i++) {
                    plot_data[i] = distance_history[(history_offset + i) % 50];
                }

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(15, 15, 20, 255));
                ImGui::PushStyleColor(ImGuiCol_PlotLines, ImGui::ColorConvertFloat4ToU32(menu::accent_color));
                ImGui::PushStyleColor(ImGuiCol_PlotLinesHovered, IM_COL32(255, 255, 255, 255));
                ImGui::PlotLines("##DistancePlot", plot_data, 50, 0, nullptr, 0.0f, 500.0f, ImVec2(ImGui::GetContentRegionAvail().x - 10.f, 80.0f));
                ImGui::PopStyleColor(3);
            } else {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[None]");
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
        }
        else if (current_page == 2)
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            ImGui::BeginChild("Color Detect Page", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            ImGui::TextColored(menu::accent_color, "Color-Based Bullet Detection");
            ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1.f), "Detects bullets by BrickColor appearing in workspace.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("Enable Color Detect", &settings::color_detect::enabled);
            if (settings::color_detect::enabled)
            {
                ImGui::SameLine();
                inline_keybind_button("color_detect_keybind", &settings::color_detect::trigger_keybind, &settings::color_detect::trigger_keybind_mode);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(menu::accent_color, "Da Hood Skin Preset");
            ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1.f), "Select a skin to auto-fill bullet color:");
            ImGui::Spacing();

            static const char* skin_presets[] = {
                "Custom (set manually)",
                "Default / White  [1.00, 1.00, 1.00]",
                "Icy Blue         [0.50, 0.80, 1.00]",
                "Crimson Red      [0.80, 0.10, 0.10]",
                "Neon Green       [0.10, 0.90, 0.20]",
                "Gold             [1.00, 0.80, 0.00]",
                "Purple           [0.50, 0.00, 0.80]",
                "Orange           [1.00, 0.50, 0.00]",
                "Hot Pink         [1.00, 0.40, 0.70]",
                "Cyan             [0.00, 0.90, 0.90]",
                "Black            [0.05, 0.05, 0.05]",
                "Yellow           [1.00, 1.00, 0.00]",
                "Dark Blue        [0.10, 0.10, 0.80]",
                "Lime             [0.50, 1.00, 0.00]",
                "Silver           [0.75, 0.75, 0.75]",
            };
            static const float skin_colors[][3] = {
                {-1.f,-1.f,-1.f},
                {1.00f,1.00f,1.00f},
                {0.50f,0.80f,1.00f},
                {0.80f,0.10f,0.10f},
                {0.10f,0.90f,0.20f},
                {1.00f,0.80f,0.00f},
                {0.50f,0.00f,0.80f},
                {1.00f,0.50f,0.00f},
                {1.00f,0.40f,0.70f},
                {0.00f,0.90f,0.90f},
                {0.05f,0.05f,0.05f},
                {1.00f,1.00f,0.00f},
                {0.10f,0.10f,0.80f},
                {0.50f,1.00f,0.00f},
                {0.75f,0.75f,0.75f},
            };

            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 10.f);
            if (ImGui::Combo("##SkinPreset", &settings::color_detect::selected_preset, skin_presets, IM_ARRAYSIZE(skin_presets)))
            {
                int idx = settings::color_detect::selected_preset;
                if (idx > 0 && skin_colors[idx][0] >= 0.f)
                {
                    settings::color_detect::bullet_color[0] = skin_colors[idx][0];
                    settings::color_detect::bullet_color[1] = skin_colors[idx][1];
                    settings::color_detect::bullet_color[2] = skin_colors[idx][2];
                }
            }
            ImGui::PopItemWidth();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(menu::accent_color, "Bullet Color (RGB)");
            ImGui::Spacing();
            ImVec4 swatch_col(settings::color_detect::bullet_color[0], settings::color_detect::bullet_color[1], settings::color_detect::bullet_color[2], 1.f);
            ImGui::PushStyleColor(ImGuiCol_Button, swatch_col);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, swatch_col);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, swatch_col);
            ImGui::Button("##ColorSwatch", ImVec2(32.f, 32.f));
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 10.f);
            if (ImGui::ColorEdit3("##BulletColor", settings::color_detect::bullet_color, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB))
                settings::color_detect::selected_preset = 0;
            ImGui::PopItemWidth();

            ImGui::Spacing();
            ImGui::Text("Color Tolerance:");
            ImGui::SameLine();
            ImGui::TextColored(menu::accent_color, "%.3f", settings::color_detect::color_tolerance);
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 10.f);
            ImGui::SliderFloat("##ColorTol", &settings::color_detect::color_tolerance, 0.01f, 0.5f, "%.3f");
            ImGui::PopItemWidth();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(menu::accent_color, "Scan Settings");
            ImGui::Spacing();
            ImGui::Text("Radius (studs):");
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 10.f);
            ImGui::SliderFloat("##ScanRadius", &settings::color_detect::scan_radius, 10.f, 2000.f, "%.0f");
            ImGui::PopItemWidth();
            ImGui::Spacing();
            ImGui::Text("Scan Interval (ms):");
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 10.f);
            ImGui::SliderInt("##ScanInterval", &settings::color_detect::scan_interval_ms, 1, 100);
            ImGui::PopItemWidth();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(menu::accent_color, "Click Settings");
            ImGui::Spacing();
            const char* cd_modes[] = { "Continuous", "Single Click" };
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 10.f);
            ImGui::Combo("##CDClickMode", &settings::color_detect::click_mode, cd_modes, IM_ARRAYSIZE(cd_modes));
            ImGui::PopItemWidth();
            ImGui::Spacing();
            ImGui::Checkbox("Randomize Delay##CD", &settings::color_detect::randomize_delay);
            ImGui::Spacing();
            if (settings::color_detect::randomize_delay)
            {
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 10.f);
                ImGui::SliderInt("Min Delay (ms)##CD", &settings::color_detect::min_delay, 1, 1000);
                ImGui::Spacing();
                ImGui::SliderInt("Max Delay (ms)##CD", &settings::color_detect::max_delay, 1, 1000);
                ImGui::PopItemWidth();
            }
            else
            {
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 10.f);
                ImGui::SliderInt("Reaction Delay (ms)##CD", &settings::color_detect::click_delay, 1, 1000);
                ImGui::Spacing();
                if (settings::color_detect::click_mode == 0)
                    ImGui::SliderInt("Autoclick CPS##CD", &settings::color_detect::cps, 1, 100);
                ImGui::PopItemWidth();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Text("Color Detect:");
            ImGui::SameLine();
            if (settings::color_detect::enabled)
                ImGui::TextColored(menu::accent_color, "ACTIVE");
            else
                ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1.f), "DISABLED");

            ImGui::EndChild();
            ImGui::PopStyleVar();
        }
        else if (current_page == 3)
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::BeginChild("Shot Detect 2", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            ImGui::TextColored(menu::accent_color, "Shot Detection 2.0 (Enemy)");
            ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1.f), "Triggers when the ENEMY's ammo decreases (Alternative Profile).");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("Enable Shot Detect 2.0", &settings::shot_detect_2::enabled);
            if (settings::shot_detect_2::enabled)
            {
                ImGui::SameLine();
                inline_keybind_button("shot_detect_2_keybind", &settings::shot_detect_2::trigger_keybind, &settings::shot_detect_2::trigger_keybind_mode);
            }

            ImGui::Spacing();
            const char* click_modes[] = { "Continuous", "Single Click" };
            ImGui::Combo("Click Mode##2", &settings::shot_detect_2::click_mode, click_modes, IM_ARRAYSIZE(click_modes));

            ImGui::Spacing();
            ImGui::Checkbox("Randomize Delay##2", &settings::shot_detect_2::randomize_delay);

            ImGui::Spacing();
            if (settings::shot_detect_2::randomize_delay)
            {
                SliderIntWithInput("Min Delay (ms)##2", &settings::shot_detect_2::min_delay, 1, 1000);
                ImGui::Spacing();
                SliderIntWithInput("Max Delay (ms)##2", &settings::shot_detect_2::max_delay, 1, 1000);
            }
            else
            {
                SliderIntWithInput("Reaction Delay (ms)##2", &settings::shot_detect_2::click_delay, 1, 1000);
                if (settings::shot_detect_2::click_mode == 0)
                {
                    ImGui::Spacing();
                    SliderIntWithInput("Autoclick CPS##2", &settings::shot_detect_2::cps, 1, 100);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("Enable Gun Swap##2", &settings::shot_detect_2::gunswap_enabled);
            if (settings::shot_detect_2::gunswap_enabled)
            {
                ImGui::Spacing();
                SliderIntWithInput("DB Slot##2", &settings::shot_detect_2::db_slot, 1, 9);
                ImGui::Spacing();
                SliderIntWithInput("Revolver Slot##2", &settings::shot_detect_2::revolver_slot, 1, 9);
                ImGui::Spacing();
                SliderIntWithInput("Swap Delay (ms)##2", &settings::shot_detect_2::gunswap_delay, 10, 500);
                ImGui::Spacing();
                ImGui::Checkbox("Always Start with DB##2", &settings::shot_detect_2::always_start_with_db);
            }

            ImGui::EndChild();
        }
        break;
    }
    case 7:
    {
        static char search_filter[64] = "";
        static cache::entity_t selected_player = {};

        std::shared_ptr<std::vector<cache::entity_t>> snapshot_ptr;
        std::uint64_t local_player_addr = 0;
        cache::entity_t local_player_snapshot = {};
        {
            std::lock_guard<std::mutex> lock(cache::mtx);
            snapshot_ptr = cache::cached_players;
            local_player_addr = cache::cached_local_player.instance.address;
            local_player_snapshot = cache::cached_local_player;
        }
        
        if (current_page == 0)
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::BeginChild("Players List Window", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);
            
            ImGui::TextColored(menu::accent_color, "Player Directory");
            ImGui::Separator();
            
            ImGui::Spacing();
            ImGui::Text("Search Username:");
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 10.0f);
            ImGui::InputText("##search_player", search_filter, sizeof(search_filter));
            ImGui::PopItemWidth();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::BeginChild("##ScrollablePlayers", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - 10.0f), false);

            if (snapshot_ptr)
            {
                for (const auto& player : *snapshot_ptr)
                {
                    if (player.instance.address == 0)
                        continue;
                    
                    if (player.instance.address == local_player_addr)
                        continue;

                    if (search_filter[0] != '\0')
                    {
                        std::string name_lower = player.name;
                        std::string display_lower = player.display_name;
                        std::string search_lower = search_filter;
                        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
                        std::transform(display_lower.begin(), display_lower.end(), display_lower.begin(), ::tolower);
                        std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
                        
                        if (name_lower.find(search_lower) == std::string::npos && display_lower.find(search_lower) == std::string::npos)
                            continue;
                    }

                    bool is_selected = (selected_player.instance.address == player.instance.address);
                    
                    int rel = 0; 
                    {
                        std::lock_guard<std::mutex> lock(settings::player_relations::relations_mutex);
                        auto rel_it = settings::player_relations::relations.find(player.name);
                        if (rel_it != settings::player_relations::relations.end()) {
                            rel = rel_it->second;
                        }
                    }

                    char selectable_label[256];
                    std::string label_str;
                    if (!player.display_name.empty() && player.display_name != player.name)
                    {
                        label_str = player.display_name + " (@" + player.name + ")";
                    }
                    else
                    {
                        label_str = player.name;
                    }

                    if (rel == 1) {
                        sprintf_s(selectable_label, "[T] %s", label_str.c_str());
                    } else if (rel == 2) {
                        sprintf_s(selectable_label, "[E] %s", label_str.c_str());
                    } else {
                        sprintf_s(selectable_label, "%s", label_str.c_str());
                    }

                    if (rel == 1) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 120, 255)); 
                    else if (rel == 2) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 60, 60, 255)); 
                    else ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 220, 220, 255)); 

                    if (ImGui::Selectable(selectable_label, is_selected))
                    {
                        selected_player = player;
                    }
                    
                    ImGui::PopStyleColor();
                }
            }

            ImGui::EndChild(); 
            ImGui::EndChild(); 
        }
        else if (current_page == 1)
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            ImGui::BeginChild("Player Controls Window", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            if (selected_player.instance.address != 0)
            {
                bool player_exists = false;
                cache::entity_t current_player_state = {};
                
                if (snapshot_ptr)
                {
                    for (const auto& player : *snapshot_ptr) {
                        if (player.instance.address == selected_player.instance.address) {
                            player_exists = true;
                            current_player_state = player;
                            break;
                        }
                    }
                }

                if (!player_exists) {
                    ImGui::Text("Selected player left the game.");
                    ImGui::Spacing();
                    if (styled_button("Clear Selection", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, 26.f))) {
                        selected_player = {};
                    }
                }
                else {
                    float health = 0.0f;
                    float max_health = 100.0f;
                    if (current_player_state.humanoid.address != 0) {
                        health = current_player_state.humanoid.get_health();
                        max_health = current_player_state.humanoid.get_max_health();
                    }

                    float dist = -1.f;
                    auto local_hrp_it = local_player_snapshot.parts.find("HumanoidRootPart");
                    auto enemy_hrp_it = current_player_state.parts.find("HumanoidRootPart");
                    if (local_hrp_it != local_player_snapshot.parts.end() && enemy_hrp_it != current_player_state.parts.end()) {
                        rbx::primitive_t local_prim = local_hrp_it->second.get_primitive();
                        rbx::primitive_t enemy_prim = enemy_hrp_it->second.get_primitive();
                        math::vector3 lp = local_prim.get_position();
                        math::vector3 ep = enemy_prim.get_position();
                        
                        float dx = ep.x - lp.x;
                        float dy = ep.y - lp.y;
                        float dz = ep.z - lp.z;
                        dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    }

                    ImGui::TextColored(menu::accent_color, "TARGET DATA SHEET");
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    ImGui::Text("Display Name: "); ImGui::SameLine();
                    ImGui::TextColored(menu::accent_color, current_player_state.display_name.c_str());

                    ImGui::Text("Username:     "); ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1,1,1,1), "%s", current_player_state.name.c_str());
                    
                    ImGui::Text("User ID:      "); ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%lld", current_player_state.user_id);

                    ImGui::Text("Rig Type: "); ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), current_player_state.rig_type == 1 ? "R15 Joint" : "R6 Legacy");

                    ImGui::Text("Holding:  "); ImGui::SameLine();
                    ImGui::TextColored(menu::accent_color, current_player_state.tool_name.empty() ? "None" : current_player_state.tool_name.c_str());

                    ImGui::Text("Distance: "); ImGui::SameLine();
                    if (dist >= 0.0f) {
                        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "%.1f Studs", dist);
                    } else {
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Out of Range");
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::Text("Target Health Status:");
                    float hp_percentage = (max_health > 0.0f) ? (health / max_health) : 0.0f;
                    hp_percentage = std::clamp(hp_percentage, 0.0f, 1.0f);
                    
                    ImVec4 hp_col = ImVec4(1.0f - hp_percentage, hp_percentage, 0.1f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hp_col);
                    
                    char hp_buf[64];
                    sprintf_s(hp_buf, "%.0f / %.0f HP", health, max_health);
                    ImGui::ProgressBar(hp_percentage, ImVec2(ImGui::GetContentRegionAvail().x - 13.f, 18.0f), hp_buf);
                    ImGui::PopStyleColor();

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    int rel = 0;
                    {
                        std::lock_guard<std::mutex> lock(settings::player_relations::relations_mutex);
                        auto rel_it = settings::player_relations::relations.find(current_player_state.name);
                        if (rel_it != settings::player_relations::relations.end()) {
                            rel = rel_it->second;
                        }
                    }

                    ImGui::Text("Team Classification:");
                    ImGui::Spacing();

                    bool is_neutral = (rel == 0);
                    if (is_neutral) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(45, 45, 54, 255));
                    if (ImGui::Button("Neutral", ImVec2(90, 26))) {
                        std::lock_guard<std::mutex> lock(settings::player_relations::relations_mutex);
                        settings::player_relations::relations[current_player_state.name] = 0;
                        notifications::add(current_player_state.name + " marked Neutral.", notifications::NotificationType::Info, 2.0f);
                    }
                    if (is_neutral) ImGui::PopStyleColor();

                    ImGui::SameLine();

                    bool is_teammate = (rel == 1);
                    if (is_teammate) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 160, 80, 255));
                    if (ImGui::Button("Teammate", ImVec2(90, 26))) {
                        std::lock_guard<std::mutex> lock(settings::player_relations::relations_mutex);
                        settings::player_relations::relations[current_player_state.name] = 1;
                        notifications::add(current_player_state.name + " marked as Teammate.", notifications::NotificationType::Info, 2.0f);
                    }
                    if (is_teammate) ImGui::PopStyleColor();

                    ImGui::SameLine();

                    bool is_enemy = (rel == 2);
                    if (is_enemy) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(180, 40, 40, 255));
                    if (ImGui::Button("Enemy", ImVec2(90, 26))) {
                        std::lock_guard<std::mutex> lock(settings::player_relations::relations_mutex);
                        settings::player_relations::relations[current_player_state.name] = 2;
                        notifications::add(current_player_state.name + " marked as Enemy!", notifications::NotificationType::Info, 2.0f);
                    }
                    if (is_enemy) ImGui::PopStyleColor();

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::Text("Graphical Shell Interventions:");
                    ImGui::Spacing();

                    ImGui::Checkbox("Legit Teleport Mode", &settings::expl::legit_teleport);
                    if (settings::expl::legit_teleport)
                    {
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 20.f);
                        SliderFloatWithInput("Glide Speed##Target", &settings::expl::legit_teleport_speed, 10.0f, 1000.0f, "%.0f");
                        SliderIntWithInput("Step Delay (ms)##Target", &settings::expl::legit_teleport_delay, 5, 100);
                        ImGui::PopItemWidth();
                        ImGui::Spacing();
                    }

                    if (styled_button("Teleport To Target Location", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, 26.f))) {
                        if (enemy_hrp_it != current_player_state.parts.end()) {
                            math::vector3 target_pos = enemy_hrp_it->second.get_primitive().get_position();
                            target_pos.y += 2.0f; 
                            TeleportTo(target_pos);
                            if (settings::expl::legit_teleport) {
                                notifications::add("Gliding smoothly to: " + current_player_state.name, notifications::NotificationType::Success, 3.0f);
                            } else {
                                notifications::add("Teleported directly to: " + current_player_state.name, notifications::NotificationType::Success, 3.0f);
                            }
                        } else {
                            notifications::add("Failed: RootPart coordinates missing!", notifications::NotificationType::Error, 3.0f);
                        }
                    }

                    bool is_aimbot_locked = false;
                    {
                        std::lock_guard<std::mutex> lock(rbx::aimbot::g_aimbot_mutex);
                        is_aimbot_locked = (rbx::aimbot::g_aimbot_manual_locked && rbx::aimbot::g_aimbot_manual_target.instance.address == current_player_state.instance.address);
                    }
                    std::string secure_aimbot_lock_label = is_aimbot_locked ? "Release Aimbot Target Lock" : "Secure Aimbot Target Lock";
                    if (styled_button(secure_aimbot_lock_label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 13.f, 26.f))) {
                        if (is_aimbot_locked) {
                            rbx::aimbot::unlock_target();
                            notifications::add("Released aimbot target lock.", notifications::NotificationType::Info, 2.0f);
                        } else {
                            rbx::aimbot::lock_target(current_player_state);
                            notifications::add("Aimbot Target Lock Established: " + current_player_state.name, notifications::NotificationType::Success, 3.0f);
                        }
                    }
                }
            }
            else
            {
                ImGui::Text("Select a player from the directory");
                ImGui::TextDisabled("to initiate real-time diagnostics");
                ImGui::TextDisabled("and custom tactical options.");
            }

            ImGui::EndChild(); 
            ImGui::PopStyleVar();
        }
        break;
    }
    case 8:
    {
        if (current_page == 0)
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::BeginChild("Silent Aim Controls", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            ImGui::Checkbox("Enable Silent Aim", &settings::new_silent::enabled);
            if (settings::new_silent::enabled)
            {
                ImGui::SameLine();
                inline_keybind_button("silent_keybind", &settings::new_silent::keybind, &settings::new_silent::keybind_mode);
            }

            ImGui::Checkbox("Sticky Aim", &settings::new_silent::sticky_aim);
            ImGui::Checkbox("Draw FOV", &settings::new_silent::draw_fov);
            ImGui::SameLine();
            if (add_tooltip_trigger("silent_fov_tooltip")) {
                if (begin_tooltip_popup("silent_fov_tooltip", ImVec2(290, 180))) {
                    ImGui::BeginChild("Silent FOV Settings", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y), true, ImGuiWindowFlags_NoBackground);

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.f);
                    SliderFloatWithInput("size", &settings::new_silent::fov, 1.0f, 1000.0f, "%.1f");

                    ImGui::Checkbox("Fill", &settings::new_silent::filled_fov);
                    ImGui::SameLine();
                    ImGui::ColorEdit4("FOV Color", settings::new_silent::fov_color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);

                    ImGui::Checkbox("Rotate", &settings::new_silent::rotate_fov);
                    ImGui::Checkbox("Rainbow", &settings::new_silent::rainbow_fov);

                    ImGui::EndChild();
                    end_tooltip_popup("silent_fov_tooltip", ImVec2(290, 180));
                }
            }

            ImGui::EndChild();
        }
        else
        {
            ImGui::SetCursorPos(ImVec2(22.f, 114.f));
            ImGui::BeginChild("Silent Aim Settings", ImVec2(ImGui::GetContentRegionAvail().x - 13.f, ImGui::GetContentRegionAvail().y - 13.f), true, ImGuiWindowFlags_NoBackground);

            const char* targeting_modes[] = { "Closest to Crosshair", "Closest Distance", "Lowest Health" };
            ImGui::Combo("Target Mode", &settings::new_silent::target_mode, targeting_modes, IM_ARRAYSIZE(targeting_modes));

            const char* silent_modes[] = { "Mouse Snap (Universal)", "Memory Write (Silent)" };
            ImGui::Combo("Silent Aim Method", &settings::new_silent::silent_mode, silent_modes, IM_ARRAYSIZE(silent_modes));

            const char* silent_parts[] = {
                "Head", "Upper Torso", "Lower Torso", "HumanoidRootPart", "Smart (Head/Torso)", "Random"
            };
            ImGui::Combo("Target Part", &settings::new_silent::aim_part, silent_parts, IM_ARRAYSIZE(silent_parts));

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 20.f);
            ImGui::SliderInt("Hit Chance", &settings::new_silent::hit_chance, 0, 100, "%d%%");

            ImGui::Checkbox("FOV Check", &settings::new_silent::fov_check);
            ImGui::Checkbox("Knocked Check", &settings::new_silent::knocked_check);
            ImGui::Checkbox("Wall Check", &settings::new_silent::wall_check);
            ImGui::Checkbox("Team Check", &settings::new_silent::team_check);

            ImGui::Checkbox("Enable Prediction", &settings::new_silent::prediction_enabled);
            if (settings::new_silent::prediction_enabled) {
                ImGui::Checkbox("Auto-Prediction", &settings::new_silent::auto_prediction);
                if (!settings::new_silent::auto_prediction) {
                    SliderFloatWithInput("predict scale x", &settings::new_silent::prediction_scale_x, 0.0f, 10.0f, "%.2f");
                    SliderFloatWithInput("predict scale y", &settings::new_silent::prediction_scale_y, 0.0f, 10.0f, "%.2f");
                }
            }

            ImGui::EndChild();
        }
        break;
    }
    }

#undef SetCursorPos

    ImGui::End();
    ImGui::PopStyleVar();

    
    dex_explorer::DexExplorer::render();
}

void render_t::render_visuals()
{
    esp::run();

    render_feature_indicator();
}

void render_t::render_feature_indicator()
{
}

void render_t::render_notifications()
{
    notifications::update();
    notifications::render();
}
