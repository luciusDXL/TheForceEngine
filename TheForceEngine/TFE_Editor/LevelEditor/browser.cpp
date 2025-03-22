#include "browser.h"
#include "entity.h"
#include "error.h"
#include "levelEditor.h"
#include "infoPanel.h"
#include "sharedState.h"
#include <TFE_Editor/editor.h>
#include <TFE_Editor/editorLevel.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/EditorAsset/editorFrame.h>
#include <TFE_Editor/EditorAsset/editorSprite.h>
#include <TFE_Editor/EditorAsset/editor3dThumbnails.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <TFE_System/math.h>
#include <TFE_Input/input.h>
#include <TFE_Ui/ui.h>

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

	struct FilteredEntity
	{
		Entity* entity;
		s32 index;
	};

	struct UsedIndex
	{
		s32 id;
		s32 count;
	};

	typedef std::vector<Asset*> FilteredAssetList;
	typedef std::vector<FilteredEntity> FilteredEntityList;
	typedef std::vector<UsedIndex> UsedCountList;

	static char s_filter[32] = "";
	static BrowseSort s_browseSort = BSORT_NAME;
	static FilteredAssetList s_filteredList;
	static FilteredAssetList s_filteredListCopy;
	static FilteredEntityList s_filteredEntityList;
	static UsedCountList s_usedCountList;
	static s32 s_focusOnRow = -1;
	static s32 s_entityCategory = -1;
	static TextureGpu* s_icon3d = nullptr;
	static u32 s_textureSourceFlags = 0xffffffffu;
	static u32 s_nextTextureSourceFlags = 0xffffffffu;

	void browseTextures();
	void browseEntities();
	s32 getFilteredIndex(s32 unfilteredIndex);
	s32 getUnfilteredIndex(s32 filteredIndex);
	s32 getLevelTextureIndex(s32 id);
		
	void browserLoadIcons()
	{
		// Load as a PNG.
		s_icon3d = loadGpuImage("UI_Images/Obj-3d-Icon.png");
		s_textureSourceFlags = 0xffffffffu;
	}

	void browserFreeIcons()
	{
		// All icons are freed later, so just leave it alone.
	}
	
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

	bool textureSourcesUI()
	{
		pushFont(FONT_SMALL);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;
		bool exit = false;
		if (ImGui::BeginPopupModal("Texture Sources", nullptr, window_flags))
		{
			// Slots.
			const s32 levelCount = TFE_Editor::level_getDarkForcesSlotCount();
			const s32 rowCount = 3;
			const s32 yCount = (levelCount + 2) / rowCount;
			for (s32 y = 0, idx = 0; y < yCount; y++)
			{
				for (s32 i = 0; i < rowCount && idx < levelCount; i++, idx++)
				{
					const char* name = TFE_Editor::level_getDarkForcesSlotName(idx);
					ImGui::CheckboxFlags(name, &s_nextTextureSourceFlags, (1 << idx));

					if (i < rowCount - 1 && idx < levelCount - 1)
					{
						ImGui::SameLine(128.0f * (i + 1));
					}
				}
			}
			// External Resources.
			ImGui::CheckboxFlags("Resources", &s_nextTextureSourceFlags, (1 << levelCount));

			ImGui::Separator();
			if (ImGui::Button("Select All"))
			{
				s_nextTextureSourceFlags = 0xffffffffu;
			}
			ImGui::SameLine();
			if (ImGui::Button("Select None"))
			{
				s_nextTextureSourceFlags = 0x0u;
			}

			ImGui::Separator();
			if (ImGui::Button("Exit"))
			{
				exit = true;
			}
			ImGui::EndPopup();
		}
		popFont();

		return exit;
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

	bool sortByCount(UsedIndex& a, UsedIndex& b)
	{
		return a.count > b.count;
	}
					
	void filterTextures()
	{
		// Textures must be a power of two.
		const s32 count = (s32)s_levelTextureList.size();
		Asset* asset = s_levelTextureList.data();
		s_filteredList.clear();

		u32 levCount = level_getDarkForcesSlotCount();
		u32 extFlag = 1 << levCount;

		for (s32 i = 0; i < count; i++, asset++)
		{
			EditorTexture* texture = (EditorTexture*)getAssetData(asset->handle);
			if (!texture) { continue; }
			if (!TFE_Math::isPow2(texture->width) || !TFE_Math::isPow2(texture->height)) { continue; }

			// Sources.
			bool isValidSource = true;
			if (s_textureSourceFlags != 0xffffffffu)
			{
				if (!(s_textureSourceFlags & extFlag) && asset->assetSource != ASRC_VANILLA) { isValidSource = false; }
				if (asset->assetSource == ASRC_VANILLA)
				{
					bool anyLevel = false;
					for (u32 i = 0; i < levCount && isValidSource; i++)
					{
						if ((s_textureSourceFlags & (1 << i)) && AssetBrowser::isAssetInLevel(asset->name.c_str(), i))
						{
							anyLevel = true;
							break;
						}
					}
					if (!anyLevel) { isValidSource = false; }
				}
			}
			if (isValidSource && browserFilter(asset->name.c_str()))
			{
				s_filteredList.push_back(asset);
			}
		}

		// Sort by most used to least used.
		if (s_browseSort == BSORT_USED)
		{
			const s32 listCount = (s32)s_filteredList.size();
			s_usedCountList.resize(listCount);
			UsedIndex* usedCount = s_usedCountList.data();
			for (s32 i = 0; i < listCount; i++)
			{
				usedCount[i].id = i;
				usedCount[i].count = 0;
			}

			const s32 sectorCount = (s32)s_level.sectors.size();
			const EditorSector* sector = s_level.sectors.data();
			for (s32 s = 0; s < sectorCount; s++, sector++)
			{
				const s32 floorIndex = getLevelTextureIndex(sector->floorTex.texIndex);
				const s32 ceilIndex = getLevelTextureIndex(sector->ceilTex.texIndex);
				if (floorIndex >= 0)
				{
					usedCount[floorIndex].count++;
				}
				if (ceilIndex >= 0)
				{
					usedCount[ceilIndex].count++;
				}

				const s32 wallCount = (s32)sector->walls.size();
				const EditorWall* wall = sector->walls.data();
				for (s32 w = 0; w < wallCount; w++, wall++)
				{
					for (s32 t = 0; t < WP_COUNT; t++)
					{
						const s32 index = getLevelTextureIndex(wall->tex[t].texIndex);
						if (index >= 0)
						{
							usedCount[index].count++;
						}
					}
				}
			}
			std::sort(s_usedCountList.begin(), s_usedCountList.end(), sortByCount);

			// Now re-order the filtered list.
			s_filteredListCopy = s_filteredList;
			Asset** dstList = s_filteredList.data();
			Asset** srcList = s_filteredListCopy.data();
			for (s32 i = 0; i < listCount; i++)
			{
				dstList[i] = srcList[s_usedCountList[i].id];
			}
		}
	}

	s32 getLevelTextureIndex(s32 id)
	{
		if (id < 0) { return -1; }
		const char* srcName = s_level.textures[id].name.c_str();

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

		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);

		bool filterChanged = false;
		bool viewportHovered = !TFE_Input::relativeModeEnabled() && edit_viewportHovered(mx, my);
		if (isTextureAssignDirty() && s_browseSort == BSORT_USED && !viewportHovered)
		{
			filterChanged = true;
		}
		if (s_nextTextureSourceFlags != s_textureSourceFlags && !isPopupOpen())
		{
			filterChanged = true;
			s_textureSourceFlags = s_nextTextureSourceFlags;
		}
				
		if (ImGui::Button("Texture Sources"))
		{
			s_nextTextureSourceFlags = s_textureSourceFlags;
			openEditorPopup(POPUP_TEX_SOURCES);
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
		ImGui::SetNextItemWidth(196.0f);

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
			setTextureAssignDirty(false);
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

	void updateEntityFilter()
	{
		s_filteredEntityList.clear();
		const u32 flag = s_entityCategory < 0 ? 0xffffffffu : 1u << u32(s_entityCategory);
		const s32 count = (s32)s_entityDefList.size();
		Entity* entity = s_entityDefList.data();
		for (s32 i = 0; i < count; i++, entity++)
		{
			if (flag == 0xffffffffu || (entity->categories & flag))
			{
				s_filteredEntityList.push_back({entity, i});
			}
		}
	}

	void browseEntities()
	{
		// Do sort stuff later....
		ImGui::Text("Category: ");
		ImGui::SameLine(0.0f, 8.0f);
		ImGui::SetNextItemWidth(128.0f);
		s32 prevCategory = s_entityCategory;
		if (ImGui::BeginCombo("##EntityCategory", s_entityCategory < 0 ? "All" : s_categoryList[s_entityCategory].name.c_str()))
		{
			s32 count = (s32)s_categoryList.size() + 1;
			for (s32 i = 0; i < count; i++)
			{
				const char* name = "All";
				if (i > 0)
				{
					name = s_categoryList[i - 1].name.c_str();
				}

				if (ImGui::Selectable(name, i-1 == s_entityCategory))
				{
					s_entityCategory = i-1;
				}
				setTooltip(i == 0 ? "Show all categories." : s_categoryList[i - 1].tooltip.c_str());
			}
			ImGui::EndCombo();
		}
		setTooltip("Select a category to filter by.");
		ImGui::Separator();
		if (s_entityCategory != prevCategory || s_filteredEntityList.empty())
		{
			updateEntityFilter();
		}

		const s32 count = (s32)s_filteredEntityList.size();
		const u32 padding = 8;
		s32 selectedIndex = s_selectedEntity;

		ImGui::BeginChild("##EntityList");
		{
			const u32 idBase = 0x100100;
			for (s32 i = 0, x = 0; i < count; i++)
			{
				s32 index = s_filteredEntityList[i].index;
				Entity* entity = s_filteredEntityList[i].entity;
				bool isSelected = selectedIndex == index;

				void* ptr = nullptr;
				u32 w = 64, h = 64;
				Vec2f uv0, uv1;
				if (!entity->image && entity->type == ETYPE_3D)
				{
					const TextureGpu* thumbnail = getThumbnail(entity->obj3d, &uv0, &uv1, false);
					ptr = thumbnail ? TFE_RenderBackend::getGpuPtr(thumbnail) : nullptr;
				}
				else
				{
					f32 du = fabsf(entity->st[1].x - entity->st[0].x);
					f32 dv = fabsf(entity->st[1].z - entity->st[0].z);
					ptr = entity->image ? TFE_RenderBackend::getGpuPtr(entity->image) : nullptr;

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

					uv0 = entity->uv[0];
					uv1 = entity->uv[1];
				}

				if (x + w + padding >= 400)
				{
					ImGui::NewLine();
					x = 0;
				}
				x += w + padding;

				if (isSelected)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, { 1.0f, 1.0f, 0.3f, 1.0f });
				}

				ImGui::PushID(idBase + i);
				if (ImGui::ImageButton(ptr, ImVec2(f32(w), f32(h)), ImVec2(uv0.x, uv0.z), ImVec2(uv1.x, uv1.z), 2))
				{
					s_selectedEntity = index;
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
