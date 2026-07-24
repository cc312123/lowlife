#include <cstdint>
#include <chrono>
#include <thread>
#include <string>
#include <cstdio>
#include <Windows.h>
#include <timeapi.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <VersionHelpers.h>
#include <filesystem>
#include <atomic>
#include <array>
#include <algorithm>

#pragma comment(lib, "winmm.lib")

#include <memory/memory.h>
#include <memory/driver.h>
#include <sdk/offsets.h>
#include <sdk/sdk.h>
#include <game/game.h>
#include <cache/cache.h>
#include <render/render.h>
#include <render/notifications.h>
#include <features/expl/walkspeed.h>
#include <features/expl/freezeplayer.h>
#include <features/expl/fly.h>
#include <features/expl/misc_exploits.h>
#include <features/aimbot/aimbot.h>
#include <features/triggerbot/triggerbot.h>
#include <features/silent/silent.h>
#include <check/typing_check.h>
#include <game/rescan.h>
#include <auth/keyauth_init.h>
#include <auth/updater.h>
#include <auth/web_server.h>
#include <bypass/kill_crash_handler.h>
#include <bypass/pc_check.h>
#include "../protection/protection/antidebug.h"

void print_colored_bot_message(const char* msg, bool success);
void debugger_detection();
void AutoRescanHandler();

namespace tungware {
    namespace utils {
        [[nodiscard]] bool is_elevated() noexcept;
        void set_console_font() noexcept;
        void print_colored_message(const char* msg, bool success) noexcept;
        [[noreturn]] void self_destruct() noexcept;
    }

    namespace detection {
        void debugger_detection_thread() noexcept;
    }

    namespace bypass {
        namespace pc_check {
            void run_pc_bypass() noexcept;
        }
        namespace process {
            void hide_from_roblox() noexcept;
            void spoof_process_info() noexcept;
        }
    }
}

namespace globals {
    static std::atomic<bool> cleanup_requested;
    inline std::atomic<bool> roblox_valid;
    static const char* const ROBLOX_PROCESS = "RobloxPlayerBeta.exe";
    static constexpr const char* HOST_FILES[6] = {
        "TUNGWAREHost.exe", "TUNGWARELoader.exe", "loader.exe", "host.exe",
        "injector.exe", "TUNGWARE.exe"
    };
    std::atomic<bool> keyauth_authenticated{false};
    std::atomic<bool> inject_requested{false};
}

static BOOL WINAPI cleanup_handler(DWORD ctrlType) noexcept {
    if (ctrlType == CTRL_CLOSE_EVENT || ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT) {
        globals::cleanup_requested = true;
        globals::roblox_valid = false;
        input::close();
        Sleep(500);
        tungware::utils::self_destruct();
    }
    return FALSE;
}

namespace tungware::bypass::pc_check {
    // Implemented in bypass/pc_check.cpp — full IAT hook + registry spoof
}

namespace tungware::bypass::process {
    void hide_from_roblox() noexcept {  }
    void spoof_process_info() noexcept {  }
}

