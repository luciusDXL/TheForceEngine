#include "historyView.h"
#include "editor.h"
#include "history.h"
#include <TFE_Ui/ui.h>

namespace TFE_Editor
{
	bool historyView()
	{
		const ImVec4 hiddenColor(1.0f, 1.0f, 1.0f, 0.25f);

		pushFont(FONT_SMALL);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

		bool exit = false;
		if (ImGui::BeginPopupModal("History View", nullptr, window_flags))
		{
			u32 hSize = history_getSize();
			const char* sizeTypeStr[] = { "Bytes", "KB", "MB" };
			s32 sizeType = 0;
			f32 scale = 1.0;
			if (hSize > 1024 * 1024)
			{
				sizeType = 2;
				scale = 1.0 / (1024.0 * 1024.0);
			}
			else if (hSize > 1024)
			{
				sizeType = 1;
				scale = 1.0 / (1024.0);
			}
			ImGui::Text("Items: %d   Size: %0.2f %s", history_getItemCount(), f64(history_getSize()) * scale, sizeTypeStr[sizeType]);

			ImGui::BeginChild("##HistoryList", ImVec2(256, 512), ImGuiChildFlags_Border);
			{
				const u32 count = history_getItemCount();
				const s32 pos = history_getPos();

				for (u32 i = 0; i < count; i++)
				{
					u32 parentId;
					bool isHidden;
					const char* name = history_getItemNameAndState(i, parentId, isHidden);

					if (isHidden) { ImGui::PushStyleColor(ImGuiCol_Text, hiddenColor); }
					if (ImGui::Selectable(editor_getUniqueLabel(name), pos == i))
					{
						history_setPos(i);
						history_showBranch(i);
					}
					if (isHidden) { ImGui::PopStyleColor(); }
				}
			}
			ImGui::EndChild();

			if (ImGui::Button("Close"))
			{
				exit = true;
			}

			ImGui::EndPopup();
		}
		popFont();
		return exit;
	}
}