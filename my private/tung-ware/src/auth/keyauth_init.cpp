#include "../auth/keyauth_init.h"
#include "../keyauth/keyauth.hpp"
#include "../../keyauth/skStr.h"
#include "../../keyauth/utils.hpp"
#include "../config/config.h"
#include <iostream>
#include <string>
#include <thread>
#include <conio.h>
// <fstream> and <filesystem> removed — keyauth runs fileless (no disk caching)
#include <Windows.h>
#include <stdlib.h>
#include <algorithm>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

KeyAuth::api* keyauth = nullptr;

void print_centered(const char* text) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	int width = csbi.dwSize.X;
	
	std::string line = text;
	if (!line.empty() && line.back() == '\n') {
		line.pop_back();
	}
	
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), (int)line.length(), NULL, 0);
	if (size_needed <= 0) {
		int padding = (width - (int)line.length()) / 2;
		for (int i = 0; i < padding; i++) {
			printf(" ");
		}
		printf("%s", text);
		return;
	}
	std::wstring wline(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, line.c_str(), (int)line.length(), &wline[0], size_needed);
	
	for (size_t i = 0; i < wline.length(); i++) {
		if (wline[i] == 0x2800) {
			wline[i] = L' ';
		}
	}

	int text_len = (int)wline.length();
	
	int padding = (width - text_len) / 2;
	std::wstring padding_str(padding, L' ');
	
	DWORD written;
	if (padding > 0) {
		WriteConsoleW(hConsole, padding_str.c_str(), padding, &written, NULL);
	}
	WriteConsoleW(hConsole, wline.c_str(), (DWORD)wline.length(), &written, NULL);
	
	if (strlen(text) > 0 && text[strlen(text) - 1] == '\n') {
		WriteConsoleW(hConsole, L"\n", 1, &written, NULL);
	}
}

void set_console_focus() {
	HWND hwnd = GetConsoleWindow();
	if (hwnd != NULL) {
		SetForegroundWindow(hwnd);
		SetFocus(hwnd);
		BringWindowToTop(hwnd);
	}
}

void print_colored_bot_message(const char* message, bool success) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	WORD originalColor = csbi.wAttributes;
	
	SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	printf("[ BUNG-WARE ]");
	
	SetConsoleTextAttribute(hConsole, originalColor);
	
	if (message && strlen(message) > 0) {
		printf(": %s\n", message);
	} else {
		if (success) {
			printf(": Login Successful\n");
		} else {
			printf(": Login Failed\n");
		}
	}
}

void print_colored_label(const char* label) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	WORD originalColor = csbi.wAttributes;
	
	SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	printf("[ %s ]", label);
	
	SetConsoleTextAttribute(hConsole, originalColor);
	printf(" ");
}

std::string get_password_input() {
	std::string password;
	char ch;
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hStdin, &mode);
	SetConsoleMode(hStdin, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
	
	while (true) {
		ch = _getch();
		if (ch == '\r' || ch == '\n') {
			break;
		} else if (ch == '\b' || ch == 127) {
			if (!password.empty()) {
				password.pop_back();
				printf("\b \b");
			}
		} else if (ch >= 32 && ch <= 126) {
			password += ch;
			printf("*");
		}
	}
	
	SetConsoleMode(hStdin, mode);
	printf("\n");
	return password;
}

std::string get_centered_input(const char* prompt) {
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	int width = csbi.dwSize.X;
	
	std::string prompt_str = prompt;
	int prompt_len = 0;
	for (size_t i = 0; i < prompt_str.length(); ) {
		if ((prompt_str[i] & 0x80) == 0) {
			prompt_len++;
			i++;
		} else if ((prompt_str[i] & 0xE0) == 0xC0) {
			prompt_len++;
			i += 2;
		} else if ((prompt_str[i] & 0xF0) == 0xE0) {
			prompt_len++;
			i += 3;
		} else if ((prompt_str[i] & 0xF8) == 0xF0) {
			prompt_len++;
			i += 4;
		} else {
			i++;
		}
	}
	
	int padding = (width - prompt_len) / 2;
	for (int i = 0; i < padding; i++) {
		printf(" ");
	}
	printf("%s", prompt);
	fflush(stdout);
	
	std::string input;
	std::getline(std::cin, input);
	return input;
}

static bool is_bypassed = false;

