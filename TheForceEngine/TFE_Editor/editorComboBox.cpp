#include "editorComboBox.h"
#include "editor.h"
#include <TFE_Ui/ui.h>

namespace TFE_Editor
{
	struct OverlayAssetList
	{
		bool active = false;
		s32 id = -1;
		ImVec2 pos = { 0, 0 };
		s32 listCount = 0;
		s32 lastHovered = -1;
		const Asset* assetList = nullptr;
		const ItemInfo* itemList = nullptr;
		char buffer[256] = "";
	};
	static OverlayAssetList s_overlayList = {};

	void editor_comboBoxInit()
	{
		s_overlayList = OverlayAssetList{};
	}

	bool editor_beginList()
	{
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_ChildWindow;
		const char* label = editor_getUniqueLabel("");
		ImGui::OpenPopup(label);
		return ImGui::BeginPopup(label, flags);
	}

	void editor_endList()
	{
		ImGui::EndPopup();
	}

	const char* getAssetName(const Asset* asset)
	{
		return asset->name.c_str();
	}

	bool strInStrNoCase(const char* srcStr, const char* findStr)
	{
		const s32 lenSrc = (s32)strlen(srcStr);
		const s32 lenFind = (s32)strlen(findStr);
		if (lenSrc < lenFind) { return false; }
		const s32 end = lenSrc - lenFind + 1;
		char f0 = tolower(findStr[0]);
		for (s32 i = 0; i < end; i++)
		{
			// Check the first letter and early out if they don't match.
			if (tolower(srcStr[i]) != f0) { continue; }
			// Otherwise check the full "find" string.
			if (strncasecmp(&srcStr[i], findStr, lenFind) == 0)
			{
				return true;
			}
		}
		return false;
	}

	void editor_handleOverlayList()
	{
		if (!s_overlayList.active || s_overlayList.id < 0) { return; }
		s_overlayList.lastHovered = -1;

		ImGui::SetNextWindowPos(s_overlayList.pos);
		ImGui::SetNextWindowSize(ImVec2(200, 400));
		if (editor_beginList())
		{
			const size_t lenInput = strlen(s_overlayList.buffer);
			for (s32 i = 0; i < s_overlayList.listCount; i++)
			{
				// Does it match the name.
				const char* itemName = nullptr;
				if (s_overlayList.assetList)
				{
					itemName = getAssetName(&s_overlayList.assetList[i]);
				}
				else if (s_overlayList.itemList)
				{
					itemName = s_overlayList.itemList[i].name.c_str();
				}
				const size_t lenItem = strlen(itemName);
				if (lenItem < lenInput || (lenInput && !strInStrNoCase(itemName, s_overlayList.buffer))) { continue; }

				// Add to the list.
				ImGui::Selectable(itemName);
				if (ImGui::IsItemHovered())
				{
					s_overlayList.lastHovered = i;
				}
			}
		}
		editor_endList();
	}

	bool editor_comboBoxInternal(s32 id, char* inputBuffer, size_t inputBufferSize, s32 listCount, const ItemInfo* itemList, const Asset* assetList)
	{
		// Text Input.
		ImVec2 pos = ImGui::GetWindowPos();
		pos.x += ImGui::GetCursorPosX();
		pos.y += ImGui::GetCursorPosY();
		bool update = ImGui::InputText(editor_getUniqueLabel(""), inputBuffer, inputBufferSize);

		if (ImGui::IsItemActive())
		{
			pos.y += 26;
			s_overlayList.id = id;
			s_overlayList.active = true;
			s_overlayList.lastHovered = -1;
			s_overlayList.pos = pos;
			s_overlayList.listCount = listCount;
			s_overlayList.assetList = assetList;
			s_overlayList.itemList = itemList;
			strcpy(s_overlayList.buffer, inputBuffer);
		}
		else if (s_overlayList.id == id && s_overlayList.active)
		{
			//ImGui::CloseCurrentPopup();
			if (s_overlayList.lastHovered >= 0)
			{
				if (assetList)
				{
					strcpy(inputBuffer, getAssetName(&s_overlayList.assetList[s_overlayList.lastHovered]));
				}
				else if (itemList)
				{
					strcpy(inputBuffer, itemList[s_overlayList.lastHovered].name.c_str());
				}
				update = true;
			}
			s_overlayList.active = false;
			s_overlayList.id = -1;
		}

		return update;
	}

	bool editor_itemEditComboBox(s32 id, char* inputBuffer, size_t inputBufferSize, s32 listCount, const ItemInfo* itemList)
	{
		return editor_comboBoxInternal(id, inputBuffer, inputBufferSize, listCount, itemList, nullptr);
	}

	bool editor_assetEditComboBox(s32 id, char* inputBuffer, size_t inputBufferSize, s32 listCount, const Asset* assetList)
	{
		return editor_comboBoxInternal(id, inputBuffer, inputBufferSize, listCount, nullptr, assetList);
	}
}