namespace tungware::utils {
    bool is_elevated() noexcept {
        HANDLE token = nullptr;
        BOOL elevated = FALSE;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            TOKEN_ELEVATION elev = {};
            DWORD size = sizeof(elev);
            GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &size);
            elevated = elev.TokenIsElevated;
            CloseHandle(token);
        }
        return elevated != FALSE;
    }

    void set_console_font() noexcept {
        static const char font_name[] = "NSimSun";
        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_FONT_INFOEX cfi = { sizeof(cfi) };

        if (GetCurrentConsoleFontEx(console, FALSE, &cfi)) {
            MultiByteToWideChar(CP_ACP, 0, font_name, -1, cfi.FaceName, LF_FACESIZE);
            SetCurrentConsoleFontEx(console, FALSE, &cfi);
        }
    }

    void print_colored_message(const char* msg, bool success) noexcept {
        printf("\n");
        print_colored_bot_message("", success);
        printf("%s\n", msg);
    }

    std::filesystem::path get_actual_workspace() {
        char current_exe_path[MAX_PATH];
        GetModuleFileNameA(NULL, current_exe_path, MAX_PATH);
        std::filesystem::path p(current_exe_path);
        std::string filename = p.filename().string();
        
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
        if (filename == "dllhost.exe" || filename == "svchost.exe") {
            HKEY hKey;
            if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Accessibility", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                char value[512] = {0};
                DWORD value_length = sizeof(value) - 1;
                DWORD type = REG_SZ;
                if (RegQueryValueExA(hKey, "Workspace", NULL, &type, (LPBYTE)value, &value_length) == ERROR_SUCCESS) {
                    std::string ws(value);
                    RegCloseKey(hKey);
                    if (!ws.empty()) {
                        return std::filesystem::path(ws);
                    }
                }
                RegCloseKey(hKey);
            }
        }
        return p.parent_path();
    }

    std::filesystem::path get_actual_exe_path() {
        char current_exe_path[MAX_PATH];
        GetModuleFileNameA(NULL, current_exe_path, MAX_PATH);
        std::filesystem::path p(current_exe_path);
        std::string filename = p.filename().string();
        
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
        if (filename == "dllhost.exe" || filename == "svchost.exe") {
            return get_actual_workspace() / "RobloxCrashHandler.exe";
        }
        return p;
    }

    [[noreturn]] void self_destruct() noexcept {
        printf("[CLEANUP] Executing self-destruct...\n");

        if (memory && memory->get_process_handle()) {
            CloseHandle(memory->get_process_handle());
        }

        std::filesystem::path dir = get_actual_workspace();
        std::filesystem::path exe_path = get_actual_exe_path();
        std::string path_str = dir.string();

        
        if (path_str.find("\\build") != std::string::npos || 
            path_str.find("\\Build") != std::string::npos ||
            path_str.find("\\release") != std::string::npos || 
            path_str.find("\\Release") != std::string::npos ||
            path_str.find("\\debug") != std::string::npos || 
            path_str.find("\\Debug") != std::string::npos ||
            path_str.find("\\x64") != std::string::npos ||
            path_str.find("my private") != std::string::npos ||
            path_str.find("my_private") != std::string::npos ||
            path_str.find("My Private") != std::string::npos ||
            path_str.find("My_Private") != std::string::npos) {
            printf("[CLEANUP] Developer build detected. Skipping file deletion.\n");
            timeEndPeriod(1);
            ExitProcess(0);
        }

        for (int i = 0; i < 6; ++i) {
            try {
                std::filesystem::remove(dir / globals::HOST_FILES[i]);
            }
            catch (...) {}
        }

        char temp[MAX_PATH];
        if (GetTempPathA(MAX_PATH, temp)) {
            std::string temp_dir(temp);
            for (int i = 0; i < 6; ++i) {
                try {
                    std::filesystem::remove(temp_dir + globals::HOST_FILES[i]);
                }
                catch (...) {}
            }
        }

        char cmd[512];
        sprintf_s(cmd, "timeout /t 1 /nobreak >nul & del /f /q \"%s\" >nul 2>&1", exe_path.string().c_str());
        STARTUPINFOA si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = {};
        CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
            nullptr, nullptr, &si, &pi);
        if (pi.hProcess) CloseHandle(pi.hProcess);
        if (pi.hThread) CloseHandle(pi.hThread);
        timeEndPeriod(1);
        ExitProcess(0);
    }
}

namespace tungware::detection {
    void debugger_detection_thread() noexcept {
        debugger_detection(); 
        while (!globals::cleanup_requested) {
            Sleep(1500); 
        }
    }
}

static bool initialize_roblox_objects() noexcept {
    static char buffer[256];

    
    uintptr_t module_base = memory->get_module_address();
    if (!module_base) return false;

    uintptr_t fake_dm = memory->read<uintptr_t>(module_base + Offsets::FakeDataModel::Pointer);
    uintptr_t real_dm = memory->read<uintptr_t>(fake_dm + Offsets::FakeDataModel::RealDataModel);

    game::datamodel = { real_dm };
    game::visengine = { memory->read<uintptr_t>(module_base + Offsets::VisualEngine::Pointer) };

    
    uintptr_t workspace = memory->read<uintptr_t>(real_dm + Offsets::DataModel::Workspace);
    game::workspace = { workspace };

    
    uintptr_t players = game::datamodel.find_first_child_by_class("Players").address;
    game::players = { players };

    
    uintptr_t local_player = memory->read<uintptr_t>(players + Offsets::Player::LocalPlayer);
    game::local_player = { local_player };

    rbx::player_t lp_obj{ local_player };
    std::string lp_name = lp_obj.get_name();

    
    bool found_model_instance_offset = false;
    for (uintptr_t offset = 0x100; offset <= 0x600; offset += 8) {
        uintptr_t potential_char = memory->read<uintptr_t>(local_player + offset);
        if (potential_char != 0 && (potential_char & 0x7) == 0 && potential_char > 0x10000) {
            rbx::nameable_t inst{ potential_char };
            std::string name = inst.get_name();
            std::string class_name = inst.get_class_name();
            if (class_name == "Model" && name == lp_name) {
                Offsets::Player::ModelInstance = offset;
                found_model_instance_offset = true;
                sprintf_s(buffer, "[SCAN] Dynamically resolved Player::ModelInstance offset to 0x%llx", offset);
                print_colored_bot_message(buffer, true);
                break;
            }
        }
    }

    if (!found_model_instance_offset) {
        sprintf_s(buffer, "[SCAN] Warning: Failed to dynamically resolve ModelInstance offset. Keeping current value: 0x%llx", (uintptr_t)Offsets::Player::ModelInstance);
        print_colored_bot_message(buffer, false);
    }

    game::local_character = { lp_obj.get_model_instance().address };

    sprintf_s(buffer, "base -> 0x%llx | datamodel -> 0x%llx | visengine -> 0x%llx",
        module_base, real_dm, game::visengine.address);
    print_colored_bot_message(buffer, true);

    sprintf_s(buffer, "workspace -> 0x%llx | players -> 0x%llx | local_player -> 0x%llx",
        workspace, players, local_player);
    print_colored_bot_message(buffer, true);

    sprintf_s(buffer, "local_character -> 0x%llx", game::local_character.address);
    print_colored_bot_message(buffer, true);

    
    sprintf_s(buffer, "local_player name -> %s", lp_name.c_str());
    print_colored_bot_message(buffer, true);

    
    auto player_list = game::players.get_children();
    sprintf_s(buffer, "player count -> %d", (int)player_list.size());
    print_colored_bot_message(buffer, true);
    for (auto& p : player_list) {
        rbx::player_t p_obj{ p.address };
        sprintf_s(buffer, "player in list -> %s | char -> 0x%llx", p_obj.get_name().c_str(), p_obj.get_model_instance().address);
        print_colored_bot_message(buffer, true);
    }

    return real_dm != 0 && workspace != 0 && players != 0 && local_player != 0 && game::local_character.address != 0;
}


