
#pragma once

#include "../../ext/imgui/imgui_internal.h"
#include <d3d11.h>
#include <stdint.h>
#include "../settings.h"
#include <vector>
#include <unordered_map>
#include <string>

static int text_idx = 0;
static float transition_timer = 0.0f;
static float char_timer = 0.0f;
static int visible_chars = 0;
static bool is_fading = false;

struct TooltipState {
    bool is_open = false;
    ImVec2 position = ImVec2(0, 0);
    float alpha = 1.0f;
    char id[64] = "";
};

static std::vector<TooltipState> tooltip_states;

TooltipState* get_tooltip_state(const char* id) {
    for (auto& state : tooltip_states) {
        if (strcmp(state.id, id) == 0) {
            return &state;
        }
    }

    TooltipState new_state;
    strncpy_s(new_state.id, id, sizeof(new_state.id) - 1);
    tooltip_states.push_back(new_state);
    return &tooltip_states.back();
}

bool add_tooltip_trigger(const char* id, ImVec2 size = ImVec2(20, 20)) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;

    if (g.LastItemData.ID != 0) {
        float content_region_max = window->ContentRegionRect.Max.x;
        window->DC.CursorPos.x = content_region_max - size.x - 5.0f;
        window->DC.CursorPos.y -= 4.0f;
    }

    ImVec2 pos = window->DC.CursorPos;
    ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, window->GetID(id)))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, window->GetID(id), &hovered, &held);

    TooltipState* state = get_tooltip_state(id);

    if (pressed) {
        state->is_open = !state->is_open;
        if (state->is_open) {
            state->position = ImVec2(pos.x + size.x + 5, pos.y);
        }
    }

    float target_alpha = state->is_open ? 0.5f : 1.0f;
    float tween_speed = g.IO.DeltaTime * 8.0f;
    state->alpha += (target_alpha - state->alpha) * tween_speed;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    const char* symbol = state->is_open ? "-" : "+";
    ImVec2 text_pos = ImVec2(bb.Min.x + (size.x - ImGui::CalcTextSize(symbol).x) * 0.5f, bb.Min.y + (size.y - ImGui::CalcTextSize(symbol).y) * 0.5f);

    ImU32 outline_color = IM_COL32(0, 0, 0, (int)(255 * state->alpha));
    ImU32 text_color = IM_COL32(255, 255, 255, (int)(255 * state->alpha));

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            if (x == 0 && y == 0) continue;
            dl->AddText(ImVec2(text_pos.x + x, text_pos.y + y), outline_color, symbol);
        }
    }
    dl->AddText(text_pos, text_color, symbol);

    return state->is_open;
}

bool begin_tooltip_popup(const char* id, ImVec2 size = ImVec2(200, 100)) {
    TooltipState* state = get_tooltip_state(id);

    if (!state->is_open)
        return false;

    ImGui::SetNextWindowPos(state->position, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(size);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(24, 24, 28, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertFloat4ToU32(menu::accent_color));

    char popup_name[128];
    sprintf_s(popup_name, "##tooltip_%s", id);

    bool popup_open = ImGui::Begin(popup_name, nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_AlwaysAutoResize);

    return popup_open;
}

void end_tooltip_popup(const char* id, ImVec2 size = ImVec2(200, 100)) {
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

bool add_button(const char* label, const ImVec2& size)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    ImVec2 pos = window->DC.CursorPos;

    pos.x += 2;

    ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
    ImVec2 button_size = ImGui::CalcItemSize(size, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    ImRect bb(pos, ImVec2(pos.x + button_size.x, pos.y + button_size.y));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, window->GetID(label)))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, window->GetID(label), &hovered, &held);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    static float tween_progress = 0.0f;
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

    
    ImU32 bg_col = IM_COL32(30, 30, 36, 255);
    dl->AddRectFilled(bb.Min, bb.Max, bg_col, 5.0f);
    dl->AddRect(bb.Min, bb.Max, ImGui::ColorConvertFloat4ToU32(tweened_color), 5.0f, 0, 1.0f);

    ImVec2 text_pos = ImVec2(bb.Min.x + (button_size.x - label_size.x) * 0.5f, bb.Min.y + (button_size.y - label_size.y) * 0.5f);

    dl->AddText(ImVec2(text_pos.x + 1.f, text_pos.y + 1.f), IM_COL32(0, 0, 0, 180), label);
    dl->AddText(text_pos, IM_COL32_WHITE, label);

    return pressed;
}

