#include "dex_explorer.h"
#include <render/render.h>
#include <settings.h>
#include "../resources/iconsdex.h"
#include <d3d11.h>
#include <wincodec.h>
#include <memory>
#include <unordered_map>
#include <imgui/imgui_internal.h>

#pragma comment(lib, "windowscodecs.lib")

static bool styled_button_explorer(const char* label, const ImVec2& size = ImVec2(0, 0))
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;

	ImVec2 button_size = size;
	if (button_size.x == 0) button_size.x = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
	if (button_size.y == 0) button_size.y = ImGui::GetFrameHeight();

	ImVec2 pos = window->DC.CursorPos;
	pos.x += 2;

	ImRect bb(pos, ImVec2(pos.x + button_size.x, pos.y + button_size.y));
	ImGui::ItemSize(bb);
	if (!ImGui::ItemAdd(bb, window->GetID(label)))
		return false;

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(bb, window->GetID(label), &hovered, &held);

	static std::unordered_map<std::string, float> button_tween_progress;
	std::string button_id = label;

	if (button_tween_progress.find(button_id) == button_tween_progress.end())
		button_tween_progress[button_id] = 0.0f;

	float& tween_progress = button_tween_progress[button_id];

	ImVec4 base_color = ImVec4(60.0f / 255.0f, 60.0f / 255.0f, 60.0f / 255.0f, 1.0f);
	ImVec4 target_color = menu::accent_color;

	float target_progress = 0.0f;
	if (held)
		target_progress = 1.0f;
	else if (hovered)
		target_progress = 0.5f;

	float tween_speed = 4.0f;
	tween_progress += (target_progress - tween_progress) * g.IO.DeltaTime * tween_speed;

	ImVec4 tweened_color = ImVec4(
		base_color.x + (target_color.x - base_color.x) * tween_progress,
		base_color.y + (target_color.y - base_color.y) * tween_progress,
		base_color.z + (target_color.z - base_color.z) * tween_progress,
		base_color.w + (target_color.w - base_color.w) * tween_progress
	);

	ImDrawList* dl = ImGui::GetWindowDrawList();

	ImU32 bg_col = IM_COL32(35, 35, 35, 255);

	dl->AddRect(ImVec2(pos.x - 1, pos.y - 1), ImVec2(pos.x + button_size.x + 1, pos.y + button_size.y + 1), ImGui::ColorConvertFloat4ToU32(tweened_color), style.FrameRounding);
	dl->AddRect(ImVec2(pos.x - 2, pos.y - 2), ImVec2(pos.x + button_size.x + 2, pos.y + button_size.y + 2), IM_COL32(0, 0, 0, 255), style.FrameRounding);

	dl->AddRectFilled(bb.Min, bb.Max, bg_col, style.FrameRounding);

	ImVec2 text_size = ImGui::CalcTextSize(label);
	ImVec2 text_pos = ImVec2(bb.Min.x + (button_size.x - text_size.x) * 0.5f, bb.Min.y + (button_size.y - text_size.y) * 0.5f);

	for (int x = -1; x <= 1; x++) {
		for (int y = -1; y <= 1; y++) {
			if (x == 0 && y == 0) continue;
			dl->AddText(ImVec2(text_pos.x + x * 1.f, text_pos.y + y * 1.f), IM_COL32_BLACK, label);
		}
	}
	dl->AddText(text_pos, IM_COL32_WHITE, label);

	return pressed;
}

namespace dex_explorer
{
	bool DexExplorer::first_time = true;
	rbx::instance_t DexExplorer::selected_instance{};
	std::string DexExplorer::selected_instance_path = "";
	std::unordered_map<std::string, ImTextureID> DexExplorer::icon_textures{};
	std::unordered_map<std::uint64_t, bool> DexExplorer::expanded_nodes{};

	void DexExplorer::initialize()
	{
		if (first_time)
		{
			load_icons();
			first_time = false;
		}
	}