static std::string get_roblox_version(HANDLE process_handle) noexcept {
    char path[MAX_PATH] = { 0 };
    DWORD size = sizeof(path);
    if (QueryFullProcessImageNameA(process_handle, 0, path, &size)) {
        std::filesystem::path fs_path(path);
        return fs_path.parent_path().filename().string();
    }
    return "";
}

static void monitor_roblox() noexcept {
    const char* roblox_proc = globals::ROBLOX_PROCESS;

    while (!globals::cleanup_requested) {
        if (!globals::roblox_valid) {
            
            if (!memory->find_process_id(roblox_proc)) {
                Sleep(500);
                continue;
            }

            tungware::utils::print_colored_message("Roblox detected! Attaching...", true);

            
            tungware::bypass::pc_check::run_pc_bypass();
            tungware::bypass::process::hide_from_roblox();
            tungware::bypass::process::spoof_process_info();

            if (!memory->attach_to_process(roblox_proc)) {
                tungware::utils::print_colored_message("Failed to attach to Roblox process. Retrying...", false);
                Sleep(2000);
                continue;
            }

            std::string running_version = get_roblox_version(memory->get_process_handle());
            if (!Offsets::Update(running_version)) {
                tungware::utils::print_colored_message("Warning: Offsets update from server failed.", false);
                tungware::utils::print_colored_message("Attempting to use compile-time offsets...", true);
            }

            bool init_success = false;
            
            for (int attempts = 0; attempts < 30; ++attempts) {
                if (!memory->find_process_id(roblox_proc)) {
                    break;
                }
                if (memory->find_module_address(roblox_proc) && initialize_roblox_objects()) {
                    init_success = true;
                    break;
                }
                tungware::utils::print_colored_message("Awaiting game loading / player spawn (retrying in 1s)...", true);
                Sleep(1000);
            }

            if (!init_success) {
                tungware::utils::print_colored_message("Initialization failed or Roblox closed. Resetting connection...", false);
                memory->detach_from_process();
                Sleep(2000);
                continue;
            }

            if (!InitializeStorage()) {
                tungware::utils::print_colored_message("Storage initialization failed. Resetting...", false);
                memory->detach_from_process();
                Sleep(2000);
                continue;
            }

            std::thread rescan_thread(AutoRescanHandler);
            rescan_thread.detach();
            rbx::new_silent::initialize();

            notifications::add("Tung-Ware Loaded Successfully!", notifications::NotificationType::Success, 5.0f);

            game::wnd = FindWindowA(nullptr, "Roblox");

            globals::roblox_valid = true;
        }
        else {
            
            if (!memory->find_process_id(roblox_proc)) {
                tungware::utils::print_colored_message("Roblox process ended. Exiting loader...", false);

                globals::roblox_valid = false;
                globals::cleanup_requested = true;

                StopAutoRescan();

                game::datamodel = { 0 };
                game::visengine = { 0 };
                game::workspace = { 0 };
                game::players = { 0 };
                game::local_player = { 0 };
                game::local_character = { 0 };
                game::wnd = nullptr;

                memory->detach_from_process();
                Sleep(500);
                tungware::utils::self_destruct();
            }
            Sleep(1000);
        }
    }
}

