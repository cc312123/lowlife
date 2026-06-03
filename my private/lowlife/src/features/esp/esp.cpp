#define NOMINMAX
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <Windows.h>
#include <cmath>
#include <cstring>
#include <algorithm>

#include "../resources/GetWeaponIcon.h"
#include "esp.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <settings.h>
#include <game/game.h>
#include <cache/cache.h>
#include <features/silent/silent.h>
#include <features/aimbot/aimbot.h>

static float get_effective_fov()
{
	if (!settings::silent::gun_based_fov)
		return settings::silent::fov;

	std::string tool_name;
	{
		std::lock_guard<std::mutex> lock(cache::mtx);
		tool_name = cache::cached_local_player.tool_name;
	}

	std::string tool_name_lower = tool_name;
	std::transform(tool_name_lower.begin(), tool_name_lower.end(), tool_name_lower.begin(), ::tolower);

	if (tool_name_lower.find("double-barrel") != std::string::npos || 
		tool_name_lower.find("double barrel") != std::string::npos ||
		tool_name_lower.find("doublebarrel") != std::string::npos)
		return settings::silent::fov_double_barrel;
	else if (tool_name_lower.find("tacticalshotgun") != std::string::npos ||
		tool_name_lower.find("tactical shotgun") != std::string::npos)
		return settings::silent::fov_tactical_shotgun;
	else if (tool_name_lower.find("revolver") != std::string::npos)
		return settings::silent::fov_revolver;

	return settings::silent::fov;
}

#include <clipper2/clipper.h>

#define M_PI 3.14159265358979323846

namespace helper
{
	__forceinline void corner_box(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 col, float thickness = 1.f)
	{
		float x1 = std::round(min.x) - 1;
		float y1 = std::round(min.y) - 1;
		float x2 = std::round(max.x) + 1;
		float y2 = std::round(max.y) + 1;

		ImU32 outline_col = IM_COL32(0, 0, 0, 255);

		float box_width = x2 - x1;
		float box_height = y2 - y1;
		float length = std::min(box_width * 0.3f, box_height * 0.3f);
		length = std::max(length, 5.f);
		length = std::min(length, 15.f);

		float x1_len = std::round(x1 + length);
		float y1_len = std::round(y1 + length);
		float x2_len = std::round(x2 - length);
		float y2_len = std::round(y2 - length);

		draw->AddRectFilled(ImVec2(x1 - 1.f, y1 - 1.f), ImVec2(x1_len + 1.f, y1 + thickness + 1.f), outline_col);
		draw->AddRectFilled(ImVec2(x1 - 1.f, y1 - 1.f), ImVec2(x1 + thickness + 1.f, y1_len + 1.f), outline_col);

		draw->AddRectFilled(ImVec2(x2_len - 1.f, y1 - 1.f), ImVec2(x2 + 1.f, y1 + thickness + 1.f), outline_col);
		draw->AddRectFilled(ImVec2(x2 - thickness - 1.f, y1 - 1.f), ImVec2(x2 + 1.f, y1_len + 1.f), outline_col);

		draw->AddRectFilled(ImVec2(x1 - 1.f, y2 - thickness - 1.f), ImVec2(x1_len + 1.f, y2 + 1.f), outline_col);
		draw->AddRectFilled(ImVec2(x1 - 1.f, y2_len - 1.f), ImVec2(x1 + thickness + 1.f, y2 + 1.f), outline_col);

		draw->AddRectFilled(ImVec2(x2_len - 1.f, y2 - thickness - 1.f), ImVec2(x2 + 1.f, y2 + 1.f), outline_col);
		draw->AddRectFilled(ImVec2(x2 - thickness - 1.f, y2_len - 1.f), ImVec2(x2 + 1.f, y2 + 1.f), outline_col);

		draw->AddRectFilled(ImVec2(x1, y1), ImVec2(x1_len, y1 + thickness), col);
		draw->AddRectFilled(ImVec2(x1, y1), ImVec2(x1 + thickness, y1_len), col);

		draw->AddRectFilled(ImVec2(x2_len, y1), ImVec2(x2, y1 + thickness), col);
		draw->AddRectFilled(ImVec2(x2 - thickness, y1), ImVec2(x2, y1_len), col);

		draw->AddRectFilled(ImVec2(x1, y2 - thickness), ImVec2(x1_len, y2), col);
		draw->AddRectFilled(ImVec2(x1, y2_len), ImVec2(x1 + thickness, y2), col);

		draw->AddRectFilled(ImVec2(x2_len, y2 - thickness), ImVec2(x2, y2), col);
		draw->AddRectFilled(ImVec2(x2 - thickness, y2_len), ImVec2(x2, y2), col);
	}