inline bool add_tab(const char* label, int idx, bool sel, int total_tabs)
{
    static float tab_alphas[10] = { 0.3f };
    static float hover_alphas[10] = { 0.0f };
    static int initialized = 0;
    if (!initialized) {
        for (int i = 0; i < 10; i++) {
            tab_alphas[i] = 0.3f;
            hover_alphas[i] = 0.0f;
        }
        initialized = 1;
    }
    
    float target_alpha = sel ? 1.0f : 0.4f;
    float tween_speed = ImGui::GetIO().DeltaTime * 10.0f;
    tab_alphas[idx] += (target_alpha - tab_alphas[idx]) * tween_speed;
    
    ImVec4 text_color = sel ? menu::accent_color : ImVec4(0.85f, 0.85f, 0.90f, 1.0f);
    text_color.w = tab_alphas[idx];
    
    static float active_x = 0.0f;
    static float active_w = 0.0f;
    static bool slide_initialized = false;

    ImVec2 wpos = ImGui::GetWindowPos();
    ImVec2 wsz = ImGui::GetWindowSize();

    float sh_w = wsz.x - 28;
    float gap = 4.0f;
    float t_gaps = gap * (total_tabs - 1);
    float tw = (sh_w - t_gaps) / total_tabs;

    ImVec2 tsz = ImGui::CalcTextSize(label);
    ImVec2 bsz = ImVec2(tw, tsz.y + 8);

    if (idx == 0) {
        ImGui::SetCursorPosX(14);
        ImGui::SetCursorPosY(44.0f);
    }
    else {
        ImGui::SameLine(0, gap);
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));
    
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::Button(label, bsz);
    bool hovered = ImGui::IsItemHovered();
    
    float t_hover = (hovered && !sel) ? 0.15f : 0.0f;
    hover_alphas[idx] += (t_hover - hover_alphas[idx]) * tween_speed;
    
    ImDrawList* dl = ImGui::GetWindowDrawList();

    
    int hover_alpha_val = (int)(255 * hover_alphas[idx]);
    if (hover_alpha_val > 0) {
        dl->AddRectFilled(pos, ImVec2(pos.x + bsz.x, pos.y + bsz.y), IM_COL32(40, 40, 48, hover_alpha_val), 5.f);
    }

    
    if (sel) {
        if (!slide_initialized) {
            active_x = pos.x;
            active_w = bsz.x;
            slide_initialized = true;
        }
        active_x += (pos.x - active_x) * tween_speed;
        active_w += (bsz.x - active_w) * tween_speed;
        
        
        dl->AddRectFilled(ImVec2(active_x, pos.y), ImVec2(active_x + active_w, pos.y + bsz.y), IM_COL32(32, 32, 38, 255), 5.f);
        dl->AddRectFilled(ImVec2(active_x + 8, pos.y + bsz.y - 2), ImVec2(active_x + active_w - 8, pos.y + bsz.y), ImGui::ColorConvertFloat4ToU32(menu::accent_color), 1.f);
        
        
        ImU32 glow_col = IM_COL32(menu::accent_color.x * 255, menu::accent_color.y * 255, menu::accent_color.z * 255, 30);
        dl->AddRect(ImVec2(active_x, pos.y), ImVec2(active_x + active_w, pos.y + bsz.y), glow_col, 5.f, 0, 1.0f);
    }

    ImVec2 tpos = ImVec2(pos.x + (bsz.x - tsz.x) * 0.5f, pos.y + (bsz.y - tsz.y) * 0.5f - 1.0f);
    dl->AddText(ImVec2(tpos.x + 1, tpos.y + 1), IM_COL32(0, 0, 0, (int)(150 * tab_alphas[idx])), label);
    dl->AddText(tpos, ImGui::ColorConvertFloat4ToU32(text_color), label);
    
    ImGui::PopStyleColor(4);
    return clicked;
}