	ImTextureID DexExplorer::create_texture_from_data(const unsigned char* data, size_t size)
	{
		if (!::render || !::render->detail || !::render->detail->device || size == 0)
			return 0;

		static bool com_initialized = false;
		if (!com_initialized)
		{
			CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
			com_initialized = true;
		}

		IWICImagingFactory* pFactory = nullptr;
		HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
		if (FAILED(hr))
			return 0;

		IWICStream* pStream = nullptr;
		hr = pFactory->CreateStream(&pStream);
		if (FAILED(hr))
		{
			pFactory->Release();
			return 0;
		}

		hr = pStream->InitializeFromMemory((BYTE*)data, (DWORD)size);
		if (FAILED(hr))
		{
			pStream->Release();
			pFactory->Release();
			return 0;
		}

		IWICBitmapDecoder* pDecoder = nullptr;
		hr = pFactory->CreateDecoderFromStream(pStream, nullptr, WICDecodeMetadataCacheOnLoad, &pDecoder);
		if (FAILED(hr))
		{
			pStream->Release();
			pFactory->Release();
			return 0;
		}

		IWICBitmapFrameDecode* pFrame = nullptr;
		hr = pDecoder->GetFrame(0, &pFrame);
		if (FAILED(hr))
		{
			pDecoder->Release();
			pStream->Release();
			pFactory->Release();
			return 0;
		}

		IWICFormatConverter* pConverter = nullptr;
		hr = pFactory->CreateFormatConverter(&pConverter);
		if (FAILED(hr))
		{
			pFrame->Release();
			pDecoder->Release();
			pStream->Release();
			pFactory->Release();
			return 0;
		}

		hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
		if (FAILED(hr))
		{
			pConverter->Release();
			pFrame->Release();
			pDecoder->Release();
			pStream->Release();
			pFactory->Release();
			return 0;
		}

		UINT width = 0, height = 0;
		hr = pConverter->GetSize(&width, &height);
		if (FAILED(hr))
		{
			pConverter->Release();
			pFrame->Release();
			pDecoder->Release();
			pStream->Release();
			pFactory->Release();
			return 0;
		}

		UINT stride = width * 4;
		UINT bufferSize = stride * height;
		std::vector<BYTE> buffer(bufferSize);

		hr = pConverter->CopyPixels(nullptr, stride, bufferSize, buffer.data());
		if (FAILED(hr))
		{
			pConverter->Release();
			pFrame->Release();
			pDecoder->Release();
			pStream->Release();
			pFactory->Release();
			return 0;
		}

		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA subResource = {};
		subResource.pSysMem = buffer.data();
		subResource.SysMemPitch = stride;
		subResource.SysMemSlicePitch = 0;

		ID3D11Texture2D* pTexture = nullptr;
		hr = ::render->detail->device->CreateTexture2D(&desc, &subResource, &pTexture);
		if (FAILED(hr))
		{
			pConverter->Release();
			pFrame->Release();
			pDecoder->Release();
			pStream->Release();
			pFactory->Release();
			return 0;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = desc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;

		ID3D11ShaderResourceView* pSRV = nullptr;
		hr = ::render->detail->device->CreateShaderResourceView(pTexture, &srvDesc, &pSRV);
		pTexture->Release();

		pConverter->Release();
		pFrame->Release();
		pDecoder->Release();
		pStream->Release();
		pFactory->Release();

		if (FAILED(hr))
			return 0;

		return (ImTextureID)pSRV;
	}

	void DexExplorer::load_icons()
	{
		icon_textures["Workspace"] = create_texture_from_data(workspace, sizeof(workspace));
		icon_textures["Folder"] = create_texture_from_data(folder, sizeof(folder));
		icon_textures["Camera"] = create_texture_from_data(camera, sizeof(camera));
		icon_textures["Lightning"] = create_texture_from_data(lightning, sizeof(lightning));
		icon_textures["Humanoid"] = create_texture_from_data(humanoid, sizeof(humanoid));
		icon_textures["Part"] = create_texture_from_data(part, sizeof(part));
		icon_textures["Players"] = create_texture_from_data(players, sizeof(players));
		icon_textures["MeshPart"] = create_texture_from_data(meshpart, sizeof(meshpart));
		icon_textures["Player"] = create_texture_from_data(player, sizeof(player));
		icon_textures["Model"] = create_texture_from_data(model, sizeof(model));
		icon_textures["Terrain"] = create_texture_from_data(terrain, sizeof(terrain));
		icon_textures["LocalScript"] = create_texture_from_data(localscript, sizeof(localscript));
		icon_textures["LocalScripts"] = create_texture_from_data(localscripts, sizeof(localscripts));
		icon_textures["PlayerGui"] = create_texture_from_data(playergui, sizeof(playergui));
		icon_textures["Stats"] = create_texture_from_data(stats, sizeof(stats));
		icon_textures["GuiService"] = create_texture_from_data(guiservice, sizeof(guiservice));
		icon_textures["VideoCaptureService"] = create_texture_from_data(videocapture, sizeof(videocapture));
		icon_textures["RunService"] = create_texture_from_data(runservice, sizeof(runservice));
		icon_textures["Frame"] = create_texture_from_data(frame, sizeof(frame));
		icon_textures["CSGDictionaryService"] = create_texture_from_data(csd, sizeof(csd));
		icon_textures["ContentProvider"] = create_texture_from_data(contentprovider, sizeof(contentprovider));
		icon_textures["NonReplicatedStorage"] = create_texture_from_data(nonreplicated, sizeof(nonreplicated));
		icon_textures["StarterGear"] = create_texture_from_data(startergear, sizeof(startergear));
		icon_textures["TimerDevice"] = create_texture_from_data(timerdevice, sizeof(timerdevice));
		icon_textures["Backpack"] = create_texture_from_data(backpack, sizeof(backpack));
		icon_textures["MarketplaceService"] = create_texture_from_data(marketplaceservice, sizeof(marketplaceservice));
		icon_textures["SoundService"] = create_texture_from_data(soundservice, sizeof(soundservice));
		icon_textures["LogService"] = create_texture_from_data(logservice, sizeof(logservice));
		icon_textures["StatsItem"] = create_texture_from_data(statsitem, sizeof(statsitem));
		icon_textures["BoolValue"] = create_texture_from_data(boolvalue, sizeof(boolvalue));
		icon_textures["IntValue"] = create_texture_from_data(intvalue, sizeof(intvalue));
		icon_textures["NumberValue"] = create_texture_from_data(doubletype, sizeof(doubletype));
	}

	ImTextureID DexExplorer::get_icon_for_class(const std::string& class_name)
	{
		auto it = icon_textures.find(class_name);
		if (it != icon_textures.end())
			return it->second;
		
		auto folder_it = icon_textures.find("Folder");
		if (folder_it != icon_textures.end())
			return folder_it->second;
		
		return 0;
	}

	void DexExplorer::recursive_draw(rbx::instance_t instance, const std::string& parent_path, const std::string& display_path)
	{
		if (instance.address == 0)
			return;

		std::string name = instance.get_name();
		std::string class_name = instance.get_class_name();
		std::string full_path = parent_path.empty() ? name : parent_path + "." + name;
		std::string display_name = name;
		
		if (!class_name.empty() && class_name != "unknown")
		{
			display_name += " [" + class_name + "]";
		}

		ImGui::PushID((int)instance.address);
		
		std::string context_menu_id = "ContextMenu_" + std::to_string(instance.address);

		bool has_children = false;
		try
		{
			auto children = instance.get_children();
			has_children = !children.empty();
		}
		catch (...)
		{
			has_children = false;
		}

		bool is_selected = (selected_instance.address == instance.address);

		ImTextureID icon = get_icon_for_class(class_name);
		if (icon)
		{
			ImGui::Image(icon, ImVec2(16, 16));
			ImGui::SameLine(0.0f, 4.0f);
		}

		if (!has_children)
		{
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			
			if (is_selected)
			{
				flags |= ImGuiTreeNodeFlags_Selected;
				ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(menu::accent_color.x * 0.3f, menu::accent_color.y * 0.3f, menu::accent_color.z * 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(menu::accent_color.x * 0.4f, menu::accent_color.y * 0.4f, menu::accent_color.z * 0.4f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(menu::accent_color.x * 0.5f, menu::accent_color.y * 0.5f, menu::accent_color.z * 0.5f, 1.0f));
			}
			
			ImGui::TreeNodeEx(display_name.c_str(), flags);
			
			if (ImGui::IsItemClicked())
			{
				selected_instance = instance;
				selected_instance_path = full_path;
			}
			
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
			{
				selected_instance = instance;
				selected_instance_path = full_path;
				ImGui::OpenPopup(context_menu_id.c_str());
			}
			
			if (is_selected)
			{
				ImGui::PopStyleColor(3);
			}
			
			if (ImGui::BeginPopup(context_menu_id.c_str()))
			{
				if (ImGui::MenuItem("Copy Address"))
				{
					char buf[32];
					sprintf_s(buf, "0x%llX", static_cast<unsigned long long>(instance.address));
					ImGui::SetClipboardText(buf);
				}
				if (ImGui::MenuItem("Copy Path"))
				{
					if (!full_path.empty())
					{
						ImGui::SetClipboardText(full_path.c_str());
					}
				}
				ImGui::EndPopup();
			}
		}
		else
		{
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
			
			bool is_expanded = expanded_nodes[instance.address];
			if (is_expanded)
				flags |= ImGuiTreeNodeFlags_DefaultOpen;

			if (is_selected)
			{
				flags |= ImGuiTreeNodeFlags_Selected;
				ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(menu::accent_color.x * 0.3f, menu::accent_color.y * 0.3f, menu::accent_color.z * 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(menu::accent_color.x * 0.4f, menu::accent_color.y * 0.4f, menu::accent_color.z * 0.4f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(menu::accent_color.x * 0.5f, menu::accent_color.y * 0.5f, menu::accent_color.z * 0.5f, 1.0f));
			}

			bool open = ImGui::TreeNodeEx(display_name.c_str(), flags);
			
			if (ImGui::IsItemClicked())
			{
				selected_instance = instance;
				selected_instance_path = full_path;
			}
			
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
			{
				selected_instance = instance;
				selected_instance_path = full_path;
				ImGui::OpenPopup(context_menu_id.c_str());
			}

			if (is_selected)
			{
				ImGui::PopStyleColor(3);
			}
			
			if (ImGui::BeginPopup(context_menu_id.c_str()))
			{
				if (ImGui::MenuItem("Copy Address"))
				{
					char buf[32];
					sprintf_s(buf, "0x%llX", static_cast<unsigned long long>(instance.address));
					ImGui::SetClipboardText(buf);
				}
				if (ImGui::MenuItem("Copy Path"))
				{
					if (!full_path.empty())
					{
						ImGui::SetClipboardText(full_path.c_str());
					}
				}
				ImGui::EndPopup();
			}

			if (open)
			{
				expanded_nodes[instance.address] = true;
				try
				{
					auto children = instance.get_children();
					for (auto& child : children)
					{
						bool child_has_children = false;
						try
						{
							auto child_children = child.get_children();
							child_has_children = !child_children.empty();
						}
						catch (...)
						{
							child_has_children = false;
						}
						
						if (child_has_children)
						{
							recursive_draw(child, full_path, display_path);
						}
					}
				}
				catch (...)
				{
				}
				ImGui::TreePop();
			}
			else
			{
				expanded_nodes[instance.address] = false;
			}
		}

		ImGui::PopID();
	}

	void DexExplorer::render()
	{
		if (!settings::dex_explorer::enabled)
			return;

		initialize();

		static bool first_open = true;
		static bool window_open = true;
		static bool was_enabled = false;

		if (settings::dex_explorer::enabled && !was_enabled)
		{
			window_open = true;
			first_open = true;
		}
		was_enabled = settings::dex_explorer::enabled;

		if (first_open && window_open)
		{
			ImVec2 main_viewport_size = ImGui::GetMainViewport()->Size;
			ImVec2 explorer_size = ImVec2(500, 400);
			float pos_x = (main_viewport_size.x - explorer_size.x) * 0.5f;
			float pos_y = (main_viewport_size.y - explorer_size.y) * 0.5f;
			if (pos_x < 10.0f) pos_x = 10.0f;
			if (pos_y < 10.0f) pos_y = 10.0f;
			ImVec2 explorer_pos = ImVec2(pos_x, pos_y);
			ImGui::SetNextWindowPos(explorer_pos, ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(explorer_size, ImGuiCond_FirstUseEver);
			first_open = false;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.137f, 0.137f, 0.137f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.137f, 0.137f, 0.137f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(menu::accent_color.x * 0.5f, menu::accent_color.y * 0.5f, menu::accent_color.z * 0.5f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 6));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);

		if (ImGui::Begin("DEX Explorer", &window_open, ImGuiWindowFlags_NoTitleBar))
		{
			ImVec2 window_pos = ImGui::GetWindowPos();
			ImVec2 window_size = ImGui::GetWindowSize();
			ImDrawList* draw_list = ImGui::GetWindowDrawList();

			draw_list->AddRect(ImVec2(window_pos.x + 1, window_pos.y + 1), ImVec2(window_pos.x + window_size.x - 1, window_pos.y + window_size.y - 1), IM_COL32(60, 60, 60, 255), 0, 1.0f);
			draw_list->AddRect(ImVec2(window_pos.x + 2, window_pos.y + 2), ImVec2(window_pos.x + window_size.x - 2, window_pos.y + window_size.y - 2), IM_COL32(0, 0, 0, 255), 0, 1.0f);
			draw_list->AddRectFilled(ImVec2(window_pos.x + 5, window_pos.y + 5), ImVec2(window_pos.x + window_size.x - 5, window_pos.y + window_size.y - 5), IM_COL32(20, 20, 20, 255));
			draw_list->AddRect(ImVec2(window_pos.x + 7, window_pos.y + 7), ImVec2(window_pos.x + window_size.x - 7, window_pos.y + window_size.y - 7), IM_COL32(0, 0, 0, 255));
			draw_list->AddRect(ImVec2(window_pos.x + 6, window_pos.y + 6), ImVec2(window_pos.x + window_size.x - 6, window_pos.y + window_size.y - 6), IM_COL32(60, 60, 60, 255));

			float total_w = ImGui::GetContentRegionAvail().x;
			float total_h = ImGui::GetContentRegionAvail().y;

			ImGui::BeginChild("##ExplorerTree", ImVec2(total_w, total_h), true, ImGuiWindowFlags_HorizontalScrollbar);
			{

				if (game::datamodel.address != 0)
				{
					std::string dm_name = game::datamodel.get_name();
					std::string dm_class = game::datamodel.get_class_name();
					std::string dm_display = dm_name;
					if (!dm_class.empty() && dm_class != "unknown")
					{
						dm_display += " [" + dm_class + "]";
					}

					ImGui::PushID((int)game::datamodel.address);
					
					bool has_children = false;
					try
					{
						auto children = game::datamodel.get_children();
						has_children = !children.empty();
					}
					catch (...)
					{
						has_children = false;
					}

					bool is_selected = (selected_instance.address == game::datamodel.address);

					ImTextureID icon = get_icon_for_class(dm_class);
					if (icon)
					{
						ImGui::Image(icon, ImVec2(16, 16));
						ImGui::SameLine(0.0f, 4.0f);
					}

					if (!has_children)
					{
						ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
						
						if (is_selected)
						{
							flags |= ImGuiTreeNodeFlags_Selected;
							ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(menu::accent_color.x * 0.3f, menu::accent_color.y * 0.3f, menu::accent_color.z * 0.3f, 1.0f));
							ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(menu::accent_color.x * 0.4f, menu::accent_color.y * 0.4f, menu::accent_color.z * 0.4f, 1.0f));
							ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(menu::accent_color.x * 0.5f, menu::accent_color.y * 0.5f, menu::accent_color.z * 0.5f, 1.0f));
						}
						
						ImGui::TreeNodeEx(dm_display.c_str(), flags);
						
						if (ImGui::IsItemClicked())
						{
							selected_instance = game::datamodel;
							selected_instance_path = dm_name;
						}
						
						if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
						{
							selected_instance = game::datamodel;
							selected_instance_path = dm_name;
							ImGui::OpenPopup("ContextMenuDM");
						}
						
						if (is_selected)
						{
							ImGui::PopStyleColor(3);
						}
						
						
						if (ImGui::BeginPopup("ContextMenuDM"))
						{
							if (ImGui::MenuItem("Copy Address"))
							{
								char buf[32];
								sprintf_s(buf, "0x%llX", static_cast<unsigned long long>(game::datamodel.address));
								ImGui::SetClipboardText(buf);
							}
							if (ImGui::MenuItem("Copy Path"))
							{
								if (!dm_name.empty())
								{
									ImGui::SetClipboardText(dm_name.c_str());
								}
							}
							ImGui::EndPopup();
						}
					}
					else
					{
						ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
						
						if (is_selected)
						{
							flags |= ImGuiTreeNodeFlags_Selected;
							ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(menu::accent_color.x * 0.3f, menu::accent_color.y * 0.3f, menu::accent_color.z * 0.3f, 1.0f));
							ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(menu::accent_color.x * 0.4f, menu::accent_color.y * 0.4f, menu::accent_color.z * 0.4f, 1.0f));
							ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(menu::accent_color.x * 0.5f, menu::accent_color.y * 0.5f, menu::accent_color.z * 0.5f, 1.0f));
						}

						bool open = ImGui::TreeNodeEx(dm_display.c_str(), flags);
						if (ImGui::IsItemClicked())
						{
							selected_instance = game::datamodel;
							selected_instance_path = dm_name;
						}
						
						if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
						{
							selected_instance = game::datamodel;
							selected_instance_path = dm_name;
							ImGui::OpenPopup("ContextMenuDM");
						}
						
						if (is_selected)
						{
							ImGui::PopStyleColor(3);
						}
						
						
						if (ImGui::BeginPopup("ContextMenuDM"))
						{
							if (ImGui::MenuItem("Copy Address"))
							{
								char buf[32];
								sprintf_s(buf, "0x%llX", static_cast<unsigned long long>(game::datamodel.address));
								ImGui::SetClipboardText(buf);
							}
							if (ImGui::MenuItem("Copy Path"))
							{
								if (!dm_name.empty())
								{
									ImGui::SetClipboardText(dm_name.c_str());
								}
							}
							ImGui::EndPopup();
						}
						if (open)
						{
							try
							{
								auto children = game::datamodel.get_children();
								std::string root_path = dm_name;
								for (auto& child : children)
								{
									bool child_has_children = false;
									try
									{
										auto child_children = child.get_children();
										child_has_children = !child_children.empty();
									}
									catch (...)
									{
										child_has_children = false;
									}
									
									if (child_has_children)
									{
										recursive_draw(child, root_path, root_path);
									}
								}
							}
							catch (...)
							{
								
							}
							ImGui::TreePop();
						}
					}
					ImGui::PopID();
				}
				else
				{
					ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Datamodel not found!");
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();

		ImGui::PopStyleVar(7);
		ImGui::PopStyleColor(7);

		if (!window_open)
		{
			settings::dex_explorer::enabled = false;
			first_open = true;
		}
	}
}

