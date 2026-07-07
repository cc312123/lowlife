#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include "../ext/imgui/imgui.h"
#include <Windows.h>

namespace globals
{
	extern std::atomic<bool> roblox_valid;
}

namespace menu
{
	inline ImVec4 accent_color = ImVec4(217.f / 255.0f, 119.f / 255.0f, 6.f / 255.0f, 1.0f);
	inline bool sahur_theme_active{ false };
	inline bool watermark{ false };
	inline ImVec2 watermark_pos = ImVec2(-1, 10);
	inline bool streamproof{ true };
	inline bool hide_console{ true };
	inline int menu_keybind{ VK_INSERT };
	inline bool authenticated{ true };
	inline char password_input[64]{ "" };
	inline std::string correct_key{ "tungware_private" };
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
		
		inline bool mouse_smooth{ true };
		inline float mouse_smooth_x{ 15.0f };
		inline float mouse_smooth_y{ 15.0f };
		inline float mouse_sensitivity{ 1.0f };
		inline bool mouse_prediction{ false };
		inline float mouse_prediction_x{ 1.0f };
		inline float mouse_prediction_y{ 1.0f };
		
		inline bool camera_smooth{ true };
		inline float camera_smooth_x{ 15.0f };
		inline float camera_smooth_y{ 15.0f };
		inline bool camera_prediction{ false };
		inline float camera_prediction_x{ 1.0f };
		inline float camera_prediction_y{ 1.0f };
		
		inline bool shake{ false };
		inline float shake_x{ 0.0f };
		inline float shake_y{ 0.0f };
		inline int easing_style{ 0 }; 
		inline float ease_time{ 0.25f };

		inline int target_selection_mode{ 0 }; 
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
		inline bool spring_damping{ true };
	}
	namespace new_silent
	{
		inline bool enabled{ false };
		inline int silent_mode{ 0 }; // 0 = Mouse Snap, 1 = Memory Write
		inline int target_mode{ 0 }; 
		inline int hit_chance{ 100 }; 
		inline bool sticky_aim{ false };
		inline bool prediction_enabled{ true };
		inline float prediction_scale_x{ 1.0f };
		inline float prediction_scale_y{ 1.0f };
		inline bool auto_prediction{ true };
		inline int keybind{ 0 };
		inline int keybind_mode{ 0 }; 
		inline int aim_part{ 0 }; 
		inline bool fov_check{ true };
		inline bool knocked_check{ true };
		inline bool team_check{ false };
		inline bool wall_check{ false };
		inline bool draw_fov{ false };
		inline bool filled_fov{ false };
		inline bool rotate_fov{ false };
		inline bool rainbow_fov{ false };
		inline float fov{ 200.0f };
		inline float fov_color[4]{ 1.f, 0.2f, 0.2f, 0.4f };
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
	}

	namespace expl
	{
		inline bool walkspeed{ false };
		inline float walkspeed_speed{ 16.0f };
		inline int walkspeed_mode{ 0 };
		inline float walkspeed_health_threshold{ 50.0f };
		inline int walkspeed_keybind{ 0 };
		inline int walkspeed_keybind_mode{ 0 };
		inline int walkspeed_method{ 0 };
		
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
		inline int jumppower_keybind{ 0 };
		inline int jumppower_keybind_mode{ 0 };
		inline int jumppower_method{ 0 };

		inline bool infinite_jump{ false };

		inline bool noclip_enabled{ false };

		inline bool gravity_enabled{ false };
		inline float gravity_value{ 196.2f };

		inline bool fov_changer_enabled{ false };
		inline float fov_changer_value{ 70.0f };

		inline bool infinite_ammo{ false };

		inline bool legit_teleport{ false };
		inline float legit_teleport_speed{ 150.0f };
		inline int legit_teleport_delay{ 15 };

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



	namespace botter
	{
		inline bool autoclicker_enabled{ false };
		inline int trigger_keybind{ 'V' };
		inline int trigger_keybind_mode{ 0 }; 
		inline float hitbox_size{ 100.0f };
		inline bool wall_check{ false };
		inline bool visualize_hitbox{ false };
		inline bool raycast_hitbox{ false };
		inline bool db_spread_raycast{ false };
		inline float db_spread_angle{ 0.12f };
		inline int db_min_pellets{ 1 };
		inline int cps{ 45 };
		inline bool knocked_check{ false };
		inline bool team_check{ false };
	}

	namespace shot_detect
	{
		inline bool enabled{ false };
		inline int click_mode{ 0 }; 
		inline int cps{ 50 };
		inline int trigger_keybind{ 'C' };
		inline int trigger_keybind_mode{ 0 }; 
		inline int min_delay{ 50 };
		inline int max_delay{ 150 };
		inline bool randomize_delay{ false };
		inline int click_delay{ 100 };
		inline bool gunswap_enabled{ false };
		inline int db_slot{ 1 };
		inline int revolver_slot{ 2 };
		inline int gunswap_delay{ 50 };
		inline bool always_start_with_db{ false };
	}

	namespace player_relations
	{
		inline std::mutex relations_mutex;
		inline std::unordered_map<std::string, int> relations;
	}
}