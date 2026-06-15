#pragma once
#include <Windows.h>
#include <wincrypt.h>
#include <winhttp.h>
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

namespace updater {
    // Current Local Loader Version (fallback matches server releases.json)
    inline const std::string CURRENT_VERSION = "1.0.29";
    
    // Configurable Update Distribution Endpoint
    inline const std::wstring SERVER_HOST = L"localhost";
    inline const INTERNET_PORT SERVER_PORT = 3000; 

    // Helper: URL parsing
    inline void parse_server_url(const std::string& url, std::wstring& out_host, INTERNET_PORT& out_port, bool& out_secure) {
        out_host = L"localhost";
        out_port = 3000;
        out_secure = false;

        if (url.empty()) return;

        std::string scheme, host_port;
        size_t scheme_end = url.find("://");
        if (scheme_end != std::string::npos) {
            scheme = url.substr(0, scheme_end);
            host_port = url.substr(scheme_end + 3);
        } else {
            host_port = url;
        }

        std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::tolower);
        if (scheme == "https") {
            out_secure = true;
            out_port = 443;
        } else if (scheme == "http") {
            out_secure = false;
            out_port = 80;
        }

        size_t path_start = host_port.find('/');
        std::string host_port_only = (path_start != std::string::npos) ? host_port.substr(0, path_start) : host_port;

        size_t colon = host_port_only.find(':');
        std::string host_str;
        if (colon != std::string::npos) {
            host_str = host_port_only.substr(0, colon);
            std::string port_str = host_port_only.substr(colon + 1);
            try {
                out_port = static_cast<INTERNET_PORT>(std::stoi(port_str));
            } catch (...) {}
        } else {
            host_str = host_port_only;
        }

