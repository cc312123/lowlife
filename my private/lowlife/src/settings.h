#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>
#include "../ext/imgui/imgui.h"
#include <Windows.h>

namespace menu
{
	inline ImVec4 accent_color = ImVec4(0.f / 255.0f, 150.f / 255.0f, 255.f / 255.0f, 1.0f);
	inline bool watermark{ false };
	inline ImVec2 watermark_pos = ImVec2(-1, 10);
	inline bool streamproof{ false };
	inline bool hide_console{ true };
	inline int menu_keybind{ VK_INSERT };
	inline bool authenticated{ true };
	inline char password_input[64]{ "" };
	inline std::string correct_key{ "lowlife_private" };
	inline bool update_log{ false };
	inline float update_log_height{ 80.f };
}

namespace settings
{
	namespace aimbot
	{
		inline bool enabled{ false };
		inline int aimbot_type{ 0 };
		inline bool sticky_aim{ false };
		inline bool draw_fov{ false };
		inline bool filled_fov{ false };
		inline bool rotate_fov{ false };
		inline bool rainbow_fov{ false };
		inline float fov{ 200.0f };
		inline float fov_color[4]{ 1.f, 1.f, 1.f, 0.5f };
		inline bool fov_check{ true };
		inline bool knocked_check{ false };
		inline bool wall_check{ false };
		inline int keybind{ 0 };
		inline int keybind_mode{ 0 };
		inline int aimpart{ 0 };
		inline bool team_check{ false };
		
		inline bool mouse_smooth{ false };
		inline float mouse_smooth_x{ 1.0f };
		inline float mouse_smooth_y{ 1.0f };
		inline float mouse_sensitivity{ 1.0f };
		inline bool mouse_prediction{ false };
		inline float mouse_prediction_x{ 1.0f };
		inline float mouse_prediction_y{ 1.0f };
		
		inline bool camera_smooth{ false };
		inline float camera_smooth_x{ 1.0f };
		inline float camera_smooth_y{ 1.0f };
		inline bool camera_prediction{ false };
		inline float camera_prediction_x{ 1.0f };
		inline float camera_prediction_y{ 1.0f };
		
		inline bool shake{ false };
		inline float shake_x{ 0.0f };
		inline float shake_y{ 0.0f };
		inline int easing_style{ 0 }; 

		inline int target_selection_mode{ 0 }; // 0 = Crosshair, 1 = 3D Distance, 2 = Health
		inline bool smart_bone{ false };
		inline bool bone_random_offset{ false };
		inline float bone_random_offset_val{ 0.2f };
		inline float latency_ms{ 50.0f };
		inline bool projectile_prediction{ false };
		inline float projectile_speed{ 1000.0f };
		inline float projectile_gravity{ 196.2f };
		inline bool adaptive_smoothing{ false };
		inline float adaptive_smooth_min{ 2.0f };
		inline float adaptive_smooth_max{ 10.0f };
	}
	namespace silent
	{
		inline bool enabled{ false };
		inline bool sticky_aim{ false };
		inline bool spoof_mouse{ true };
		inline bool draw_fov{ false };
		inline bool filled_fov{ false };
		inline bool rotate_fov{ false };
		inline bool rainbow_fov{ false };
		inline float fov{ 200.0f };
		inline int keybind{ 0 };
		inline int keybind_mode{ 0 }; 
		inline int aim_part{ 0 };
		inline bool fov_check{ true };
		inline bool knocked_check{ false };
		inline bool wall_check{ false };
		inline bool magic_bullet{ false };
		inline bool gun_based_fov{ false };
		inline float fov_double_barrel{ 200.0f };
		inline float fov_tactical_shotgun{ 200.0f };
		inline float fov_revolver{ 200.0f };
		inline float fov_color[4]{ 1.f, 1.f, 1.f, 0.5f };
	}
	namespace visuals
	{
		inline bool box{ false };
		inline int box_type{ 0 }; 
		inline float box_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool name{ false };
		inline float name_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool distance{ false };
		inline float distance_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool tool{ false };
		inline float tool_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool weapon_icon{ false };
		inline float weapon_icon_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool localplayer{ false };