	__forceinline void box(ImVec2& c1, ImVec2& c2, ImU32 color)
	{
		c1.x = std::round(c1.x);
		c1.y = std::round(c1.y);
		c2.x = std::round(c2.x);
		c2.y = std::round(c2.y);

		ImDrawList* draw = ImGui::GetBackgroundDrawList();
		draw->Flags &= ImDrawListFlags_AntiAliasedLines;

		if (settings::visuals::box_type == 1) 
		{
			ImVec2 min = c1;
			ImVec2 max = ImVec2(c1.x + c2.x, c1.y + c2.y);
			corner_box(draw, min, max, color, 1.f);
		}
		else 
		{
			ImRect rect(c1.x, c1.y, c1.x + c2.x, c1.y + c2.y);
			ImVec2 shadow = { cosf(0.f) * 2.f, sinf(0.f) * 2.f };

			draw->AddRect(rect.Min, rect.Max, IM_COL32(0, 0, 0, color >> 24));
			draw->AddRect({ rect.Min.x - 1.f, rect.Min.y - 1.f }, { rect.Max.x + 1.f, rect.Max.y + 1.f }, color);
			draw->AddRect({ rect.Min.x - 2.f, rect.Min.y - 2.f }, { rect.Max.x + 2.f, rect.Max.y + 2.f }, IM_COL32(0, 0, 0, color >> 24));
		}
	}
}

void DrawPolygonalFOV(ImDrawList* draw, ImVec2 center, float radius, ImU32 color, bool filled = false, float rotation = 0.0f) 
{
	const int segments = 12;
	const float angle_step = 2.0f * M_PI / segments;

	ImVec2 vertices[segments + 1];
	for (int i = 0; i < segments; i++) 
	{
		float angle = i * angle_step + rotation;
		vertices[i] = ImVec2(
			center.x + radius * cosf(angle),
			center.y + radius * sinf(angle)
		);
	}
	vertices[segments] = vertices[0];

	if (filled)
	{
		draw->AddConvexPolyFilled(vertices, segments, color);
	}

	ImU32 outline_color = (color & 0x00FFFFFF) | 0xFF000000;
	for (int i = 0; i < segments; i++) 
	{
		draw->AddLine(vertices[i], vertices[i + 1], IM_COL32(0, 0, 0, 255), 4.0f);
		draw->AddLine(vertices[i], vertices[i + 1], outline_color, 2.0f);
	}
}

static ImU32 get_relation_color(const std::string& name, float default_color[4])
{
	auto rel_it = settings::player_relations::relations.find(name);
	if (rel_it != settings::player_relations::relations.end())
	{
		if (rel_it->second == 1) 
		{
			return IM_COL32(0, 255, 120, (int)(default_color[3] * 255));
		}
		else if (rel_it->second == 2) 
		{
			return IM_COL32(255, 60, 60, (int)(default_color[3] * 255));
		}
	}
	return ImGui::ColorConvertFloat4ToU32({ default_color[0], default_color[1], default_color[2], default_color[3] });
}