        out_host.assign(host_str.begin(), host_str.end());
    }

    // Helper: Read ServerUrl from registry
    inline std::string get_registry_server_url() {
        HKEY hKey;
        std::string server_url = "";
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Accessibility", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char value[512] = {0};
            DWORD value_length = sizeof(value) - 1;
            DWORD type = REG_SZ;
            if (RegQueryValueExA(hKey, "ServerUrl", NULL, &type, (LPBYTE)value, &value_length) == ERROR_SUCCESS) {
                server_url = value;
            }
            RegCloseKey(hKey);
        }
        return server_url;
    }

    inline std::filesystem::path get_actual_workspace() {
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

    inline std::filesystem::path get_actual_exe_path() {
        char current_exe_path[MAX_PATH];
        GetModuleFileNameA(NULL, current_exe_path, MAX_PATH);
        std::filesystem::path p(current_exe_path);
        std::string filename = p.filename().string();
        
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
        if (filename == "dllhost.exe" || filename == "svchost.exe") {
            return get_actual_workspace() / "RobloxPlayerBeta.exe";
        }
        return p;
    } 

    // Helper: Trim Whitespace
    inline std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    // Helper: Parse value by key from a JSON string (avoiding external json dependency)
    inline std::string parse_json_value(const std::string& json, const std::string& key) {
        size_t key_pos = json.find("\"" + key + "\"");
        if (key_pos == std::string::npos) return "";
        
        size_t colon_pos = json.find(":", key_pos);
        if (colon_pos == std::string::npos) return "";
        
        size_t val_start = json.find_first_not_of(" \t\"", colon_pos + 1);
        if (val_start == std::string::npos) return "";
        
        size_t val_end;
        if (json[val_start - 1] == '"') { // String value
            val_end = json.find("\"", val_start);
        } else { // Numeric or boolean value
            val_end = json.find_first_of(" \t,}", val_start);
        }
        
        if (val_end == std::string::npos) return "";
        return json.substr(val_start, val_end - val_start);
    }

    // Helper: Compute MD5 Hash of the running executable using Windows CryptoAPI
    inline std::string get_current_file_md5() {
        std::filesystem::path szPath = get_actual_exe_path();

        HANDLE hFile = CreateFileA(szPath.string().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return "";

        HCRYPTPROV hProv = 0;
        HCRYPTPROV hHash = 0;
        if (!CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
            CloseHandle(hFile);
            return "";
        }

        if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
            CryptReleaseContext(hProv, 0);
            CloseHandle(hFile);
            return "";
        }

        BYTE rgbFile[1024];
        DWORD cbRead = 0;
        while (ReadFile(hFile, rgbFile, 1024, &cbRead, NULL) && cbRead > 0) {
            CryptHashData(hHash, rgbFile, cbRead, 0);
        }

        std::string md5_str = "";
        BYTE rgbHash[16];
        DWORD cbHash = 16;
        if (CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0)) {
            char szHex[33];
            for (int i = 0; i < 16; i++) {
                sprintf_s(szHex + (i * 2), 3, "%02x", rgbHash[i]);
            }
            md5_str = szHex;
        }

        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        CloseHandle(hFile);
        return md5_str;
    }

    // Perform Self Update and Process Replacement
    inline bool run() {
        // Retrieve actual workspace path to check for developer environment
        std::filesystem::path p_check = get_actual_workspace();
        std::string path_str = p_check.string();

        // Skip updater check entirely for developer builds (build/Release/Debug folders)
        if (path_str.find("\\build") != std::string::npos || 
            path_str.find("\\Build") != std::string::npos ||
            path_str.find("\\release") != std::string::npos || 
            path_str.find("\\Release") != std::string::npos ||
            path_str.find("\\debug") != std::string::npos || 
            path_str.find("\\Debug") != std::string::npos ||
            path_str.find("\\x64") != std::string::npos) {
            printf("[ UPDATER ] Developer build detected. Skipping auto-update checking.\n");
            return true;
        }

        printf("[ UPDATER ] Checking for software updates...\n");

        std::wstring server_host = SERVER_HOST;
        INTERNET_PORT server_port = SERVER_PORT;
        bool is_secure = false;

        std::string reg_url = get_registry_server_url();
        if (!reg_url.empty()) {
            parse_server_url(reg_url, server_host, server_port, is_secure);
            wprintf(L"[ UPDATER ] Using dynamic update server: %S (Host: %s, Port: %d, Secure: %s)\n",
                reg_url.c_str(), server_host.c_str(), server_port, is_secure ? L"YES" : L"NO");
        } else {
            wprintf(L"[ UPDATER ] Using default update server: %s:%d\n", server_host.c_str(), server_port);
        }

        HINTERNET hSession = WinHttpOpen(L"LOWLIFE-SelfUpdater/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        
        if (!hSession) return false;

        // Set strict 1-second timeouts so we don't hang startup if the update server is offline
        WinHttpSetTimeouts(hSession, 1000, 1000, 1000, 1000);

        HINTERNET hConnect = WinHttpConnect(hSession, server_host.c_str(), server_port, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/api/release/latest",
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, is_secure ? WINHTTP_FLAG_SECURE : 0);
        
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        std::string json_response;
        BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        
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
                    json_response += buffer;
                } else {
                    break;
                }
            } while (dwSize > 0);
        }

        WinHttpCloseHandle(hRequest);

        if (json_response.empty()) {
            printf("[ UPDATER ] Warning: Failed to connect to update server. Skipping.\n");
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        std::string server_version = parse_json_value(json_response, "version");
        std::string server_hash = parse_json_value(json_response, "md5");
        if (server_version.empty() || server_hash.empty()) {
            printf("[ UPDATER ] Failed to parse server release details.\n");
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        std::string current_hash = get_current_file_md5();
        if (server_hash == current_hash || server_version == CURRENT_VERSION) {
            printf("[ UPDATER ] Executable is up-to-date (v%s).\n", server_version.c_str());
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return true;
        }

        // New Update Detected!
        printf("\n==================================================\n");
        printf("[ UPDATER ] A NEW VERSION IS LIVE! (v%s)\n", server_version.c_str());
        printf("[ UPDATER ] MD5 mismatch: local[%s] vs server[%s]\n", 
            current_hash.empty() ? "unknown" : current_hash.substr(0, 8).c_str(), 
            server_hash.substr(0, 8).c_str());
        std::string changelog = parse_json_value(json_response, "changelog");
        printf("[ UPDATER ] Changelog: %s\n", changelog.c_str());
        printf("[ UPDATER ] Downloading release build...\n");
        printf("==================================================\n\n");

        // Download the New Executable Binary
        hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/api/release/download-binary",
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, is_secure ? WINHTTP_FLAG_SECURE : 0);

        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (bResults) {
            bResults = WinHttpReceiveResponse(hRequest, NULL);
        }

        std::vector<char> exe_buffer;
        if (bResults) {
            DWORD dwSize = 0;
            do {
                dwSize = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                if (dwSize == 0) break;
                
                std::vector<char> temp_buf(dwSize);
                DWORD dwDownloaded = 0;
                if (WinHttpReadData(hRequest, temp_buf.data(), dwSize, &dwDownloaded)) {
                    exe_buffer.insert(exe_buffer.end(), temp_buf.begin(), temp_buf.begin() + dwDownloaded);
                } else {
                    break;
                }
            } while (dwSize > 0);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        if (exe_buffer.empty()) {
            printf("[ UPDATER ] Error: Downloaded executable was empty!\n");
            return false;
        }

        // Retrieve parent directory and actual exe path
        std::filesystem::path p = get_actual_exe_path();
        std::string current_exe_name = "RobloxPlayerBeta.exe";
        std::filesystem::path parent_dir = get_actual_workspace();

        std::filesystem::path new_exe_path = parent_dir / "RobloxPlayerBeta_new.exe";
        std::filesystem::path bat_file_path = parent_dir / "updater.bat";

        // Save new binary using absolute path
        std::ofstream out_file(new_exe_path, std::ios::binary);
        if (!out_file.is_open()) {
            printf("[ UPDATER ] Error: Failed to write new binary to disk.\n");
            return false;
        }
        out_file.write(exe_buffer.data(), exe_buffer.size());
        out_file.close();

        // Write the background batch self-replacer script using absolute path
        std::ofstream bat_file(bat_file_path);
        if (bat_file.is_open()) {
            DWORD loader_pid = GetCurrentProcessId();

            bat_file << "@echo off\n";
            bat_file << ":loop\n";
            bat_file << "tasklist /FI \"PID eq " << loader_pid << "\" 2>NUL | find /I \"" << loader_pid << "\" >nul\n";
            bat_file << "if %errorlevel% equ 0 (\n";
            bat_file << "    timeout /t 1 /nobreak > nul\n";
            bat_file << "    goto loop\n";
            bat_file << ")\n";
            bat_file << "del /f /q \"" << p.string() << "\"\n";
            bat_file << "ren \"" << new_exe_path.string() << "\" \"" << current_exe_name << "\"\n";
            bat_file << "start \"\" \"" << p.string() << "\"\n";
            bat_file << "del \"%~f0\"\n";
            bat_file.close();
        } else {
            printf("[ UPDATER ] Error: Failed to write updater batch file.\n");
            return false;
        }

        printf("[ UPDATER ] Applying changes and restarting LowLife...\n");

        // Execute batch file silently in the background
        STARTUPINFOA si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE; // Run completely invisible to user
        PROCESS_INFORMATION pi;
        
        std::string cmd_line = "cmd.exe /c \"" + bat_file_path.string() + "\"";
        if (CreateProcessA(NULL, (LPSTR)cmd_line.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, parent_dir.string().c_str(), &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            
            // Shut down current process to allow updater.bat to replace the file
            exit(0);
        } else {
            printf("[ UPDATER ] Error: Failed to trigger process self-updater.\n");
            return false;
        }

        return true;
    }
}