		inline bool highlights{ false };
		inline float highlights_color[4]{ 1.f, 1.f, 1.f, 0.4f };

		inline bool tracers{ false };
		inline float tracers_color[4]{ 1.f, 1.f, 1.f, 1.f };
		inline int tracers_origin{ 0 }; 

		inline bool skeleton{ false };
		inline float skeleton_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool head_dot{ false };
		inline float head_dot_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool look_vector{ false };
		inline float look_vector_color[4]{ 1.f, 0.f, 0.f, 1.f };

		inline bool healthbar{ false };
		inline float healthbar_color[3]{ 0.f, 1.f, 0.f };
		inline bool health_text{ false };
		inline float health_text_color[3]{ 1.f, 1.f, 1.f };
		inline bool feature_indicator{ false };
		inline float feature_indicator_x{ 50.0f };
		inline float feature_indicator_y{ 0.0f };
		inline bool target{ false };
	}

	namespace expl
	{
		inline bool walkspeed{ false };
		inline float walkspeed_speed{ 16.0f };
		inline int walkspeed_mode{ 0 };
		inline float walkspeed_health_threshold{ 50.0f };
		inline int walkspeed_keybind{ 0 };
		
		inline bool freeze_players{ false };
		inline int freeze_players_keybind{ 0 };
		inline int freeze_players_keybind_mode{ 0 };

		inline bool tickrate{ false };
		inline float tickrate_amount{ 0 };
		
		inline bool fly_enabled{ false };
		inline float fly_speed{ 50.0f };
		inline int fly_mode{ 0 }; 
		inline int fly_keybind{ 0 };
		inline int fly_keybind_mode{ 0 };

		inline bool jumppower_enabled{ false };
		inline float jumppower_power{ 50.0f };

		inline bool infinite_jump{ false };

		inline bool noclip_enabled{ false };

		inline bool gravity_enabled{ false };
		inline float gravity_value{ 196.2f };

		inline bool fov_changer_enabled{ false };
		inline float fov_changer_value{ 70.0f };

		inline bool infinite_ammo{ false };
	}

	namespace hitbox_expander
	{
		inline bool enabled{ false };
		inline int target_part{ 0 };
		inline float size_x{ 2.2f };
		inline float size_y{ 2.2f };
		inline float size_z{ 1.2f };
		inline bool visualize{ false };
	}

	namespace dex_explorer
	{
		inline bool enabled{ false };
	}

	namespace cleaner
	{
		inline bool enabled{ true };
		inline bool clean_registry{ true };
		inline bool clean_temp{ true };
		inline bool clean_prefetch{ true };
		inline bool clean_eventlogs{ true };
		inline bool show_details{ true };
	}

	namespace shot_detection
	{
		inline bool enabled{ false };
		inline int trigger_key{ 'C' };
		inline int select_key{ VK_XBUTTON2 }; 
		inline int min_delay{ 15 }; 
		inline int max_delay{ 30 }; 
		inline float select_hitbox{ 150.0f }; 
		inline std::string ammo_name{ "Ammo" }; 
		inline std::string target_name{ "None" };
		inline std::uint64_t target_address{ 0 };
		
		inline bool db_revolver_combo{ false };
		inline int db_slot_key{ '1' };
		inline int revolver_slot_key{ '2' };
		inline int combo_cps{ 45 };
		inline bool auto_switch_on_start{ true };
		inline bool knocked_check{ false };
		inline int switch_delay{ 15 };
	}

	namespace botter
	{
		inline bool autoclicker_enabled{ false };
		inline int trigger_keybind{ 'V' };
		inline int trigger_keybind_mode{ 0 }; 
		inline float hitbox_size{ 100.0f };
		inline bool wall_check{ false };
		inline bool visualize_hitbox{ false };
		inline int cps{ 45 };
		inline bool knocked_check{ false };
		inline bool team_check{ false };
	}

	namespace player_relations
	{
		
		inline std::unordered_map<std::string, int> relations;
	}
}