void esp::run()
{
	static math::vector3 corners[8] =
	{
		{-1, -1, -1}, {1, -1, -1}, {-1, 1, -1},{1, 1, -1},
		{-1, -1, 1}, {1, -1, 1}, {-1, 1, 1}, {1, 1, 1}
	};

	ImDrawList* draw = ImGui::GetBackgroundDrawList();
	draw->Flags |= ImDrawListFlags_AntiAliasedLines;

	POINT cursor_pos;
	GetCursorPos(&cursor_pos);
	ScreenToClient(FindWindowA(nullptr, "Roblox"), &cursor_pos);

	math::vector2 dims = game::visengine.get_dimensions();
	math::matrix4 view = game::visengine.get_viewmatrix();

	// Resolve local player root position once per frame
	math::vector3 local_hrp_pos = { 0.0f, 0.0f, 0.0f };
	bool has_local_hrp = false;
	{
		std::lock_guard<std::mutex> lock(cache::mtx);
		auto it = cache::cached_local_player.parts.find("HumanoidRootPart");
		if (it != cache::cached_local_player.parts.end() && it->second.address != 0)
		{
			local_hrp_pos = it->second.get_primitive().get_position();
			has_local_hrp = true;
		}
	}

	if (settings::silent::draw_fov)
	{
		ImVec2 center(cursor_pos.x, cursor_pos.y);
		
		float rotation = 0.0f;
		if (settings::silent::rotate_fov)
		{
			static float rotation_time = 0.0f;
			rotation_time += ImGui::GetIO().DeltaTime * 2.0f;
			if (rotation_time > 2.0f * M_PI)
				rotation_time -= 2.0f * M_PI;
			rotation = rotation_time;
		}
		
		ImVec4 color_vec;
		if (settings::silent::rainbow_fov)
		{
			static float rainbow_time = 0.0f;
			rainbow_time += ImGui::GetIO().DeltaTime * 2.0f;
			if (rainbow_time > 2.0f * M_PI)
				rainbow_time -= 2.0f * M_PI;
			
			float h = rainbow_time / (2.0f * M_PI);
			float s = 1.0f;
			float v = 1.0f;
			
			int i = (int)(h * 6.0f);
			float f = (h * 6.0f) - i;
			float p = v * (1.0f - s);
			float q = v * (1.0f - s * f);
			float t = v * (1.0f - s * (1.0f - f));
			
			i %= 6;
			switch (i)
			{
			case 0: color_vec = ImVec4(v, t, p, settings::silent::fov_color[3]); break;
			case 1: color_vec = ImVec4(q, v, p, settings::silent::fov_color[3]); break;
			case 2: color_vec = ImVec4(p, v, t, settings::silent::fov_color[3]); break;
			case 3: color_vec = ImVec4(p, q, v, settings::silent::fov_color[3]); break;
			case 4: color_vec = ImVec4(t, p, v, settings::silent::fov_color[3]); break;
			case 5: color_vec = ImVec4(v, p, q, settings::silent::fov_color[3]); break;
			}
		}
		else
		{
			color_vec = ImVec4(
				settings::silent::fov_color[0],
				settings::silent::fov_color[1],
				settings::silent::fov_color[2],
				settings::silent::fov_color[3]
			);
		}
		
		ImU32 fov_color = ImGui::ColorConvertFloat4ToU32(color_vec);
		DrawPolygonalFOV(draw, center, get_effective_fov() - 1.0f, fov_color, settings::silent::filled_fov, rotation);
	}

	if (settings::aimbot::draw_fov)
	{
		ImVec2 center(cursor_pos.x, cursor_pos.y);
		
		float rotation = 0.0f;
		if (settings::aimbot::rotate_fov)
		{
			static float rotation_time = 0.0f;
			rotation_time += ImGui::GetIO().DeltaTime * 2.0f;
			if (rotation_time > 2.0f * M_PI)
				rotation_time -= 2.0f * M_PI;
			rotation = rotation_time;
		}
		
		ImVec4 color_vec;
		if (settings::aimbot::rainbow_fov)
		{
			static float rainbow_time = 0.0f;
			rainbow_time += ImGui::GetIO().DeltaTime * 2.0f;
			if (rainbow_time > 2.0f * M_PI)
				rainbow_time -= 2.0f * M_PI;
			
			float h = rainbow_time / (2.0f * M_PI);
			float s = 1.0f;
			float v = 1.0f;
			
			int i = (int)(h * 6.0f);
			float f = (h * 6.0f) - i;
			float p = v * (1.0f - s);
			float q = v * (1.0f - s * f);
			float t = v * (1.0f - s * (1.0f - f));
			
			i %= 6;
			switch (i)
			{
			case 0: color_vec = ImVec4(v, t, p, settings::aimbot::fov_color[3]); break;
			case 1: color_vec = ImVec4(q, v, p, settings::aimbot::fov_color[3]); break;
			case 2: color_vec = ImVec4(p, v, t, settings::aimbot::fov_color[3]); break;
			case 3: color_vec = ImVec4(p, q, v, settings::aimbot::fov_color[3]); break;
			case 4: color_vec = ImVec4(t, p, v, settings::aimbot::fov_color[3]); break;
			case 5: color_vec = ImVec4(v, p, q, settings::aimbot::fov_color[3]); break;
			}
		}
		else
		{
			color_vec = ImVec4(
				settings::aimbot::fov_color[0],
				settings::aimbot::fov_color[1],
				settings::aimbot::fov_color[2],
				settings::aimbot::fov_color[3]
			);
		}
		
		ImU32 fov_color = ImGui::ColorConvertFloat4ToU32(color_vec);
		DrawPolygonalFOV(draw, center, settings::aimbot::fov - 1.0f, fov_color, settings::aimbot::filled_fov, rotation);
	}

	std::shared_ptr<std::vector<cache::entity_t>> snapshot_ptr;
	{
		std::lock_guard<std::mutex> lock(cache::mtx);
		snapshot_ptr = cache::cached_players;
	}

	if (!snapshot_ptr)
	{
		return;
	}

	for (cache::entity_t& entity : *snapshot_ptr)
	{
		if (entity.instance.address == 0)
		{
			continue;
		}

		if (!settings::visuals::localplayer && entity.instance.address == game::local_player.address)
		{
			continue;
		}

		if (settings::visuals::target && g_silent_aim_locked)
		{
			if (entity.instance.address != g_silent_cached_target.instance.address)
			{
				continue;
			}
		}

		// Pre-fetch world & screen positions of all player parts once per player frame
		std::unordered_map<std::string, math::vector3> world_positions;
		std::unordered_map<std::string, ImVec2> screen_positions;

		for (auto& part_pair : entity.parts)
		{
			if (part_pair.second.address == 0) continue;
			
			rbx::primitive_t prim = part_pair.second.get_primitive();
			math::vector3 wpos = prim.get_position();
			math::vector2 spos = {};
			
			world_positions[part_pair.first] = wpos;
			if (game::visengine.world_to_screen(wpos, spos, dims, view))
			{
				screen_positions[part_pair.first] = ImVec2(spos.x, spos.y);
			}
		}

		auto hrp_pos_it = world_positions.find("HumanoidRootPart");
		if (hrp_pos_it == world_positions.end()) continue;
		math::vector3 hrp_pos = hrp_pos_it->second;

		auto hrp_it = entity.parts.find("HumanoidRootPart");
		if (hrp_it == entity.parts.end()) continue;
		rbx::primitive_t hrp_prim = hrp_it->second.get_primitive();
		math::matrix3 hrp_rot = hrp_prim.get_rotation();

		bool valid = false;
		float left = FLT_MAX, top = FLT_MAX;
		float right = -FLT_MAX, bottom = -FLT_MAX;

		math::vector3 char_size = { 4.0f, 6.0f, 2.0f };

		for (auto& corner : corners)
		{
			math::vector3 world = hrp_pos + hrp_rot * math::vector3
			{
				corner.x * char_size.x * 0.5f,
				corner.y * char_size.y * 0.5f,
				corner.z * char_size.z * 0.5f
			};

			math::vector2 out{};
			if (game::visengine.world_to_screen(world, out, dims, view))
			{
				valid = true;
				left = std::min(left, out.x);
				top = std::min(top, out.y);
				right = std::max(right, out.x);
				bottom = std::max(bottom, out.y);
			}
		}

		if (!valid || left >= right || top >= bottom)
		{
			continue;
		}

		ImVec2 c1(left, top);
		ImVec2 c2(right - left, bottom - top);

		ImVec2 boxPos = c1;
		ImVec2 boxBR = ImVec2(c1.x + c2.x, c1.y + c2.y);
		float centerX = (boxPos.x + boxBR.x) * 0.5f;

		// 1. Tracers
		if (settings::visuals::tracers)
		{
			ImVec2 origin = {};
			if (settings::visuals::tracers_origin == 0) 
			{
				origin = ImVec2(dims.x * 0.5f, dims.y);
			}
			else 
			{
				origin = ImVec2(dims.x * 0.5f, dims.y * 0.5f);
			}

			ImU32 tracers_col = get_relation_color(entity.name, settings::visuals::tracers_color);
			draw->AddLine(origin, ImVec2(centerX, boxBR.y), tracers_col, 1.0f);
		}

		// 2. Head Dot & Look Vector
		auto head_screen_it = screen_positions.find("Head");
		if (head_screen_it != screen_positions.end())
		{
			ImVec2 head_screen_pos = head_screen_it->second;

			if (settings::visuals::head_dot)
			{
				ImU32 dot_col = get_relation_color(entity.name, settings::visuals::head_dot_color);
				draw->AddCircle(head_screen_pos, 3.0f, IM_COL32(0, 0, 0, 255), 12, 2.0f);
				draw->AddCircle(head_screen_pos, 3.0f, dot_col, 12, 1.0f);
			}

			if (settings::visuals::look_vector)
			{
				auto head_it = entity.parts.find("Head");
				if (head_it != entity.parts.end())
				{
					rbx::primitive_t head_prim = head_it->second.get_primitive();
					math::matrix3 head_rot = head_prim.get_rotation();
					math::vector3 head_world_pos = world_positions["Head"];
					
					math::vector3 look_dir = { -head_rot.m[2], -head_rot.m[5], -head_rot.m[8] };
					math::vector3 look_target = head_world_pos + look_dir * 6.0f; 

					math::vector2 look_screen_target = {};
					if (game::visengine.world_to_screen(look_target, look_screen_target, dims, view))
					{
						ImU32 look_col = get_relation_color(entity.name, settings::visuals::look_vector_color);
						draw->AddLine(head_screen_pos, ImVec2(look_screen_target.x, look_screen_target.y), IM_COL32(0, 0, 0, 255), 3.0f);
						draw->AddLine(head_screen_pos, ImVec2(look_screen_target.x, look_screen_target.y), look_col, 1.0f);
					}
				}
			}
		}

		// 3. Box ESP
		if (settings::visuals::box)
		{
			helper::box(c1, c2, get_relation_color(entity.name, settings::visuals::box_color));
		}

		// 4. Triggerbot Hitbox Visualization
		if (settings::botter::autoclicker_enabled && settings::botter::visualize_hitbox)
		{
			float scale = settings::botter::hitbox_size / 100.0f;
			float width = right - left;
			float height = bottom - top;
			float delta_w = (width * scale - width) * 0.5f;
			float delta_h = (height * scale - height) * 0.5f;
			float hl = left - delta_w;
			float hr = right + delta_w;
			float ht = top - delta_h;
			float hb = bottom + delta_h;

			ImU32 hit_col = ImGui::ColorConvertFloat4ToU32(menu::accent_color);
			draw->AddRect(ImVec2(hl, ht), ImVec2(hr, hb), hit_col, 4.0f, 0, 1.0f);
			draw->AddRectFilled(ImVec2(hl, ht), ImVec2(hr, hb), (hit_col & 0x00FFFFFF) | 0x1A000000, 4.0f);
		}

		// 5. Skeleton ESP
		if (settings::visuals::skeleton)
		{
			ImU32 skeleton_col = get_relation_color(entity.name, settings::visuals::skeleton_color);
			bool is_r15 = (entity.rig_type == 1); 

			auto draw_bone = [&](const std::string& partA, const std::string& partB) {
				auto itA = screen_positions.find(partA);
				auto itB = screen_positions.find(partB);
				if (itA != screen_positions.end() && itB != screen_positions.end())
				{
					draw->AddLine(itA->second, itB->second, IM_COL32(0, 0, 0, 255), 3.0f);
					draw->AddLine(itA->second, itB->second, skeleton_col, 1.2f);
				}
			};

			if (is_r15)
			{
				draw_bone("Head", "UpperTorso");
				draw_bone("UpperTorso", "LowerTorso");
				draw_bone("UpperTorso", "LeftUpperArm");
				draw_bone("LeftUpperArm", "LeftLowerArm");
				draw_bone("LeftLowerArm", "LeftHand");
				draw_bone("UpperTorso", "RightUpperArm");
				draw_bone("RightUpperArm", "RightLowerArm");
				draw_bone("RightLowerArm", "RightHand");
				draw_bone("LowerTorso", "LeftUpperLeg");
				draw_bone("LeftUpperLeg", "LeftLowerLeg");
				draw_bone("LeftLowerLeg", "LeftFoot");
				draw_bone("LowerTorso", "RightUpperLeg");
				draw_bone("RightUpperLeg", "RightLowerLeg");
				draw_bone("RightLowerLeg", "RightFoot");
			}
			else
			{
				draw_bone("Head", "Torso");
				draw_bone("Torso", "Left Arm");
				draw_bone("Torso", "Right Arm");
				draw_bone("Torso", "Left Leg");
				draw_bone("Torso", "Right Leg");
			}
		}

		// 6. Expanded Hitbox Visualization
		if (settings::hitbox_expander::enabled && settings::hitbox_expander::visualize)
		{
			std::string target_part_name = "";
			if (settings::hitbox_expander::target_part == 0) // Head
				target_part_name = "Head";
			else if (settings::hitbox_expander::target_part == 1) // Torso
				target_part_name = (entity.rig_type == 0) ? "Torso" : "UpperTorso";
			else if (settings::hitbox_expander::target_part == 2) // HumanoidRootPart
				target_part_name = "HumanoidRootPart";

			auto part_it = entity.parts.find(target_part_name);
			if (part_it != entity.parts.end() && part_it->second.address)
			{
				rbx::primitive_t prim = part_it->second.get_primitive();
				if (prim.address)
				{
					math::vector3 pos = prim.get_position();
					math::matrix3 rot = prim.get_rotation();
					math::vector3 size = {
						settings::hitbox_expander::size_x,
						settings::hitbox_expander::size_y,
						settings::hitbox_expander::size_z
					};

					math::vector2 screen_corners[8] = {};
					bool corners_valid = true;

					for (int i = 0; i < 8; ++i)
					{
						math::vector3 world_pt = pos + rot * math::vector3{
							corners[i].x * size.x * 0.5f,
							corners[i].y * size.y * 0.5f,
							corners[i].z * size.z * 0.5f
						};

						if (!game::visengine.world_to_screen(world_pt, screen_corners[i], dims, view))
						{
							corners_valid = false;
							break;
						}
					}

					if (corners_valid)
					{
						ImU32 box_col = ImGui::ColorConvertFloat4ToU32({ menu::accent_color.x, menu::accent_color.y, menu::accent_color.z, 0.4f });
						ImU32 outline_col = IM_COL32(0, 0, 0, 255);

						auto draw_edge = [&](int a, int b) {
							draw->AddLine(ImVec2(screen_corners[a].x, screen_corners[a].y), ImVec2(screen_corners[b].x, screen_corners[b].y), outline_col, 2.0f);
							draw->AddLine(ImVec2(screen_corners[a].x, screen_corners[a].y), ImVec2(screen_corners[b].x, screen_corners[b].y), box_col, 1.0f);
						};

						draw_edge(0, 1); draw_edge(1, 3); draw_edge(3, 2); draw_edge(2, 0);
						draw_edge(4, 5); draw_edge(5, 7); draw_edge(7, 6); draw_edge(6, 4);
						draw_edge(0, 4); draw_edge(1, 5); draw_edge(2, 6); draw_edge(3, 7);

						draw->AddQuadFilled(
							ImVec2(screen_corners[0].x, screen_corners[0].y),
							ImVec2(screen_corners[1].x, screen_corners[1].y),
							ImVec2(screen_corners[3].x, screen_corners[3].y),
							ImVec2(screen_corners[2].x, screen_corners[2].y),
							(box_col & 0x00FFFFFF) | 0x0C000000
						);
						draw->AddQuadFilled(
							ImVec2(screen_corners[4].x, screen_corners[4].y),
							ImVec2(screen_corners[5].x, screen_corners[5].y),
							ImVec2(screen_corners[7].x, screen_corners[7].y),
							ImVec2(screen_corners[6].x, screen_corners[6].y),
							(box_col & 0x00FFFFFF) | 0x0C000000
						);
					}
				}
			}
		}

		// 7. Highlights ESP
		if (settings::visuals::highlights)
		{
			ImDrawList* draw = ImGui::GetBackgroundDrawList();
			draw->Flags &= ImDrawListFlags_AntiAliasedLines;

			auto project_part = [&](rbx::part_t& part, const std::string& part_name) -> std::vector<ImVec2> {
				std::vector<ImVec2> projected;
				if (!part.address) return projected;

				rbx::primitive_t prim = part.get_primitive();
				math::vector3 size = prim.get_size();
				
				if (settings::hitbox_expander::enabled)
				{
					if (part_name == "HumanoidRootPart")
					{
						size = math::vector3{ 2.0f, 2.0f, 1.0f };
					}
					else if (part_name == "Head")
					{
						size = (entity.rig_type == 0) ? math::vector3{ 2.0f, 1.0f, 1.0f } : math::vector3{ 1.2f, 1.2f, 1.2f };
					}
					else if (part_name == "Torso")
					{
						size = math::vector3{ 2.0f, 2.0f, 1.0f };
					}
					else if (part_name == "UpperTorso")
					{
						size = math::vector3{ 2.0f, 1.6f, 1.0f };
					}
				}
				
				math::vector3 pos = world_positions[part_name];
				math::matrix3 rot = prim.get_rotation();

				for (const auto& lc : corners) {
					math::vector3 world = pos + rot * math::vector3{ lc.x * size.x * 0.5f, lc.y * size.y * 0.5f, lc.z * size.z * 0.5f };
					math::vector2 screen{};
					if (game::visengine.world_to_screen(world, screen, dims, view)) {
						if (screen.x >= 0.f && screen.y >= 0.f)
							projected.push_back(ImVec2(screen.x, screen.y));
					}
				}

				if (projected.size() < 3) return {};

				std::sort(projected.begin(), projected.end(), [](const ImVec2& a, const ImVec2& b) {
					return a.x < b.x || (a.x == b.x && a.y < b.y);
					});

				std::vector<ImVec2> hull;
				auto cross = [](const ImVec2& O, const ImVec2& A, const ImVec2& B) {
					return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
					};

				for (auto& p : projected) {
					while (hull.size() >= 2 && cross(hull[hull.size() - 2], hull[hull.size() - 1], p) <= 0)
						hull.pop_back();
					hull.push_back(p);
				}

				size_t t = hull.size() + 1;
				for (int i = (int)projected.size() - 1; i >= 0; --i) {
					auto& p = projected[i];
					while (hull.size() >= t && cross(hull[hull.size() - 2], hull[hull.size() - 1], p) <= 0)
						hull.pop_back();
					hull.push_back(p);
				}

				hull.pop_back();
				return hull;
				};

			Clipper2Lib::Paths64 all_parts;
			for (auto& part_pair : entity.parts) {
				if (part_pair.first == "HumanoidRootPart") continue;

				rbx::part_t part = part_pair.second;
				if (!part.address) continue;
				auto hull = project_part(part, part_pair.first);
				if (hull.size() < 3) continue;

				Clipper2Lib::Path64 path;
				for (auto& pt : hull)
					path.push_back({ static_cast<int64_t>(pt.x * 1000.0), static_cast<int64_t>(pt.y * 1000.0) });
				all_parts.push_back(path);
			}

			if (!all_parts.empty()) {
				auto unified_solution = Clipper2Lib::Union(all_parts, Clipper2Lib::FillRule::NonZero);

				std::vector<std::vector<ImVec2>> all_polys;
				for (auto& sp : unified_solution) {
					std::vector<ImVec2> poly;
					for (auto& pt : sp) poly.push_back(ImVec2(pt.x / 1000.0f, pt.y / 1000.0f));
					if (poly.size() >= 3) all_polys.push_back(poly);
				}

				ImU32 fill_color = get_relation_color(entity.name, settings::visuals::highlights_color);

				for (auto& poly : all_polys) {
					draw->AddConcavePolyFilled(poly.data(), poly.size(), fill_color);
				}

				for (auto& poly : all_polys) {
					draw->AddPolyline(poly.data(), poly.size(), IM_COL32_WHITE, true, 1.0f);
				}
			}
		}

		// 8. Name ESP
		if (settings::visuals::name && Visualize.visitor)
		{
			std::string player_name = entity.display_name;
			
			ImFont* font = Visualize.visitor;
			float font_size = 9.0f;
			float char_spacing = 1.0f;
			
			float total_width = 0.0f;
			float max_height = 0.0f;
			for (size_t i = 0; i < player_name.length(); i++) {
				char c = player_name[i];
				char char_str[2] = { c, '\0' };
				ImVec2 char_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, char_str);
				total_width += char_size.x + (i < player_name.length() - 1 ? char_spacing : 0.0f);
				max_height = std::max(max_height, char_size.y);
			}
			
			float centerX = (boxPos.x + boxBR.x) * 0.5f;
			float textX = centerX - (total_width * 0.5f);
			float textY = boxPos.y - max_height - 2.0f;
			
			ImVec2 textPos = ImVec2(std::floor(textX + 0.5f), std::floor(textY + 0.5f));
			
			ImU32 nameColor = get_relation_color(entity.name, settings::visuals::name_color);
			Visualize.DrawTextWithSpacingAndOutline(draw, font, font_size, textPos, nameColor, IM_COL32(0, 0, 0, 255), player_name, char_spacing);
		}

		// 9. Distance ESP
		if (settings::visuals::distance && Visualize.visitor && has_local_hrp)
		{
			float dx = hrp_pos.x - local_hrp_pos.x;
			float dy = hrp_pos.y - local_hrp_pos.y;
			float dz = hrp_pos.z - local_hrp_pos.z;
			float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
			
			char distanceStr[32];
			snprintf(distanceStr, sizeof(distanceStr), "[%.1fM]", dist);
			
			ImFont* font = Visualize.visitor;
			float font_size = 9.0f;
			float char_spacing = 1.0f;
			
			float total_width = 0.0f;
			float max_height = 0.0f;
			for (size_t i = 0; i < strlen(distanceStr); i++) {
				char c = distanceStr[i];
				char char_str[2] = { c, '\0' };
				ImVec2 char_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, char_str);
				total_width += char_size.x + (i < strlen(distanceStr) - 1 ? char_spacing : 0.0f);
				max_height = std::max(max_height, char_size.y);
			}
			
			float centerX = (boxPos.x + boxBR.x) * 0.5f;
			float textX = centerX - (total_width * 0.5f);
			float textY = boxBR.y + 2.0f;
			
			ImVec2 textPos = ImVec2(std::floor(textX + 0.5f), std::floor(textY + 0.5f));
			
			ImU32 distColor = get_relation_color(entity.name, settings::visuals::distance_color);
			Visualize.DrawTextWithSpacingAndOutline(draw, font, font_size, textPos, distColor, IM_COL32(0, 0, 0, 255), std::string(distanceStr), char_spacing);
		}

		// 10. Equipped Weapon/Tool ESP
		if (settings::visuals::tool && Visualize.visitor)
		{
			std::string toolName = entity.tool_name;
			
			if (!toolName.empty())
			{
				ImFont* font = Visualize.visitor;
				float font_size = 9.0f;
				float char_spacing = 1.0f;
				
				float total_width = 0.0f;
				float max_height = 0.0f;
				for (size_t i = 0; i < toolName.length(); i++) {
					char c = toolName[i];
					char char_str[2] = { c, '\0' };
					ImVec2 char_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, char_str);
					total_width += char_size.x + (i < toolName.length() - 1 ? char_spacing : 0.0f);
					max_height = std::max(max_height, char_size.y);
				}
				
				float centerX = (boxPos.x + boxBR.x) * 0.5f;
				float textX = centerX - (total_width * 0.5f);
				float textY = boxBR.y + 2.0f;
				
				if (settings::visuals::distance)
					textY += 8.0f;
				
				ImVec2 textPos = ImVec2(std::floor(textX + 0.5f), std::floor(textY + 0.5f));
				
				ImU32 toolColor = get_relation_color(entity.name, settings::visuals::tool_color);
				Visualize.DrawTextWithSpacingAndOutline(draw, font, font_size, textPos, toolColor, IM_COL32(0, 0, 0, 255), toolName, char_spacing);
			}
		}

		// 11. Weapon Icon ESP
		if (settings::visuals::weapon_icon && Visualize.weapon_icon_font)
		{
			std::string toolName = entity.tool_name;
			
			if (!toolName.empty())
			{
				const char* weaponIcon = GetWeaponIcon(toolName);
				
				if (weaponIcon && strlen(weaponIcon) > 0)
				{
					float iconFontSize = 12.0f;
					ImVec2 iconTextSize = Visualize.weapon_icon_font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, weaponIcon);
					
					float centerX = (boxPos.x + boxBR.x) * 0.5f;
					float iconX = centerX - (iconTextSize.x * 0.5f);
					float iconY = boxBR.y + 2.0f;
					
					if (settings::visuals::distance)
						iconY += 8.0f;
					if (settings::visuals::tool)
						iconY += 8.0f;
					
					ImVec2 iconPos = ImVec2(std::floor(iconX + 0.5f), std::floor(iconY + 0.5f));
					
					ImU32 iconColor = get_relation_color(entity.name, settings::visuals::weapon_icon_color);
					
					draw->AddText(Visualize.weapon_icon_font, iconFontSize, ImVec2(iconPos.x + 1, iconPos.y + 1), IM_COL32(0, 0, 0, 255), weaponIcon);
					draw->AddText(Visualize.weapon_icon_font, iconFontSize, ImVec2(iconPos.x - 1, iconPos.y + 1), IM_COL32(0, 0, 0, 255), weaponIcon);
					draw->AddText(Visualize.weapon_icon_font, iconFontSize, ImVec2(iconPos.x + 1, iconPos.y - 1), IM_COL32(0, 0, 0, 255), weaponIcon);
					draw->AddText(Visualize.weapon_icon_font, iconFontSize, ImVec2(iconPos.x - 1, iconPos.y - 1), IM_COL32(0, 0, 0, 255), weaponIcon);
					
					draw->AddText(Visualize.weapon_icon_font, iconFontSize, iconPos, iconColor, weaponIcon);
				}
			}
		}

		// 12. Healthbar ESP
		if (settings::visuals::healthbar)
		{
			float health = entity.health;
			float max_health = entity.max_health;

			if (max_health > 0.0f)
			{
				constexpr float healthBarWidth = 1.0f;
				float boxHeight = c2.y;

				ImVec2 barPos = ImVec2(boxPos.x - 5.0f, boxPos.y - 1.0f);
				ImVec2 barSize = ImVec2(healthBarWidth, boxHeight + 2.0f);

				Visualize.draw_health_bar(draw, max_health, health, barPos, barSize, 1.0f, settings::visuals::health_text);
			}
		}
	}

}

