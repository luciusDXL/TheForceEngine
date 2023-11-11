#include "browser.h"
#include "infoPanel.h"
#include "sharedState.h"
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/EditorAsset/editorFrame.h>
#include <TFE_Editor/EditorAsset/editorSprite.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;

namespace LevelEditor
{
	static s32 s_focusOnRow = -1;

	void browseTextures();
	
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

		if (s_focusOnRow >= 0)
		{
			ImGui::SetScrollY(s_focusOnRow * (64 + 8) - 128);
			s_focusOnRow = -1;
		}
	}

	void browserEnd()
	{
		ImGui::End();
	}

	void drawBrowser()
	{
		browserBegin(infoPanelGetHeight());
		browseTextures();
		browserEnd();
	}

	void browserScrollToSelection()
	{
		if (s_selectedTexture < 0) { return; }
		s_focusOnRow = s_selectedTexture / 6;
	}

	void browseTextures()
	{
		u32 count = (u32)s_levelTextureList.size();
		u32 rows = count / 6;
		u32 leftOver = count - rows * 6;
		f32 y = 0.0f;

		ImVec4 bgColor       = { 0.0f, 0.0f, 0.0f, 0.0f };
		ImVec4 tintColorNrml = { 1.0f, 1.0f, 1.0f, 1.0f };
		ImVec4 tintColorSel  = { 1.5f, 1.5f, 1.5f, 1.0f };

		for (u32 i = 0; i < rows; i++)
		{
			for (u32 t = 0; t < 6; t++)
			{
				s32 index = i * 6 + t;
				EditorTexture* texture = (EditorTexture*)getAssetData(s_levelTextureList[index].handle);
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

				bool isSelected = s_selectedTexture == index;
				if (isSelected)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, {1.0f, 1.0f, 0.3f, 1.0f});
				}
				
				if (ImGui::ImageButton(ptr, ImVec2(f32(w), f32(h)), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f), 2))
				{
					// TODO: add hover functionality (tool tip - but show full texture + file name + size)
					s_selectedTexture = index;
				}

				if (isSelected)
				{
					ImGui::PopStyleColor();
				}

				ImGui::SameLine();
			}
			ImGui::NewLine();
		}
	}
}