inline bool add_sidebar_tab(const char* label, const char* icon, int idx, bool sel)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    ImVec2 wpos = ImGui::GetWindowPos();
    float sidebar_x = wpos.x + 14.f;
    float tab_y = wpos.y + 90.f + (idx * 44.f);
    ImVec2 tab_size = ImVec2(192.f, 36.f);

    ImRect bb(ImVec2(sidebar_x, tab_y), ImVec2(sidebar_x + tab_size.x, tab_y + tab_size.y));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    static float tab_alphas[10] = { 0.0f };
    static float hover_alphas[10] = { 0.0f };
    static bool initialized = false;
    if (!initialized) {
        for (int i = 0; i < 10; i++) {
            tab_alphas[i] = 0.0f;
            hover_alphas[i] = 0.0f;
        }
        initialized = true;
    }

    float target_alpha = sel ? 1.0f : 0.0f;
    float target_hover = (hovered && !sel) ? 1.0f : 0.0f;
    float tween_speed = g.IO.DeltaTime * 10.0f;

    tab_alphas[idx] += (target_alpha - tab_alphas[idx]) * tween_speed;
    hover_alphas[idx] += (target_hover - hover_alphas[idx]) * tween_speed;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (sel) {
        ImU32 bg_col = IM_COL32(28, 28, 36, 255);
        dl->AddRectFilled(bb.Min, bb.Max, bg_col, 6.f);
        dl->AddRectFilled(ImVec2(bb.Min.x, bb.Min.y + 4.f), ImVec2(bb.Min.x + 3.f, bb.Max.y - 4.f), ImGui::ColorConvertFloat4ToU32(menu::accent_color), 2.f);
        ImU32 glow_col = IM_COL32(menu::accent_color.x * 255, menu::accent_color.y * 255, menu::accent_color.z * 255, (int)(40 * tab_alphas[idx]));
        dl->AddRect(bb.Min, bb.Max, glow_col, 6.f, 0, 1.0f);
    } else if (hover_alphas[idx] > 0.01f) {
        ImU32 hover_col = IM_COL32(35, 35, 45, (int)(120 * hover_alphas[idx]));
        dl->AddRectFilled(bb.Min, bb.Max, hover_col, 6.f);
    }

    ImVec4 text_color = sel ? menu::accent_color : ImVec4(0.70f, 0.70f, 0.75f, 1.0f);
    if (!sel && hovered) {
        text_color = ImVec4(0.90f, 0.90f, 0.95f, 1.0f);
    }

    ImVec2 icon_sz = ImGui::CalcTextSize(icon);
    ImVec2 text_sz = ImGui::CalcTextSize(label);

    float text_offset_y = (tab_size.y - text_sz.y) * 0.5f;
    float icon_offset_y = (tab_size.y - icon_sz.y) * 0.5f;

    ImVec2 icon_pos = ImVec2(bb.Min.x + 14.f, bb.Min.y + icon_offset_y);
    dl->AddText(icon_pos, ImGui::ColorConvertFloat4ToU32(text_color), icon);

    ImVec2 label_pos = ImVec2(bb.Min.x + 36.f, bb.Min.y + text_offset_y);
    dl->AddText(label_pos, ImGui::ColorConvertFloat4ToU32(text_color), label);

    return pressed;
}

static std::vector<const char*> text_list =
{
    "Best Regards Tele-Gram Management Team ",
    "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ ",
};

inline void rendercrazyniggatext(ImDrawList* dl, ImVec2 wpos, ImVec2 wsize, float delta)
{
    const char* text = text_list[text_idx];
    int text_len = static_cast<int>(strlen(text));
    const float hold_time = 2.0f;
    const float char_delay = 0.03f;
    const float fade_time = 0.3f;
    if (!is_fading && visible_chars < text_len) {
        char_timer += delta;
        if (char_timer >= char_delay) {
            visible_chars++;
            char_timer = 0.0f;
        }
    }
    if (visible_chars >= text_len) {
        transition_timer += delta;
        if (transition_timer >= hold_time && !is_fading) {
            is_fading = true;
            transition_timer = 0.0f;
        }
        if (is_fading && transition_timer >= fade_time) {
            text_idx = (text_idx + 1) % text_list.size();
            visible_chars = 0;
            transition_timer = 0.0f;
            char_timer = 0.0f;
            is_fading = false;
        }
    }
    float alpha = 1.0f;
    if (is_fading) {
        alpha = 1.0f - (transition_timer / fade_time);
    }

    char display_buf[256];
    int actual_draw = visible_chars;
    if (visible_chars < text_len && char_timer > 0.0f) {
        actual_draw = visible_chars + 1;
    }
    strncpy_s(display_buf, text, actual_draw);
    display_buf[actual_draw] = '\0';

    ImVec2 text_size = ImGui::CalcTextSize(display_buf);
    float center_x = wpos.x + (wsize.x - text_size.x) * 0.5f;
    float y_pos = wpos.y + 33;

    float x_offset = 0.0f;

    for (int i = 0; i < actual_draw; i++) {
        char single_char[2] = { text[i], '\0' };
        ImVec2 char_size = ImGui::CalcTextSize(single_char);

        float char_alpha = alpha;

        if (i == visible_chars && visible_chars < text_len) {
            float progress = char_timer / char_delay;
            char_alpha = progress * alpha;
        }

        ImVec2 char_pos = ImVec2(center_x + x_offset, y_pos);

        ImU32 outline_col = IM_COL32(0, 0, 0, (int)(255 * char_alpha));
        for (int ox = -1; ox <= 1; ox++) {
            for (int oy = -1; oy <= 1; oy++) {
                if (ox == 0 && oy == 0) continue;
                dl->AddText(ImVec2(char_pos.x + ox, char_pos.y + oy), outline_col, single_char);
            }
        }
        ImVec4 accent_col = menu::accent_color;
        accent_col.w = char_alpha;
        dl->AddText(char_pos, ImGui::ColorConvertFloat4ToU32(accent_col), single_char);

        x_offset += char_size.x;
    }
}