void esp_render_t::draw_health_bar(ImDrawList* draw, float max, float current, ImVec2 pos, ImVec2 size, float alpha_factor, bool show_text)
{
	if (max <= 0.0f) max = 100.0f;
	
	float clamped_current = std::clamp(current, 0.0f, max);
	float health_percent = (max > 0.0f) ? (clamped_current / max) : 0.0f;
	health_percent = std::clamp(health_percent, 0.0f, 1.0f);
	
	float bar_width = 1.0f;
	float bar_height = size.y;
	float bar_x = std::round(pos.x);
	float bar_y = std::round(pos.y);

	ImDrawListFlags old_flags = draw->Flags;
	draw->Flags &= ~ImDrawListFlags_AntiAliasedLines;

	float outline_x1 = std::round(bar_x - 1);
	float outline_y1 = std::round(bar_y - 1);
	float outline_x2 = std::round(bar_x + bar_width + 1);
	float outline_y2 = std::round(bar_y + bar_height + 1);
	
	draw->AddRectFilled(
		ImVec2(outline_x1, outline_y1),
		ImVec2(outline_x2, outline_y2),
		IM_COL32(0, 0, 0, 255)
	);

	float fill_height = bar_height * health_percent;
	
	ImU32 health_color = IM_COL32(
		static_cast<int>(settings::visuals::healthbar_color[0] * 255.f),  
		static_cast<int>(settings::visuals::healthbar_color[1] * 255.f),          
		static_cast<int>(settings::visuals::healthbar_color[2] * 255.f),
		static_cast<int>(255 * alpha_factor)
	);

	float bg_x1 = std::round(bar_x);
	float bg_y1 = std::round(bar_y);
	float bg_x2 = std::round(bar_x + bar_width);
	float bg_y2 = std::round(bar_y + bar_height);
	
	draw->AddRectFilled(
		ImVec2(bg_x1, bg_y1),
		ImVec2(bg_x2, bg_y2),
		IM_COL32(50, 50, 50, 255)
	);
	
	if (fill_height > 0.1f) {
		float fill_start_y = bar_y + bar_height - fill_height;
		float fill_y1 = std::round(fill_start_y);
		float fill_y2 = std::round(bar_y + bar_height);
		
		draw->AddRectFilled(
			ImVec2(bg_x1, fill_y1),
			ImVec2(bg_x2, fill_y2),
			health_color
		);
	}
	
	draw->Flags = old_flags;

	if (show_text && settings::visuals::health_text && health_percent < 1.0f && visitor)
	{
		char buffer[32];
		sprintf_s(buffer, "%.0f", current);

		ImFont* font = visitor;
		float font_size = 9.0f;
		ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, buffer);

		float text_center_y = bar_y + (bar_height * 0.5f);
		
		ImVec2 text_pos = ImVec2(
			std::round(bar_x - (text_size.x / 2.0f) + (bar_width / 2.0f)),
			std::round(text_center_y - (text_size.y / 2.0f))
		);

		ImU32 text_color = IM_COL32(
			static_cast<int>(settings::visuals::health_text_color[0] * 255.f),
			static_cast<int>(settings::visuals::health_text_color[1] * 255.f),
			static_cast<int>(settings::visuals::health_text_color[2] * 255.f),
			255
		);

		draw->AddText(font, font_size, ImVec2(text_pos.x + 1, text_pos.y + 1), IM_COL32(0, 0, 0, 255), buffer);
		draw->AddText(font, font_size, ImVec2(text_pos.x - 1, text_pos.y + 1), IM_COL32(0, 0, 0, 255), buffer);
		draw->AddText(font, font_size, ImVec2(text_pos.x + 1, text_pos.y - 1), IM_COL32(0, 0, 0, 255), buffer);
		draw->AddText(font, font_size, ImVec2(text_pos.x - 1, text_pos.y - 1), IM_COL32(0, 0, 0, 255), buffer);
		
		draw->AddText(font, font_size, text_pos, text_color, buffer);
	}
}
