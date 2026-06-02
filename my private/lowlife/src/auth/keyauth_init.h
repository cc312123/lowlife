#pragma once
#include <string>

void print_centered(const char* text);
std::string get_centered_input(const char* prompt);
void print_colored_bot_message(const char* message, bool success);
void print_colored_label(const char* label);
std::string get_password_input();

namespace KeyAuth {
	class api;
}

extern KeyAuth::api* keyauth;

void initialize_keyauth();
bool authenticate_keyauth();
void cleanup_keyauth();

