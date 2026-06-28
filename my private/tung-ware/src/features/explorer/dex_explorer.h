#pragma once
#include <sdk/sdk.h>
#include <game/game.h>
#include <imgui/imgui.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace dex_explorer
{
	class DexExplorer
	{
	public:
		static void render();
		static void initialize();

	private:
		static bool first_time;
		static rbx::instance_t selected_instance;
		static std::string selected_instance_path;
		static std::unordered_map<std::string, ImTextureID> icon_textures;
		static std::unordered_map<std::uint64_t, bool> expanded_nodes;

		static void recursive_draw(rbx::instance_t instance, const std::string& parent_path, const std::string& display_path);
		static ImTextureID get_icon_for_class(const std::string& class_name);
		static void load_icons();
		static ImTextureID create_texture_from_data(const unsigned char* data, size_t size);
	};
}

