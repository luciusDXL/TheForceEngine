#include "browser.h"
#include "entity.h"
#include "error.h"
#include "levelEditor.h"
#include "infoPanel.h"
#include "sharedState.h"
#include <TFE_Editor/editor.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/EditorAsset/editorFrame.h>
#include <TFE_Editor/EditorAsset/editorSprite.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <TFE_System/math.h>
#include <TFE_Input/input.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;

namespace LevelEditor
{
	enum BrowseSort : s32
	{
		BSORT_NAME = 0,
		BSORT_USED,
		BSORD_COUNT
	};
	const char* c_browseSortItems[BSORD_COUNT] =
	{
		"Name",		// BSORT_NAME
		"Used",     // BSORT_USED
	};

	typedef std::vector<Asset*> FilteredAssetList;

	static char s_filter[32] = "";
	static BrowseSort s_browseSort = BSORT_NAME;
	static FilteredAssetList s_filteredList;
	static s32 s_focusOnRow = -1;

	void browseTextures();
	void browseEntities();
	
	void browserBegin(s32 offset)
	{
		bool browser = true;

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImGui::SetWindowPos("Browser", { (f32)displayInfo.width - 480.0f, 22.0f + f32(offset) });
		ImGui::SetWindowSize("Browser", { 480.0f, (f32)displayInfo.height - f32(offset + 22) });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("Browser", &browser, window_flags);
	}

	void browserEnd()
	{
		ImGui::End();
	}

	void drawBrowser(BrowseMode mode)
	{
		browserBegin(infoPanelGetHeight());
		switch (mode)
		{
			case BROWSE_TEXTURE:
				browseTextures();
				break;
			case BROWSE_ENTITY:
				browseEntities();
				break;
		}
		browserEnd();
	}
	
	
	// Returns true if it passes the filter.
	bool browserFilter(const char* name)
	{
		// No filter applied.
		size_t filterLen = strlen(s_filter);
		if (s_filter[0] == 0 || filterLen == 0) { return true; }

		// Make sure the name is at least as long as the filter.
		const size_t nameLength = strlen(name);
		if (nameLength < filterLen) { return false; }

		// See if there is a match...
		const size_t validLength = nameLength - filterLen + 1;
		for (size_t i = 0; i < validLength; i++)
		{
			if (editorStringFilter(&name[i], s_filter, filterLen))
			{
				return true;
			}
		}
		return false;
	}
			
	void filterTextures()
	{
		// Textures must be a power of two.
		const s32 count = (s32)s_levelTextureList.size();
		Asset* asset = s_levelTextureList.data();
		s_filteredList.clear();

		for (s32 i = 0; i < count; i++, asset++)
		{
			EditorTexture* texture = (EditorTexture*)getAssetData(asset->handle);
			if (!texture) { continue; }
			if (!TFE_Math::isPow2(texture->width) || !TFE_Math::isPow2(texture->height)) { continue; }

			if (browserFilter(asset->name.c_str()))
			{
				s_filteredList.push_back(asset);
			}
		}
	}

	s32 getFilteredIndex(s32 unfilteredIndex)
	{
		if (unfilteredIndex < 0) { return -1; }
		const char* srcName = s_levelTextureList[unfilteredIndex].name.c_str();

		const s32 count = (s32)s_filteredList.size();
		Asset** assetList = s_filteredList.data();
		for (s32 i = 0; i < count; i++)
		{
			const Asset* asset = assetList[i];
			if (strcasecmp(asset->name.c_str(), srcName) == 0)
			{
				return i;
			}
		}
		return -1;
	}

	s32 getUnfilteredIndex(s32 filteredIndex)
	{
		if (filteredIndex < 0) { return -1; }
		const char* srcName = s_filteredList[filteredIndex]->name.c_str();

		const s32 count = (s32)s_levelTextureList.size();
		const Asset* asset = s_levelTextureList.data();
		for (s32 i = 0; i < count; i++, asset++)
		{
			if (strcasecmp(asset->name.c_str(), srcName) == 0)
			{
				return i;
			}
		}
		return -1;
	}

	void browserScrollToSelection()
	{
		const s32 index = getFilteredIndex(s_selectedTexture);
		if (index >= 0)
		{
			s_focusOnRow = index / 6;
		}
	}

