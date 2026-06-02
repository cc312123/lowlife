#include "notifications.h"
#include <imgui/imgui.h>
#include <algorithm>
#include <cmath>

namespace notifications {
    static std::vector<Notification> notification_list;
    static int next_id = 1;
    static const float slide_speed = 2.5f;
    static const float fade_speed = 6.0f;
    static const float notification_spacing = 8.0f;
    static const float notification_width = 300.0f;
    static const float notification_padding = 12.0f;
    
    static float ease_out_cubic(float t) {
        t = 1.0f - t;
        return 1.0f - (t * t * t);
    }

    void add(const std::string& message, NotificationType type, float lifetime) {
        Notification notif(message, type, lifetime);
        notif.id = next_id++;
        notification_list.push_back(notif);
    }

    void update() {
        ImGuiIO& io = ImGui::GetIO();
        float delta_time = io.DeltaTime;

        
        for (auto it = notification_list.begin(); it != notification_list.end();) {
            Notification& notif = *it;
            
            if (notif.slide_progress < 1.0f) {
                float target = 1.0f;
                notif.slide_progress += (target - notif.slide_progress) * delta_time * slide_speed;
                if (notif.slide_progress > 0.99f) {
                    notif.slide_progress = 1.0f;
                }
            }
            
            float time_remaining = notif.lifetime - notif.current_time;
            if (time_remaining < 1.0f) {
                notif.fade_progress = 1.0f - (time_remaining / 1.0f);
                if (notif.fade_progress > 1.0f) {
                    notif.fade_progress = 1.0f;
                }
            } else {
                notif.fade_progress = 0.0f;
            }
            
            notif.current_time += delta_time;
            
            if (notif.current_time >= notif.lifetime) {
                it = notification_list.erase(it);
            } else {
                ++it;
            }
        }
    }

    void render() {
        if (notification_list.empty()) {
            return;
        }

        ImDrawList* draw_list = ImGui::GetForegroundDrawList();
        ImVec2 display_size = ImGui::GetIO().DisplaySize;
        
        float start_x = 10.0f;
        float start_y = 10.0f;
        float current_y = start_y;

        for (auto& notif : notification_list) {
            float slide_distance = notification_width + 80.0f;
            float eased_progress = ease_out_cubic(notif.slide_progress);
            float slide_offset = (1.0f - eased_progress) * slide_distance;
            float x_pos = start_x - slide_offset;
            
            float alpha = 1.0f - notif.fade_progress;
            if (alpha < 0.0f) alpha = 0.0f;
            if (alpha > 1.0f) alpha = 1.0f;
            
            ImVec2 text_size = ImGui::CalcTextSize(notif.message.c_str(), nullptr, false, notification_width - notification_padding * 2);
            float notification_height = text_size.y + notification_padding * 2;
            
            ImVec4 accent = menu::accent_color;
            ImU32 bg_color, border_color, accent_color, text_color;
            
            accent_color = ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, alpha));
            
            bg_color = IM_COL32(35, 35, 35, (int)(alpha * 255));
            border_color = IM_COL32(60, 60, 60, (int)(alpha * 255));
            text_color = IM_COL32(255, 255, 255, (int)(alpha * 255));
            
            ImVec2 pos = ImVec2(x_pos, current_y);
            ImVec2 size = ImVec2(notification_width, notification_height);
            
            draw_list->AddRectFilled(
                ImVec2(pos.x, pos.y),
                ImVec2(pos.x + size.x, pos.y + size.y),
                bg_color,
                0.0f
            );
            
            draw_list->AddRect(
                ImVec2(pos.x + 1, pos.y + 1),
                ImVec2(pos.x + size.x - 1, pos.y + size.y - 1),
                IM_COL32(0, 0, 0, (int)(alpha * 255)),
                0.0f,
                0,
                1.0f
            );
            
            draw_list->AddRect(
                ImVec2(pos.x + 2, pos.y + 2),
                ImVec2(pos.x + size.x - 2, pos.y + size.y - 2),
                border_color,
                0.0f,
                0,
                1.0f
            );
            
            draw_list->AddRectFilled(
                ImVec2(pos.x + 2, pos.y + 2),
                ImVec2(pos.x + size.x - 2, pos.y + 4),
                accent_color
            );
            
            ImVec2 text_pos = ImVec2(pos.x + notification_padding, pos.y + notification_padding);
            
            for (int x = -1; x <= 1; x++) {
                for (int y = -1; y <= 1; y++) {
                    if (x == 0 && y == 0) continue;
                    draw_list->AddText(
                        ImVec2(text_pos.x + x, text_pos.y + y),
                        IM_COL32(0, 0, 0, (int)(alpha * 255)),
                        notif.message.c_str()
                    );
                }
            }
            
            draw_list->AddText(
                text_pos,
                text_color,
                notif.message.c_str()
            );
            
            current_y += notification_height + notification_spacing;
        }
    }
}