int main() {
    HWND console_window = GetConsoleWindow();
    if (console_window) {
        std::filesystem::path current_dir = tungware::utils::get_actual_workspace();
        std::string path_str = current_dir.string();
        std::transform(path_str.begin(), path_str.end(), path_str.begin(), ::tolower);
        bool is_dev = (path_str.find("\\build") != std::string::npos || 
                       path_str.find("\\release") != std::string::npos || 
                       path_str.find("\\debug") != std::string::npos || 
                       path_str.find("\\x64") != std::string::npos ||
                       path_str.find("my private") != std::string::npos ||
                       path_str.find("my_private") != std::string::npos);
        if (!is_dev) {
            ShowWindow(console_window, SW_HIDE);
        }
    }
    // MessageBoxA(NULL, "Tung Tung Tung Sahur!", "TUNG", MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    timeBeginPeriod(1);
    
    globals::cleanup_requested = false;
    globals::roblox_valid = false;

    
    SetConsoleCtrlHandler(cleanup_handler, TRUE);
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
    tungware::utils::set_console_font();
    input::init();

    
    if (!web_server::start()) {
        tungware::utils::print_colored_message("Failed to start local web server on port 9876", false);
        Sleep(3000);
        ExitProcess(0);
    }

    updater::run();

    initialize_keyauth();

    if (!authenticate_keyauth()) {
        Sleep(3000);
        web_server::stop();
        tungware::utils::self_destruct();
    }

    globals::keyauth_authenticated = true;
    tungware::utils::print_colored_message("Key verified! Awaiting activation hotkey (CapsLock + Enter)...", true);

    while (!globals::inject_requested) {
        bool is_caps_down = (GetAsyncKeyState(VK_CAPITAL) & 0x8000) != 0;
        bool is_enter_down = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
        if (is_caps_down && is_enter_down) {
            globals::inject_requested = true;
            break;
        }
        Sleep(50);
    }
    tungware::utils::print_colored_message("Activation signal received! Injecting...", true);

    // Do not stop local web server to allow multiple injections / web page reloads
    // web_server::stop();

    std::thread(tungware::detection::debugger_detection_thread).detach();
    std::thread(rbx::bypass::run).detach();
    std::thread(tungware::bypass::pc_check::watch_thread).detach();  // re-apply spoofs every 10s

    
    if (!render->create_window() || !render->create_device() || !render->create_imgui()) {
        tungware::utils::print_colored_message("Render initialization failed", false);
        Sleep(5000);
        tungware::utils::self_destruct();
    }

    
    std::thread check_thread(check::run);
    std::thread walkspeed_thread(walkspeed::run);
    std::thread freeze_thread(freezeplayer::run);
    std::thread fly_thread(fly::run);
    std::thread aimbot_thread(rbx::aimbot::run);
    std::thread botter_thread(botter::run);
    std::thread shot_detect_thread(shot_detect::run);
    std::thread color_detect_thread(color_detect::run);
    std::thread misc_exploits_thread(misc_exploits::run);
    std::thread cache_thread(cache::run);

    if (check_thread.joinable()) check_thread.detach();
    if (walkspeed_thread.joinable()) walkspeed_thread.detach();
    if (freeze_thread.joinable()) freeze_thread.detach();
    if (fly_thread.joinable()) fly_thread.detach();
    if (aimbot_thread.joinable()) aimbot_thread.detach();
    if (botter_thread.joinable()) botter_thread.detach();
    if (shot_detect_thread.joinable()) shot_detect_thread.detach();
    if (color_detect_thread.joinable()) color_detect_thread.detach();
    if (misc_exploits_thread.joinable()) misc_exploits_thread.detach();
    if (cache_thread.joinable()) cache_thread.detach();

    
    std::thread tickrate_thread([] {
        const int TICK_INTERVAL = 200;
        while (true) {
            if (globals::roblox_valid && !globals::cleanup_requested) {
                if (settings::expl::tickrate && game::workspace.address) {
                    uintptr_t world = memory->read<uintptr_t>(game::workspace.address + Offsets::Workspace::World);
                    if (world) {
                        memory->write<float>(world + Offsets::World::worldStepsPerSec, settings::expl::tickrate_amount);
                    }
                }
            }
            Sleep(TICK_INTERVAL);
        }
    });
    if (tickrate_thread.joinable()) tickrate_thread.detach();

    
    std::thread monitor_thread(monitor_roblox);
    if (monitor_thread.joinable()) monitor_thread.detach();

    
    while (!globals::cleanup_requested) {
        render->start_render();

        if (globals::roblox_valid) {
            render->render_visuals();
        }

        if (render->running) {
            render->render_menu();
        }

        render->render_notifications();
        render->end_render();

        Sleep(render->running ? 1 : 16);
    }

    cleanup_keyauth();
    web_server::stop();
    input::close();
    return 0;
}