	void browseTextures()
	{
		ImVec4 bgColor       = { 0.0f, 0.0f, 0.0f, 0.0f };
		ImVec4 tintColorNrml = { 1.0f, 1.0f, 1.0f, 1.0f };
		ImVec4 tintColorSel  = { 1.5f, 1.5f, 1.5f, 1.0f };

		bool filterChanged = false;

		if (ImGui::Button("Edit List"))
		{
			// TODO: Popup.
		}
		setTooltip("Edit which textures are displayed based on groups and tags");

		ImGui::SameLine();
		ImGui::SetNextItemWidth(72.0f);
		if (ImGui::Combo("##BrowserSort", (s32*)&s_browseSort, c_browseSortItems, BSORD_COUNT))
		{
			filterChanged = true;
		}
		setTooltip("Sort method, changes the order that items are listed");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(256.0f);

		if (ImGui::InputText("##BrowserFilter", s_filter, 24))
		{
			filterChanged = true;
		}
		setTooltip("Filter textures by string");
		ImGui::SameLine();
		if (ImGui::Button("X##BrowserFilterClear"))
		{
			s_filter[0] = 0;
			filterChanged = true;
		}
		setTooltip("Clear the filter string");

		if (filterChanged || s_filteredList.empty())
		{
			filterChanged = true;
			filterTextures();
		}

		// Get the count after the filtering is done.
		u32 count = (u32)s_filteredList.size();
		u32 rows = (count + 5) / 6;
		f32 y = 0.0f;
		s32 selectedIndex = getFilteredIndex(s_selectedTexture);

		ImGui::BeginChild("##TextureList");
		{
			// Scroll to selected texture.
			if (s_focusOnRow >= 0)
			{
				ImGui::SetScrollY(f32(s_focusOnRow * (64 + 8) - 128));
				s_focusOnRow = -1;
			}
			else if (filterChanged)
			{
				ImGui::SetScrollY(0.0f);
			}

			for (u32 i = 0; i < rows; i++)
			{
				s32 startIndex = i * 6;
				for (u32 t = 0; t < 6 && startIndex + t < count; t++)
				{
					s32 index = startIndex + t;
					EditorTexture* texture = (EditorTexture*)getAssetData(s_filteredList[index]->handle);
					if (!texture) { continue; }

					void* ptr = TFE_RenderBackend::getGpuPtr(texture->frames[0]);
					u32 w = 64, h = 64;
					if (texture->width > texture->height)
					{
						w = 64;
						h = 64 * texture->height / texture->width;
					}
					else if (texture->width < texture->height)
					{
						h = 64;
						w = 64 * texture->width / texture->height;
					}

					bool isSelected = selectedIndex == index;
					if (isSelected)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, { 1.0f, 1.0f, 0.3f, 1.0f });
					}

					if (ImGui::ImageButton(ptr, ImVec2(f32(w), f32(h)), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f), 2))
					{
						s_selectedTexture = getUnfilteredIndex(index);
					}
					setTooltip("%s %dx%d", texture->name, texture->width, texture->height);

					if (isSelected)
					{
						ImGui::PopStyleColor();
					}

					ImGui::SameLine();
				}
				ImGui::NewLine();
			}
		}
		ImGui::EndChild();
	}

	void browseEntities()
	{
		// Do sort stuff later....

		const s32 count = (s32)s_entityList.size();
		const u32 padding = 8;
		s32 selectedIndex = s_selectedTexture;

		ImGui::BeginChild("##EntityList");
		{
			const u32 idBase = 0x100100;
			for (s32 i = 0, x = 0; i < count; i++)
			{
				s32 index = i;
				Entity* entity = &s_entityList[index];

				f32 du = fabsf(entity->st[1].x - entity->st[0].x);
				f32 dv = fabsf(entity->st[1].z - entity->st[0].z);
				void* ptr = TFE_RenderBackend::getGpuPtr(entity->image);

				u32 w = 64, h = 64;
				if (du > dv)
				{
					w = 64;
					h = u32(64.0f * dv / du);
				}
				else if (du < dv)
				{
					h = 64;
					w = u32(64.0f * du / dv);
				}

				if (x + w + padding >= 400)
				{
					ImGui::NewLine();
					x = 0;
				}
				x += w + padding;

				bool isSelected = selectedIndex == index;
				if (isSelected)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, { 1.0f, 1.0f, 0.3f, 1.0f });
				}

				ImGui::PushID(idBase + i);
				if (ImGui::ImageButton(ptr, ImVec2(f32(w), f32(h)), ImVec2(entity->uv[0].x, entity->uv[0].z), ImVec2(entity->uv[1].x, entity->uv[1].z), 2))
				{
					s_selectedTexture = index;
				}
				ImGui::PopID();
				setTooltip("%s", entity->name.c_str());

				if (isSelected)
				{
					ImGui::PopStyleColor();
				}
				ImGui::SameLine();
			}
		}
		ImGui::EndChild();
	}
}