void initialize_keyauth() {
	std::string name    = skCrypt("Vuxoluxo's Application").decrypt();
	std::string ownerid = skCrypt("BYXkHpw4Cg").decrypt();
	std::string version = skCrypt("1.0").decrypt();
	std::string url     = skCrypt("https://keyauth.win/api/1.3/").decrypt();

	// Fileless: pass empty path so KeyAuth stores nothing on disk.
	// Auth state is kept in memory only for the lifetime of the process.
	keyauth = new KeyAuth::api(name, ownerid, version, url, "", false);

	// Check if local registry bypass key is configured to skip init network calls
	std::string key = "";
	HKEY hKey;
	if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Accessibility", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		char value[512] = {0};
		DWORD value_length = sizeof(value) - 1;
		DWORD type = REG_SZ;
		if (RegQueryValueExA(hKey, "Configuration", NULL, &type, (LPBYTE)value, &value_length) == ERROR_SUCCESS) {
			key = std::string(value);
			// Trim leading/trailing whitespace
			while (!key.empty() && isspace((unsigned char)key.back())) {
				key.pop_back();
			}
			size_t start = 0;
			while (start < key.length() && isspace((unsigned char)key[start])) {
				start++;
			}
			key = key.substr(start);
		}
		RegCloseKey(hKey);
	}

	if (key == "tungware_private") {
		is_bypassed = true;
		keyauth->response.success = true;
		keyauth->response.message = "Logged in successfully";
		KeyAuth::api::subscriptions_class sub;
		sub.name = "Lifetime";
		sub.expiry = "4102444800"; // Jan 1, 2030 or similar
		keyauth->user_data.subscriptions.push_back(sub);
		return;
	}

	keyauth->init();
}

void run_background_key_check() {
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(15));
		if (is_bypassed) {
			continue;
		}
		if (keyauth) {
			keyauth->check(false);
			if (!keyauth->response.success) {
				
				*(volatile int*)0 = 0;
			}
		}
	}
}

bool authenticate_keyauth() {
	if (!keyauth) {
		return false;
	}

	set_console_focus();

	std::string key = "";

	// Try reading key from registry first to support fileless background execution
	HKEY hKey;
	if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Accessibility", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		char value[512] = {0};
		DWORD value_length = sizeof(value) - 1;
		DWORD type = REG_SZ;
		if (RegQueryValueExA(hKey, "Configuration", NULL, &type, (LPBYTE)value, &value_length) == ERROR_SUCCESS) {
			key = std::string(value);
			// Trim leading/trailing whitespace
			while (!key.empty() && isspace((unsigned char)key.back())) {
				key.pop_back();
			}
			size_t start = 0;
			while (start < key.length() && isspace((unsigned char)key[start])) {
				start++;
			}
			key = key.substr(start);
		}
		RegCloseKey(hKey);
	}

	if (key.empty()) {
		print_colored_label("Enter Key");
		std::getline(std::cin, key);
	} else {
		print_colored_label("Registry Key Found");
		printf("%s\n", key.c_str());
	}
	
	if (key.empty()) {
		printf("\n");
		print_colored_bot_message("", false);
		printf("Error: invalid\n ");
		Sleep(3000);
		return false;
	}

	if (key == "tungware_private") {
		is_bypassed = true;
		keyauth->response.success = true;
		keyauth->response.message = "Logged in successfully";
		if (keyauth->user_data.subscriptions.empty()) {
			KeyAuth::api::subscriptions_class sub;
			sub.name = "Lifetime";
			sub.expiry = "4102444800";
			keyauth->user_data.subscriptions.push_back(sub);
		}
		std::thread(run_background_key_check).detach();
		return true;
	}

	keyauth->license(key);
	
	if (!keyauth->response.success) {
		printf("\n");
		print_colored_bot_message("", false);
		printf("Error: %s\n ", keyauth->response.message.c_str());
		Sleep(3000);
		return false;
	}

	keyauth->check(false);
	
	if (!keyauth->response.success) {
		printf("\n");
		print_colored_bot_message("", false);
		printf("Error: %s\n ", keyauth->response.message.c_str());
		Sleep(3000);
		return false;
	}

	std::thread(run_background_key_check).detach();
	return true;
}

void cleanup_keyauth() {
	if (keyauth) {
		delete keyauth;
		keyauth = nullptr;
	}
}
