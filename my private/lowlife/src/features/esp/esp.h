#pragma once
#include <string>
#include <cfloat>
#include <imgui/imgui.h>

struct esp_render_t
{
	ImFont* visitor = nullptr;
	ImFont* weapon_icon_font = nullptr;

	void DrawTextWithSpacing(ImDrawList* draw, ImFont* font, float font_size, ImVec2 pos, ImU32 col, const std::string& text, float char_spacing = 1.0f) {
		if (!font || text.empty()) return;
		
		float x_offset = 0.0f;
		for (size_t i = 0; i < text.length(); i++) {
			char c = text[i];
			char char_str[2] = { c, '\0' };
			
			ImVec2 char_pos = ImVec2(pos.x + x_offset, pos.y);
			draw->AddText(font, font_size, char_pos, col, char_str);
			
			ImVec2 char_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, char_str);
			x_offset += char_size.x + char_spacing;
		}
	}

	void DrawTextWithSpacingAndOutline(ImDrawList* draw, ImFont* font, float font_size, ImVec2 pos, ImU32 col, ImU32 outline_col, const std::string& text, float char_spacing = 1.0f) {
		if (!font || text.empty()) return;
		
		float x_offset = 0.0f;
		for (size_t i = 0; i < text.length(); i++) {
			char c = text[i];
			char char_str[2] = { c, '\0' };
			
			ImVec2 char_pos = ImVec2(pos.x + x_offset, pos.y);
			
			// 4-way cross outline: sharper, cleaner, 2x faster than 8-way, and avoids looping text 9 times
			draw->AddText(font, font_size, ImVec2(char_pos.x - 1.0f, char_pos.y), outline_col, char_str);
			draw->AddText(font, font_size, ImVec2(char_pos.x + 1.0f, char_pos.y), outline_col, char_str);
			draw->AddText(font, font_size, ImVec2(char_pos.x, char_pos.y - 1.0f), outline_col, char_str);
			draw->AddText(font, font_size, ImVec2(char_pos.x, char_pos.y + 1.0f), outline_col, char_str);
			
			// Draw main character
			draw->AddText(font, font_size, char_pos, col, char_str);
			
			// Calc size exactly once per character instead of 9 times!
			ImVec2 char_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, char_str);
			x_offset += char_size.x + char_spacing;
		}
	}

	void draw_health_bar(ImDrawList* draw, float max, float current, ImVec2 pos, ImVec2 size, float alpha_factor = 1.0f, bool show_text = true);
};

inline esp_render_t Visualize;

namespace esp
{
	void run();
}