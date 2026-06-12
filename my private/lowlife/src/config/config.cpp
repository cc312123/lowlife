#include "config.h"
#include "../settings.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <Windows.h>

namespace config
{
	std::string get_config_folder()
	{
		return "";
	}

	bool create_config_folder()
	{
		return true;
	}

	std::vector<config_file_t> get_config_files()
	{
		std::vector<config_file_t> configs;
		HKEY hKey;
		if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Accessibility\\Configs", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
		{
			DWORD index = 0;
			char valueName[16384];
			DWORD valueNameSize = sizeof(valueName);
			while (RegEnumValueA(hKey, index, valueName, &valueNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
			{
				config_file_t config;
				config.name = std::string(valueName);
				config.path = "";
				configs.push_back(config);

				index++;
				valueNameSize = sizeof(valueName);
			}
			RegCloseKey(hKey);
		}
		return configs;
	}

	std::string escape_json_string(const std::string& str)
	{
		std::ostringstream o;
		for (size_t i = 0; i < str.length(); ++i)
		{
			switch (str[i])
			{
			case '"': o << "\\\""; break;
			case '\\': o << "\\\\"; break;
			case '\b': o << "\\b"; break;
			case '\f': o << "\\f"; break;
			case '\n': o << "\\n"; break;
			case '\r': o << "\\r"; break;
			case '\t': o << "\\t"; break;
			default:
				if ('\x00' <= str[i] && str[i] <= '\x1f')
				{
					o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)str[i];
				}
				else
				{
					o << str[i];
				}
			}
		}
		return o.str();
	}

	bool save_config(const std::string& name)
	{
		std::stringstream file;

		file << "{\n";

		
		file << "  \"menu\": {\n";
		file << "    \"accent_color\": [" << menu::accent_color.x << "," << menu::accent_color.y << "," << menu::accent_color.z << "," << menu::accent_color.w << "],\n";
		file << "    \"watermark\": " << (menu::watermark ? "true" : "false") << ",\n";
		file << "    \"watermark_pos\": [" << menu::watermark_pos.x << "," << menu::watermark_pos.y << "],\n";
		file << "    \"streamproof\": " << (menu::streamproof ? "true" : "false") << ",\n";
		file << "    \"hide_console\": " << (menu::hide_console ? "true" : "false") << ",\n";
		file << "    \"menu_keybind\": " << menu::menu_keybind << ",\n";
		file << "    \"update_log\": " << (menu::update_log ? "true" : "false") << "\n";
		file << "  },\n";

		
		file << "  \"aimbot\": {\n";
		file << "    \"enabled\": " << (settings::aimbot::enabled ? "true" : "false") << ",\n";
		file << "    \"aimbot_type\": " << settings::aimbot::aimbot_type << ",\n";
		file << "    \"sticky_aim\": " << (settings::aimbot::sticky_aim ? "true" : "false") << ",\n";
		file << "    \"draw_fov\": " << (settings::aimbot::draw_fov ? "true" : "false") << ",\n";
		file << "    \"filled_fov\": " << (settings::aimbot::filled_fov ? "true" : "false") << ",\n";
		file << "    \"rotate_fov\": " << (settings::aimbot::rotate_fov ? "true" : "false") << ",\n";
		file << "    \"rainbow_fov\": " << (settings::aimbot::rainbow_fov ? "true" : "false") << ",\n";
		file << "    \"fov\": " << settings::aimbot::fov << ",\n";
		file << "    \"fov_color\": [" << settings::aimbot::fov_color[0] << "," << settings::aimbot::fov_color[1] << "," << settings::aimbot::fov_color[2] << "," << settings::aimbot::fov_color[3] << "],\n";
		file << "    \"fov_check\": " << (settings::aimbot::fov_check ? "true" : "false") << ",\n";
		file << "    \"knocked_check\": " << (settings::aimbot::knocked_check ? "true" : "false") << ",\n";
		file << "    \"wall_check\": " << (settings::aimbot::wall_check ? "true" : "false") << ",\n";
		file << "    \"keybind\": " << settings::aimbot::keybind << ",\n";
		file << "    \"keybind_mode\": " << settings::aimbot::keybind_mode << ",\n";
		file << "    \"aimpart\": " << settings::aimbot::aimpart << ",\n";
		file << "    \"mouse_smooth\": " << (settings::aimbot::mouse_smooth ? "true" : "false") << ",\n";
		file << "    \"mouse_smooth_x\": " << settings::aimbot::mouse_smooth_x << ",\n";
		file << "    \"mouse_smooth_y\": " << settings::aimbot::mouse_smooth_y << ",\n";
		file << "    \"mouse_sensitivity\": " << settings::aimbot::mouse_sensitivity << ",\n";
		file << "    \"mouse_prediction\": " << (settings::aimbot::mouse_prediction ? "true" : "false") << ",\n";
		file << "    \"mouse_prediction_x\": " << settings::aimbot::mouse_prediction_x << ",\n";
		file << "    \"mouse_prediction_y\": " << settings::aimbot::mouse_prediction_y << ",\n";
		file << "    \"camera_smooth\": " << (settings::aimbot::camera_smooth ? "true" : "false") << ",\n";
		file << "    \"camera_smooth_x\": " << settings::aimbot::camera_smooth_x << ",\n";
		file << "    \"camera_smooth_y\": " << settings::aimbot::camera_smooth_y << ",\n";
		file << "    \"camera_prediction\": " << (settings::aimbot::camera_prediction ? "true" : "false") << ",\n";
		file << "    \"camera_prediction_x\": " << settings::aimbot::camera_prediction_x << ",\n";
		file << "    \"camera_prediction_y\": " << settings::aimbot::camera_prediction_y << ",\n";
		file << "    \"shake\": " << (settings::aimbot::shake ? "true" : "false") << ",\n";
		file << "    \"shake_x\": " << settings::aimbot::shake_x << ",\n";
		file << "    \"shake_y\": " << settings::aimbot::shake_y << ",\n";
		file << "    \"easing_style\": " << settings::aimbot::easing_style << ",\n";
		file << "    \"ease_time\": " << settings::aimbot::ease_time << ",\n";
		file << "    \"team_check\": " << (settings::aimbot::team_check ? "true" : "false") << ",\n";
		file << "    \"target_selection_mode\": " << settings::aimbot::target_selection_mode << ",\n";
		file << "    \"smart_bone\": " << (settings::aimbot::smart_bone ? "true" : "false") << ",\n";
		file << "    \"bone_random_offset\": " << (settings::aimbot::bone_random_offset ? "true" : "false") << ",\n";
		file << "    \"bone_random_offset_val\": " << settings::aimbot::bone_random_offset_val << ",\n";
		file << "    \"latency_ms\": " << settings::aimbot::latency_ms << ",\n";
		file << "    \"projectile_prediction\": " << (settings::aimbot::projectile_prediction ? "true" : "false") << ",\n";
		file << "    \"projectile_speed\": " << settings::aimbot::projectile_speed << ",\n";
		file << "    \"projectile_gravity\": " << settings::aimbot::projectile_gravity << ",\n";
		file << "    \"adaptive_smoothing\": " << (settings::aimbot::adaptive_smoothing ? "true" : "false") << ",\n";
		file << "    \"adaptive_smooth_min\": " << settings::aimbot::adaptive_smooth_min << ",\n";
		file << "    \"adaptive_smooth_max\": " << settings::aimbot::adaptive_smooth_max << "\n";
		file << "  },\n";

		
		file << "  \"silent\": {\n";
		file << "    \"enabled\": " << (settings::silent::enabled ? "true" : "false") << ",\n";
		file << "    \"sticky_aim\": " << (settings::silent::sticky_aim ? "true" : "false") << ",\n";
		file << "    \"spoof_mouse\": " << (settings::silent::spoof_mouse ? "true" : "false") << ",\n";
		file << "    \"draw_fov\": " << (settings::silent::draw_fov ? "true" : "false") << ",\n";
		file << "    \"filled_fov\": " << (settings::silent::filled_fov ? "true" : "false") << ",\n";
		file << "    \"rotate_fov\": " << (settings::silent::rotate_fov ? "true" : "false") << ",\n";
		file << "    \"rainbow_fov\": " << (settings::silent::rainbow_fov ? "true" : "false") << ",\n";
		file << "    \"fov\": " << settings::silent::fov << ",\n";
		file << "    \"keybind\": " << settings::silent::keybind << ",\n";
		file << "    \"keybind_mode\": " << settings::silent::keybind_mode << ",\n";
		file << "    \"aim_part\": " << settings::silent::aim_part << ",\n";
		file << "    \"fov_check\": " << (settings::silent::fov_check ? "true" : "false") << ",\n";
		file << "    \"knocked_check\": " << (settings::silent::knocked_check ? "true" : "false") << ",\n";
		file << "    \"wall_check\": " << (settings::silent::wall_check ? "true" : "false") << ",\n";
		file << "    \"magic_bullet\": " << (settings::silent::magic_bullet ? "true" : "false") << ",\n";
		file << "    \"gun_based_fov\": " << (settings::silent::gun_based_fov ? "true" : "false") << ",\n";
		file << "    \"fov_double_barrel\": " << settings::silent::fov_double_barrel << ",\n";
		file << "    \"fov_tactical_shotgun\": " << settings::silent::fov_tactical_shotgun << ",\n";
		file << "    \"fov_revolver\": " << settings::silent::fov_revolver << ",\n";
		file << "    \"fov_color\": [" << settings::silent::fov_color[0] << "," << settings::silent::fov_color[1] << "," << settings::silent::fov_color[2] << "," << settings::silent::fov_color[3] << "]\n";
		file << "  },\n";

		
		file << "  \"visuals\": {\n";
		file << "    \"box\": " << (settings::visuals::box ? "true" : "false") << ",\n";
		file << "    \"box_type\": " << settings::visuals::box_type << ",\n";
		file << "    \"box_color\": [" << settings::visuals::box_color[0] << "," << settings::visuals::box_color[1] << "," << settings::visuals::box_color[2] << "," << settings::visuals::box_color[3] << "],\n";
		file << "    \"name\": " << (settings::visuals::name ? "true" : "false") << ",\n";
		file << "    \"name_color\": [" << settings::visuals::name_color[0] << "," << settings::visuals::name_color[1] << "," << settings::visuals::name_color[2] << "," << settings::visuals::name_color[3] << "],\n";
		file << "    \"distance\": " << (settings::visuals::distance ? "true" : "false") << ",\n";
		file << "    \"distance_color\": [" << settings::visuals::distance_color[0] << "," << settings::visuals::distance_color[1] << "," << settings::visuals::distance_color[2] << "," << settings::visuals::distance_color[3] << "],\n";
		file << "    \"tool\": " << (settings::visuals::tool ? "true" : "false") << ",\n";
		file << "    \"tool_color\": [" << settings::visuals::tool_color[0] << "," << settings::visuals::tool_color[1] << "," << settings::visuals::tool_color[2] << "," << settings::visuals::tool_color[3] << "],\n";
		file << "    \"weapon_icon\": " << (settings::visuals::weapon_icon ? "true" : "false") << ",\n";
		file << "    \"weapon_icon_color\": [" << settings::visuals::weapon_icon_color[0] << "," << settings::visuals::weapon_icon_color[1] << "," << settings::visuals::weapon_icon_color[2] << "," << settings::visuals::weapon_icon_color[3] << "],\n";
		file << "    \"localplayer\": " << (settings::visuals::localplayer ? "true" : "false") << ",\n";
		file << "    \"highlights\": " << (settings::visuals::highlights ? "true" : "false") << ",\n";
		file << "    \"highlights_color\": [" << settings::visuals::highlights_color[0] << "," << settings::visuals::highlights_color[1] << "," << settings::visuals::highlights_color[2] << "," << settings::visuals::highlights_color[3] << "],\n";
		file << "    \"tracers\": " << (settings::visuals::tracers ? "true" : "false") << ",\n";
		file << "    \"tracers_color\": [" << settings::visuals::tracers_color[0] << "," << settings::visuals::tracers_color[1] << "," << settings::visuals::tracers_color[2] << "," << settings::visuals::tracers_color[3] << "],\n";
		file << "    \"tracers_origin\": " << settings::visuals::tracers_origin << ",\n";
		file << "    \"skeleton\": " << (settings::visuals::skeleton ? "true" : "false") << ",\n";
		file << "    \"skeleton_color\": [" << settings::visuals::skeleton_color[0] << "," << settings::visuals::skeleton_color[1] << "," << settings::visuals::skeleton_color[2] << "," << settings::visuals::skeleton_color[3] << "],\n";
		file << "    \"head_dot\": " << (settings::visuals::head_dot ? "true" : "false") << ",\n";
		file << "    \"head_dot_color\": [" << settings::visuals::head_dot_color[0] << "," << settings::visuals::head_dot_color[1] << "," << settings::visuals::head_dot_color[2] << "," << settings::visuals::head_dot_color[3] << "],\n";
		file << "    \"look_vector\": " << (settings::visuals::look_vector ? "true" : "false") << ",\n";
		file << "    \"look_vector_color\": [" << settings::visuals::look_vector_color[0] << "," << settings::visuals::look_vector_color[1] << "," << settings::visuals::look_vector_color[2] << "," << settings::visuals::look_vector_color[3] << "],\n";
		file << "    \"healthbar\": " << (settings::visuals::healthbar ? "true" : "false") << ",\n";
		file << "    \"healthbar_color\": [" << settings::visuals::healthbar_color[0] << "," << settings::visuals::healthbar_color[1] << "," << settings::visuals::healthbar_color[2] << "],\n";
		file << "    \"health_text\": " << (settings::visuals::health_text ? "true" : "false") << ",\n";
		file << "    \"health_text_color\": [" << settings::visuals::health_text_color[0] << "," << settings::visuals::health_text_color[1] << "," << settings::visuals::health_text_color[2] << "],\n";
		file << "    \"feature_indicator\": " << (settings::visuals::feature_indicator ? "true" : "false") << ",\n";
		file << "    \"feature_indicator_x\": " << settings::visuals::feature_indicator_x << ",\n";
		file << "    \"feature_indicator_y\": " << settings::visuals::feature_indicator_y << ",\n";
		file << "    \"target\": " << (settings::visuals::target ? "true" : "false") << "\n";
		file << "  },\n";

		
		file << "  \"expl\": {\n";
		file << "    \"walkspeed\": " << (settings::expl::walkspeed ? "true" : "false") << ",\n";
		file << "    \"walkspeed_speed\": " << settings::expl::walkspeed_speed << ",\n";
		file << "    \"walkspeed_mode\": " << settings::expl::walkspeed_mode << ",\n";
		file << "    \"walkspeed_health_threshold\": " << settings::expl::walkspeed_health_threshold << ",\n";
		file << "    \"walkspeed_keybind\": " << settings::expl::walkspeed_keybind << ",\n";
		file << "    \"freeze_players\": " << (settings::expl::freeze_players ? "true" : "false") << ",\n";
		file << "    \"freeze_players_keybind\": " << settings::expl::freeze_players_keybind << ",\n";
		file << "    \"freeze_players_keybind_mode\": " << settings::expl::freeze_players_keybind_mode << ",\n";
		file << "    \"tickrate\": " << (settings::expl::tickrate ? "true" : "false") << ",\n";
		file << "    \"tickrate_amount\": " << settings::expl::tickrate_amount << ",\n";
		file << "    \"fly_enabled\": " << (settings::expl::fly_enabled ? "true" : "false") << ",\n";
		file << "    \"fly_speed\": " << settings::expl::fly_speed << ",\n";
		file << "    \"fly_mode\": " << settings::expl::fly_mode << ",\n";
		file << "    \"fly_keybind\": " << settings::expl::fly_keybind << ",\n";
		file << "    \"fly_keybind_mode\": " << settings::expl::fly_keybind_mode << ",\n";
		file << "    \"jumppower_enabled\": " << (settings::expl::jumppower_enabled ? "true" : "false") << ",\n";
		file << "    \"jumppower_power\": " << settings::expl::jumppower_power << ",\n";
		file << "    \"infinite_jump\": " << (settings::expl::infinite_jump ? "true" : "false") << ",\n";
		file << "    \"noclip_enabled\": " << (settings::expl::noclip_enabled ? "true" : "false") << ",\n";
		file << "    \"gravity_enabled\": " << (settings::expl::gravity_enabled ? "true" : "false") << ",\n";
		file << "    \"gravity_value\": " << settings::expl::gravity_value << ",\n";
		file << "    \"fov_changer_enabled\": " << (settings::expl::fov_changer_enabled ? "true" : "false") << ",\n";
		file << "    \"fov_changer_value\": " << settings::expl::fov_changer_value << ",\n";
		file << "    \"infinite_ammo\": " << (settings::expl::infinite_ammo ? "true" : "false") << ",\n";
		file << "    \"mechanical_nospread\": " << (settings::expl::mechanical_nospread ? "true" : "false") << ",\n";
		file << "    \"crouch_on_fire\": " << (settings::expl::crouch_on_fire ? "true" : "false") << ",\n";
		file << "    \"stop_on_shot\": " << (settings::expl::stop_on_shot ? "true" : "false") << ",\n";
		file << "    \"crouch_key\": " << settings::expl::crouch_key << "\n";
		file << "  },\n";

		
		file << "  \"dex_explorer\": {\n";
		file << "    \"enabled\": " << (settings::dex_explorer::enabled ? "true" : "false") << "\n";
		file << "  },\n";

		file << "  \"cleaner\": {\n";
		file << "    \"enabled\": " << (settings::cleaner::enabled ? "true" : "false") << "\n";
		file << "  },\n";



		
		file << "  \"botter\": {\n";
		file << "    \"autoclicker_enabled\": " << (settings::botter::autoclicker_enabled ? "true" : "false") << ",\n";
		file << "    \"trigger_keybind\": " << settings::botter::trigger_keybind << ",\n";
		file << "    \"trigger_keybind_mode\": " << settings::botter::trigger_keybind_mode << ",\n";
		file << "    \"hitbox_size\": " << settings::botter::hitbox_size << ",\n";
		file << "    \"visualize_hitbox\": " << (settings::botter::visualize_hitbox ? "true" : "false") << ",\n";
		file << "    \"cps\": " << settings::botter::cps << ",\n";
		file << "    \"wall_check\": " << (settings::botter::wall_check ? "true" : "false") << ",\n";
		file << "    \"knocked_check\": " << (settings::botter::knocked_check ? "true" : "false") << ",\n";
		file << "    \"team_check\": " << (settings::botter::team_check ? "true" : "false") << "\n";
		file << "  },\n";

		file << "  \"shot_detect\": {\n";
		file << "    \"enabled\": " << (settings::shot_detect::enabled ? "true" : "false") << ",\n";
		file << "    \"click_mode\": " << settings::shot_detect::click_mode << ",\n";
		file << "    \"cps\": " << settings::shot_detect::cps << ",\n";
		file << "    \"trigger_keybind\": " << settings::shot_detect::trigger_keybind << ",\n";
		file << "    \"trigger_keybind_mode\": " << settings::shot_detect::trigger_keybind_mode << ",\n";
		file << "    \"min_delay\": " << settings::shot_detect::min_delay << ",\n";
		file << "    \"max_delay\": " << settings::shot_detect::max_delay << ",\n";
		file << "    \"randomize_delay\": " << (settings::shot_detect::randomize_delay ? "true" : "false") << ",\n";
		file << "    \"gunswap_enabled\": " << (settings::shot_detect::gunswap_enabled ? "true" : "false") << ",\n";
		file << "    \"db_slot\": " << settings::shot_detect::db_slot << ",\n";
		file << "    \"revolver_slot\": " << settings::shot_detect::revolver_slot << ",\n";
		file << "    \"gunswap_delay\": " << settings::shot_detect::gunswap_delay << ",\n";
		file << "    \"always_start_with_db\": " << (settings::shot_detect::always_start_with_db ? "true" : "false") << "\n";
		file << "  }\n";

		file << "}\n";

		std::string json_str = file.str();
		HKEY hKey;
		if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Accessibility\\Configs", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
		{
			LONG result = RegSetValueExA(hKey, name.c_str(), 0, REG_SZ, (const BYTE*)json_str.c_str(), (DWORD)(json_str.length() + 1));
			RegCloseKey(hKey);
			return (result == ERROR_SUCCESS);
		}
		return false;
	}

	std::string extract_json_section(const std::string& json, const std::string& section_name)
	{
		std::string search_key = "\"" + section_name + "\"";
		size_t pos = json.find(search_key);
		if (pos == std::string::npos)
			return "";

		pos = json.find("{", pos);
		if (pos == std::string::npos)
			return "";

		int brace_count = 1;
		size_t start = pos;
		pos++;
		while (pos < json.length() && brace_count > 0)
		{
			if (json[pos] == '{') brace_count++;
			else if (json[pos] == '}') brace_count--;
			pos++;
		}

		return json.substr(start, pos - start);
	}

	bool parse_json_value(const std::string& json, const std::string& key, std::string& value)
	{
		std::string search_key = "\"" + key + "\"";
		size_t pos = json.find(search_key);
		if (pos == std::string::npos)
			return false;

		pos = json.find(":", pos);
		if (pos == std::string::npos)
			return false;

		pos++;
		while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r'))
			pos++;

		size_t end_pos = pos;
		if (json[pos] == '"')
		{
			pos++;
			end_pos = json.find('"', pos);
			if (end_pos == std::string::npos) return false;
		}
		else if (json[pos] == '[')
		{
			int bracket_count = 1;
			end_pos = pos + 1;
			while (end_pos < json.length() && bracket_count > 0)
			{
				if (json[end_pos] == '[') bracket_count++;
				else if (json[end_pos] == ']') bracket_count--;
				end_pos++;
			}
		}
		else
		{
			while (end_pos < json.length() && json[end_pos] != ',' && json[end_pos] != '}' && json[end_pos] != '\n' && json[end_pos] != '\r')
				end_pos++;
		}

		value = json.substr(pos, end_pos - pos);
		if (value.length() > 0 && value.front() == '"' && value.back() == '"')
			value = value.substr(1, value.length() - 2);

		return true;
	}

	bool parse_json_array(const std::string& array_str, float* values, size_t count)
	{
		std::string str = array_str;
		if (str.front() == '[') str = str.substr(1);
		if (str.back() == ']') str = str.substr(0, str.length() - 1);

		std::istringstream iss(str);
		std::string token;
		size_t idx = 0;

		while (std::getline(iss, token, ',') && idx < count)
		{
			token.erase(0, token.find_first_not_of(" \t"));
			token.erase(token.find_last_not_of(" \t") + 1);
			try {
				values[idx++] = std::stof(token);
			}
			catch (...) {
				return false;
			}
		}

		return idx == count;
	}

	bool load_config(const std::string& name)
	{
		HKEY hKey;
		std::string json_content = "";
		if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Accessibility\\Configs", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
		{
			DWORD size = 0;
			if (RegQueryValueExA(hKey, name.c_str(), NULL, NULL, NULL, &size) == ERROR_SUCCESS)
			{
				std::vector<char> buffer(size);
				if (RegQueryValueExA(hKey, name.c_str(), NULL, NULL, (LPBYTE)buffer.data(), &size) == ERROR_SUCCESS)
				{
					json_content = std::string(buffer.data());
				}
			}
			RegCloseKey(hKey);
		}

		if (json_content.empty())
			return false;

		std::string value;
		std::string section;

		
		section = extract_json_section(json_content, "menu");
		if (!section.empty())
		{
			if (parse_json_value(section, "accent_color", value))
			{
				float colors[4];
				if (parse_json_array(value, colors, 4))
				{
					menu::accent_color.x = colors[0];
					menu::accent_color.y = colors[1];
					menu::accent_color.z = colors[2];
					menu::accent_color.w = colors[3];
				}
			}
			if (parse_json_value(section, "watermark", value))
				menu::watermark = (value == "true");
			if (parse_json_value(section, "watermark_pos", value))
			{
				float pos[2];
				if (parse_json_array(value, pos, 2))
				{
					menu::watermark_pos.x = pos[0];
					menu::watermark_pos.y = pos[1];
				}
			}
			if (parse_json_value(section, "streamproof", value))
				menu::streamproof = (value == "true");
			if (parse_json_value(section, "hide_console", value))
				menu::hide_console = (value == "true");
			if (parse_json_value(section, "menu_keybind", value))
				menu::menu_keybind = std::stoi(value);
			if (parse_json_value(section, "update_log", value))
				menu::update_log = (value == "true");
		}

		
		section = extract_json_section(json_content, "aimbot");
		if (!section.empty())
		{
			if (parse_json_value(section, "enabled", value))
				settings::aimbot::enabled = (value == "true");
			if (parse_json_value(section, "aimbot_type", value))
				settings::aimbot::aimbot_type = std::stoi(value);
			if (parse_json_value(section, "sticky_aim", value))
				settings::aimbot::sticky_aim = (value == "true");
			if (parse_json_value(section, "draw_fov", value))
				settings::aimbot::draw_fov = (value == "true");
			if (parse_json_value(section, "filled_fov", value))
				settings::aimbot::filled_fov = (value == "true");
			if (parse_json_value(section, "rotate_fov", value))
				settings::aimbot::rotate_fov = (value == "true");
			if (parse_json_value(section, "rainbow_fov", value))
				settings::aimbot::rainbow_fov = (value == "true");
			if (parse_json_value(section, "fov", value))
				settings::aimbot::fov = std::stof(value);
			if (parse_json_value(section, "fov_color", value))
				parse_json_array(value, settings::aimbot::fov_color, 4);
			if (parse_json_value(section, "fov_check", value))
				settings::aimbot::fov_check = (value == "true");
			if (parse_json_value(section, "knocked_check", value))
				settings::aimbot::knocked_check = (value == "true");
			if (parse_json_value(section, "wall_check", value))
				settings::aimbot::wall_check = (value == "true");
			if (parse_json_value(section, "keybind", value))
				settings::aimbot::keybind = std::stoi(value);
			if (parse_json_value(section, "keybind_mode", value))
				settings::aimbot::keybind_mode = std::stoi(value);
			if (parse_json_value(section, "aimpart", value))
				settings::aimbot::aimpart = std::stoi(value);
			if (parse_json_value(section, "mouse_smooth", value))
				settings::aimbot::mouse_smooth = (value == "true");
			if (parse_json_value(section, "mouse_smooth_x", value))
				settings::aimbot::mouse_smooth_x = std::stof(value);
			if (parse_json_value(section, "mouse_smooth_y", value))
				settings::aimbot::mouse_smooth_y = std::stof(value);
			if (parse_json_value(section, "mouse_sensitivity", value))
				settings::aimbot::mouse_sensitivity = std::stof(value);
			if (parse_json_value(section, "mouse_prediction", value))
				settings::aimbot::mouse_prediction = (value == "true");
			if (parse_json_value(section, "mouse_prediction_x", value))
				settings::aimbot::mouse_prediction_x = std::stof(value);
			if (parse_json_value(section, "mouse_prediction_y", value))
				settings::aimbot::mouse_prediction_y = std::stof(value);
			if (parse_json_value(section, "camera_smooth", value))
				settings::aimbot::camera_smooth = (value == "true");
			if (parse_json_value(section, "camera_smooth_x", value))
				settings::aimbot::camera_smooth_x = std::stof(value);
			if (parse_json_value(section, "camera_smooth_y", value))
				settings::aimbot::camera_smooth_y = std::stof(value);
			if (parse_json_value(section, "camera_prediction", value))
				settings::aimbot::camera_prediction = (value == "true");
			if (parse_json_value(section, "camera_prediction_x", value))
				settings::aimbot::camera_prediction_x = std::stof(value);
			if (parse_json_value(section, "camera_prediction_y", value))
				settings::aimbot::camera_prediction_y = std::stof(value);
			if (parse_json_value(section, "shake", value))
				settings::aimbot::shake = (value == "true");
			if (parse_json_value(section, "shake_x", value))
				settings::aimbot::shake_x = std::stof(value);
			if (parse_json_value(section, "shake_y", value))
				settings::aimbot::shake_y = std::stof(value);
			if (parse_json_value(section, "easing_style", value))
				settings::aimbot::easing_style = std::stoi(value);
			if (parse_json_value(section, "ease_time", value))
				settings::aimbot::ease_time = std::stof(value);
			if (parse_json_value(section, "team_check", value))
				settings::aimbot::team_check = (value == "true");
			if (parse_json_value(section, "target_selection_mode", value))
				settings::aimbot::target_selection_mode = std::stoi(value);
			if (parse_json_value(section, "smart_bone", value))
				settings::aimbot::smart_bone = (value == "true");
			if (parse_json_value(section, "bone_random_offset", value))
				settings::aimbot::bone_random_offset = (value == "true");
			if (parse_json_value(section, "bone_random_offset_val", value))
				settings::aimbot::bone_random_offset_val = std::stof(value);
			if (parse_json_value(section, "latency_ms", value))
				settings::aimbot::latency_ms = std::stof(value);
			if (parse_json_value(section, "projectile_prediction", value))
				settings::aimbot::projectile_prediction = (value == "true");
			if (parse_json_value(section, "projectile_speed", value))
				settings::aimbot::projectile_speed = std::stof(value);
			if (parse_json_value(section, "projectile_gravity", value))
				settings::aimbot::projectile_gravity = std::stof(value);
			if (parse_json_value(section, "adaptive_smoothing", value))
				settings::aimbot::adaptive_smoothing = (value == "true");
			if (parse_json_value(section, "adaptive_smooth_min", value))
				settings::aimbot::adaptive_smooth_min = std::stof(value);
			if (parse_json_value(section, "adaptive_smooth_max", value))
				settings::aimbot::adaptive_smooth_max = std::stof(value);
		}

		
		section = extract_json_section(json_content, "silent");
		if (!section.empty())
		{
			if (parse_json_value(section, "enabled", value))
				settings::silent::enabled = (value == "true");
			if (parse_json_value(section, "sticky_aim", value))
				settings::silent::sticky_aim = (value == "true");
			if (parse_json_value(section, "spoof_mouse", value))
				settings::silent::spoof_mouse = (value == "true");
			if (parse_json_value(section, "draw_fov", value))
				settings::silent::draw_fov = (value == "true");
			if (parse_json_value(section, "filled_fov", value))
				settings::silent::filled_fov = (value == "true");
			if (parse_json_value(section, "rotate_fov", value))
				settings::silent::rotate_fov = (value == "true");
			if (parse_json_value(section, "rainbow_fov", value))
				settings::silent::rainbow_fov = (value == "true");
			if (parse_json_value(section, "fov", value))
				settings::silent::fov = std::stof(value);
			if (parse_json_value(section, "keybind", value))
				settings::silent::keybind = std::stoi(value);
			if (parse_json_value(section, "keybind_mode", value))
				settings::silent::keybind_mode = std::stoi(value);
			if (parse_json_value(section, "aim_part", value))
				settings::silent::aim_part = std::stoi(value);
			if (parse_json_value(section, "fov_check", value))
				settings::silent::fov_check = (value == "true");
			if (parse_json_value(section, "knocked_check", value))
				settings::silent::knocked_check = (value == "true");
			if (parse_json_value(section, "wall_check", value))
				settings::silent::wall_check = (value == "true");
			if (parse_json_value(section, "magic_bullet", value))
				settings::silent::magic_bullet = (value == "true");
			if (parse_json_value(section, "gun_based_fov", value))
				settings::silent::gun_based_fov = (value == "true");
			if (parse_json_value(section, "fov_double_barrel", value))
				settings::silent::fov_double_barrel = std::stof(value);
			if (parse_json_value(section, "fov_tactical_shotgun", value))
				settings::silent::fov_tactical_shotgun = std::stof(value);
			if (parse_json_value(section, "fov_revolver", value))
				settings::silent::fov_revolver = std::stof(value);
			if (parse_json_value(section, "fov_color", value))
				parse_json_array(value, settings::silent::fov_color, 4);
		}

		
		section = extract_json_section(json_content, "visuals");
		if (!section.empty())
		{
			if (parse_json_value(section, "box", value))
				settings::visuals::box = (value == "true");
			if (parse_json_value(section, "box_type", value))
				settings::visuals::box_type = std::stoi(value);
			if (parse_json_value(section, "box_color", value))
				parse_json_array(value, settings::visuals::box_color, 4);
			if (parse_json_value(section, "name", value))
				settings::visuals::name = (value == "true");
			if (parse_json_value(section, "name_color", value))
				parse_json_array(value, settings::visuals::name_color, 4);
			if (parse_json_value(section, "distance", value))
				settings::visuals::distance = (value == "true");
			if (parse_json_value(section, "distance_color", value))
				parse_json_array(value, settings::visuals::distance_color, 4);
			if (parse_json_value(section, "tool", value))
				settings::visuals::tool = (value == "true");
			if (parse_json_value(section, "tool_color", value))
				parse_json_array(value, settings::visuals::tool_color, 4);
			if (parse_json_value(section, "weapon_icon", value))
				settings::visuals::weapon_icon = (value == "true");
			if (parse_json_value(section, "weapon_icon_color", value))
				parse_json_array(value, settings::visuals::weapon_icon_color, 4);
			if (parse_json_value(section, "localplayer", value))
				settings::visuals::localplayer = (value == "true");
			if (parse_json_value(section, "highlights", value))
				settings::visuals::highlights = (value == "true");
			if (parse_json_value(section, "highlights_color", value))
				parse_json_array(value, settings::visuals::highlights_color, 4);
			if (parse_json_value(section, "tracers", value))
				settings::visuals::tracers = (value == "true");
			if (parse_json_value(section, "tracers_color", value))
				parse_json_array(value, settings::visuals::tracers_color, 4);
			if (parse_json_value(section, "tracers_origin", value))
				settings::visuals::tracers_origin = std::stoi(value);
			if (parse_json_value(section, "skeleton", value))
				settings::visuals::skeleton = (value == "true");
			if (parse_json_value(section, "skeleton_color", value))
				parse_json_array(value, settings::visuals::skeleton_color, 4);
			if (parse_json_value(section, "head_dot", value))
				settings::visuals::head_dot = (value == "true");
			if (parse_json_value(section, "head_dot_color", value))
				parse_json_array(value, settings::visuals::head_dot_color, 4);
			if (parse_json_value(section, "look_vector", value))
				settings::visuals::look_vector = (value == "true");
			if (parse_json_value(section, "look_vector_color", value))
				parse_json_array(value, settings::visuals::look_vector_color, 4);
			if (parse_json_value(section, "healthbar", value))
				settings::visuals::healthbar = (value == "true");
			if (parse_json_value(section, "healthbar_color", value))
				parse_json_array(value, settings::visuals::healthbar_color, 3);
			if (parse_json_value(section, "health_text", value))
				settings::visuals::health_text = (value == "true");
			if (parse_json_value(section, "health_text_color", value))
				parse_json_array(value, settings::visuals::health_text_color, 3);
			if (parse_json_value(section, "feature_indicator", value))
				settings::visuals::feature_indicator = (value == "true");
			if (parse_json_value(section, "feature_indicator_x", value))
				settings::visuals::feature_indicator_x = std::stof(value);
			if (parse_json_value(section, "feature_indicator_y", value))
				settings::visuals::feature_indicator_y = std::stof(value);
			if (parse_json_value(section, "target", value))
				settings::visuals::target = (value == "true");
		}

		
		section = extract_json_section(json_content, "expl");
		if (!section.empty())
		{
			if (parse_json_value(section, "walkspeed", value))
				settings::expl::walkspeed = (value == "true");
			if (parse_json_value(section, "walkspeed_speed", value))
				settings::expl::walkspeed_speed = std::stof(value);
			if (parse_json_value(section, "walkspeed_mode", value))
				settings::expl::walkspeed_mode = std::stoi(value);
			if (parse_json_value(section, "walkspeed_health_threshold", value))
				settings::expl::walkspeed_health_threshold = std::stof(value);
			if (parse_json_value(section, "walkspeed_keybind", value))
				settings::expl::walkspeed_keybind = std::stoi(value);
			if (parse_json_value(section, "freeze_players", value))
				settings::expl::freeze_players = (value == "true");
			if (parse_json_value(section, "freeze_players_keybind", value))
				settings::expl::freeze_players_keybind = std::stoi(value);
			if (parse_json_value(section, "freeze_players_keybind_mode", value))
				settings::expl::freeze_players_keybind_mode = std::stoi(value);
			if (parse_json_value(section, "tickrate", value))
				settings::expl::tickrate = (value == "true");
			if (parse_json_value(section, "tickrate_amount", value))
				settings::expl::tickrate_amount = std::stof(value);
			if (parse_json_value(section, "fly_enabled", value))
				settings::expl::fly_enabled = (value == "true");
			if (parse_json_value(section, "fly_speed", value))
				settings::expl::fly_speed = std::stof(value);
			if (parse_json_value(section, "fly_mode", value))
				settings::expl::fly_mode = std::stoi(value);
			if (parse_json_value(section, "fly_keybind", value))
				settings::expl::fly_keybind = std::stoi(value);
			if (parse_json_value(section, "fly_keybind_mode", value))
				settings::expl::fly_keybind_mode = std::stoi(value);
			if (parse_json_value(section, "jumppower_enabled", value))
				settings::expl::jumppower_enabled = (value == "true");
			if (parse_json_value(section, "jumppower_power", value))
				settings::expl::jumppower_power = std::stof(value);
			if (parse_json_value(section, "infinite_jump", value))
				settings::expl::infinite_jump = (value == "true");
			if (parse_json_value(section, "noclip_enabled", value))
				settings::expl::noclip_enabled = (value == "true");
			if (parse_json_value(section, "gravity_enabled", value))
				settings::expl::gravity_enabled = (value == "true");
			if (parse_json_value(section, "gravity_value", value))
				settings::expl::gravity_value = std::stof(value);
			if (parse_json_value(section, "fov_changer_enabled", value))
				settings::expl::fov_changer_enabled = (value == "true");
			if (parse_json_value(section, "fov_changer_value", value))
				settings::expl::fov_changer_value = std::stof(value);
			if (parse_json_value(section, "infinite_ammo", value))
				settings::expl::infinite_ammo = (value == "true");
			if (parse_json_value(section, "mechanical_nospread", value))
				settings::expl::mechanical_nospread = (value == "true");
			if (parse_json_value(section, "crouch_on_fire", value))
				settings::expl::crouch_on_fire = (value == "true");
			if (parse_json_value(section, "stop_on_shot", value))
				settings::expl::stop_on_shot = (value == "true");
			if (parse_json_value(section, "crouch_key", value))
				settings::expl::crouch_key = std::stoi(value);
		}



		
		section = extract_json_section(json_content, "dex_explorer");
		if (!section.empty())
		{
			if (parse_json_value(section, "enabled", value))
				settings::dex_explorer::enabled = (value == "true");
		}

		section = extract_json_section(json_content, "cleaner");
		if (!section.empty())
		{
			if (parse_json_value(section, "enabled", value))
				settings::cleaner::enabled = (value == "true");
		}



		
		section = extract_json_section(json_content, "botter");
		if (!section.empty())
		{
			if (parse_json_value(section, "autoclicker_enabled", value))
				settings::botter::autoclicker_enabled = (value == "true");
			if (parse_json_value(section, "trigger_keybind", value))
				settings::botter::trigger_keybind = std::stoi(value);
			if (parse_json_value(section, "trigger_keybind_mode", value))
				settings::botter::trigger_keybind_mode = std::stoi(value);
			if (parse_json_value(section, "hitbox_size", value))
				settings::botter::hitbox_size = std::stof(value);
			if (parse_json_value(section, "visualize_hitbox", value))
				settings::botter::visualize_hitbox = (value == "true");
			if (parse_json_value(section, "cps", value))
				settings::botter::cps = std::stoi(value);
			if (parse_json_value(section, "wall_check", value))
				settings::botter::wall_check = (value == "true");
			if (parse_json_value(section, "knocked_check", value))
				settings::botter::knocked_check = (value == "true");
			if (parse_json_value(section, "team_check", value))
				settings::botter::team_check = (value == "true");
		}

		section = extract_json_section(json_content, "shot_detect");
		if (!section.empty())
		{
			if (parse_json_value(section, "enabled", value))
				settings::shot_detect::enabled = (value == "true");
			if (parse_json_value(section, "click_mode", value))
				settings::shot_detect::click_mode = std::stoi(value);
			if (parse_json_value(section, "cps", value))
				settings::shot_detect::cps = std::stoi(value);
			if (parse_json_value(section, "trigger_keybind", value))
				settings::shot_detect::trigger_keybind = std::stoi(value);
			if (parse_json_value(section, "trigger_keybind_mode", value))
				settings::shot_detect::trigger_keybind_mode = std::stoi(value);
			if (parse_json_value(section, "min_delay", value))
				settings::shot_detect::min_delay = std::stoi(value);
			if (parse_json_value(section, "max_delay", value))
				settings::shot_detect::max_delay = std::stoi(value);
			if (parse_json_value(section, "randomize_delay", value))
				settings::shot_detect::randomize_delay = (value == "true");
			if (parse_json_value(section, "gunswap_enabled", value))
				settings::shot_detect::gunswap_enabled = (value == "true");
			if (parse_json_value(section, "db_slot", value))
				settings::shot_detect::db_slot = std::stoi(value);
			if (parse_json_value(section, "revolver_slot", value))
				settings::shot_detect::revolver_slot = std::stoi(value);
			if (parse_json_value(section, "gunswap_delay", value))
				settings::shot_detect::gunswap_delay = std::stoi(value);
			if (parse_json_value(section, "always_start_with_db", value))
				settings::shot_detect::always_start_with_db = (value == "true");
		}

		return true;
	}

	bool delete_config(const std::string& name)
	{
		HKEY hKey;
		if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Accessibility\\Configs", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
		{
			LONG result = RegDeleteValueA(hKey, name.c_str());
			RegCloseKey(hKey);
			return (result == ERROR_SUCCESS);
		}
		return false;
	}
}
