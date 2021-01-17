#include "infDebugger.h"
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_System/profiler.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_Ui/ui.h>
#include <TFE_Ui/markdown.h>
#include <TFE_System/parser.h>

#include <TFE_InfSystem/infSystem.h>
#include <TFE_InfSystem/infItem.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>

namespace TFE_InfDebugger
{
	static bool s_open = false;
	static DisplayInfo s_displayInfo;

	static s32 s_selectedItem = -1;
	static s32 s_outputSelected = -1;

	const s32 s_debugWidth = 658;
	const s32 s_outputHeight = 256;
	const s32 s_itemListWidth = 360;

	struct OutputMsg
	{
		u32 itemIndex;
		std::string msg;
	};
	static std::vector<OutputMsg> s_output;

	void updateOutputPanel();
	void updateDebugPanel();
	void updateItemList();
	void OutputListenerFunc(u32 itemIndex, const char* msg);

	bool init()
	{
		TFE_InfSystem::setListenCallback(OutputListenerFunc);
		return true;
	}

	void destroy()
	{
	}
		
	void update()
	{
		if (!s_open) { return; }

		TFE_RenderBackend::getDisplayInfo(&s_displayInfo);
		updateOutputPanel();
		updateDebugPanel();
		updateItemList();
	}

	bool isEnabled()
	{
		return s_open;
	}
		
	void enable(bool enable)
	{
		s_open = enable;
		if (s_open)
		{
			s_selectedItem = -1;
			s_outputSelected = -1;
			if (TFE_InfSystem::getItemCount())
			{
				s_selectedItem = 0;
			}
		}
	}
		
	void OutputListenerFunc(u32 itemIndex, const char* msg)
	{
		s_output.push_back({itemIndex, msg});
	}

	void updateOutputPanel()
	{
		const s32 w = s_displayInfo.width - s_debugWidth, h = s_outputHeight;

		ImGui::SetNextWindowPos(ImVec2(s_displayInfo.width - w, s_displayInfo.height - h));
		ImGui::SetNextWindowSize(ImVec2(w, h));

		bool open = true;
		ImGui::Begin("Output", &open);

		const size_t outputCount = s_output.size();
		const OutputMsg* output = s_output.data();
		for (size_t i = 0; i < outputCount; i++)
		{
			char label[256];
			if (output[i].itemIndex < 0xffffffff)
			{
				char name[256];
				TFE_InfSystem::getItemName(output[i].itemIndex, name);
				sprintf(label, "[%s] %s##Output%d",name, output[i].msg.c_str(), i);
			}
			else
			{
				sprintf(label, "%s##Output%d", output[i].msg.c_str(), i);
			}

			if (ImGui::Selectable(label, s_outputSelected == i, ImGuiSelectableFlags_AllowDoubleClick))
			{
				s_outputSelected = i;
				if (output[i].itemIndex >= 0)
				{
					s_selectedItem = output[i].itemIndex;
				}
			}
		}

		ImGui::End();
	}

	static const char* c_debugTypeStr[]=
	{
		"unsigned int 16", // DBG_ITEM_U16
		"signed int 16",   // DBG_ITEM_S16
		"unsigned int 32", // DBG_ITEM_U32
		"signed int 32",   // DBG_ITEM_S32
		"float",		   // DBG_ITEM_F32
		"string",		   // DBG_ITEM_STR
	};

	enum DebugItemType
	{
		DBG_ITEM_U16 = 0,
		DBG_ITEM_S16,
		DBG_ITEM_U32,
		DBG_ITEM_S32,
		DBG_ITEM_F32,
		DBG_ITEM_STR,
	};

	struct DebugItem
	{
		DebugItemType type;
		std::string name;
		bool readOnly;

		union
		{
			void* valuePtr;
			u16* valueU16;
			s16* valueS16;
			u32* valueU32;
			s32* valueS32;
			f32* valueF32;
			char* valueStr;
		};
	};

	static std::vector<DebugItem> s_dbgItems;
		
	void setupDebugItem(s32 index)
	{
		if (index < 0 || index >= TFE_InfSystem::getItemCount()) { return; }

		TFE_InfSystem::ItemState* itemState = TFE_InfSystem::getItemState(s_selectedItem);
		InfItem* item = TFE_InfSystem::getItem(s_selectedItem);
		InfClassData* classData = item->classData;

		s_dbgItems.clear();
		if (classData->iclass == INF_CLASS_ELEVATOR)
		{
			s_dbgItems.push_back({ DBG_ITEM_U16, "Stop Count", true, &classData->stopCount });
			s_dbgItems.push_back({ DBG_ITEM_S32, "Current Stop", false, &itemState->curStop });
			s_dbgItems.push_back({ DBG_ITEM_F32, "Value", false, &itemState->curValue });
			s_dbgItems.push_back({ DBG_ITEM_F32, "Speed", false, &classData->var.speed });
			s_dbgItems.push_back({ DBG_ITEM_U32, "Key", false, &classData->var.key });
		}
		else if (classData->iclass == INF_CLASS_TRIGGER)
		{
			s_dbgItems.push_back({ DBG_ITEM_U32, "Event", false, &classData->var.event });
			s_dbgItems.push_back({ DBG_ITEM_U32, "Event Mask", false, &classData->var.event_mask });
			s_dbgItems.push_back({ DBG_ITEM_U32, "Entity Mask", false, &classData->var.entity_mask });
		}
		else if (classData->iclass == INF_CLASS_TELEPORTER)
		{
			s_dbgItems.push_back({ DBG_ITEM_S32, "Target", false, &classData->var.target });
		}
	}

	void updateDebugPanel()
	{
		const s32 w = s_debugWidth, h = s_outputHeight;
		setupDebugItem(s_selectedItem);

		ImGui::SetNextWindowPos(ImVec2(0, s_displayInfo.height - h));
		ImGui::SetNextWindowSize(ImVec2(w, h));

		bool open = true;
		ImGui::Begin("INF Debug", &open, ImGuiWindowFlags_NoTitleBar);

		const s32 widthName = 256, widthValue = 128, widthType = 256;
		
		if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None))
		{
			if (ImGui::BeginTabItem("State"))
			{
				if (s_selectedItem >= 0)
				{
					char name[256];
					TFE_InfSystem::getItemName(s_selectedItem, name);
					const char* className = TFE_InfSystem::getItemClass(s_selectedItem);
					const char* subClassName = TFE_InfSystem::getItemSubClass(s_selectedItem);
					ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "\"%s\" : %s : %s", name, className, subClassName);
					ImGui::Separator();

					f32 startY = s_displayInfo.height - h + 48;

					TFE_InfSystem::ItemState* itemState = TFE_InfSystem::getItemState(s_selectedItem);
					InfItem* item = TFE_InfSystem::getItem(s_selectedItem);
					InfClassData* classData = item->classData;

					size_t dbgVarCount = s_dbgItems.size();
					DebugItem* dbgItems = s_dbgItems.data();

					ImGui::SetNextWindowPos(ImVec2(0.0f, startY));
					if (ImGui::BeginChild("##StateName", ImVec2(widthName, h - 56), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
					{
						for (size_t i = 0; i < dbgVarCount; i++)
						{
							char label[256];
							sprintf(label, "##StateName_%d", i);

							ImGui::SetNextItemWidth(widthName - 8.0f);
							ImGui::InputText(label, (char*)dbgItems[i].name.c_str(), dbgItems[i].name.length(), ImGuiInputTextFlags_ReadOnly);
						}
						ImGui::EndChild();
					}
					ImGui::SetNextWindowPos(ImVec2(widthName, startY));
					if (ImGui::BeginChild("##StateValue", ImVec2(widthValue, h - 56), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
					{
						for (size_t i = 0; i < dbgVarCount; i++)
						{
							char label[256];
							sprintf(label, "##StateValue_%d", i);

							ImGui::SetNextItemWidth(widthValue - 8.0f);
							switch (dbgItems[i].type)
							{
								case DBG_ITEM_U16:
								{
									u32 tmpValue = (u32)(*dbgItems[i].valueU16);
									if (ImGui::InputUInt(label, &tmpValue, 1, 100, ImGuiInputTextFlags_NoStepButtons | (dbgItems[i].readOnly ? ImGuiInputTextFlags_ReadOnly : 0)))
									{
										*dbgItems[i].valueU16 = u16(tmpValue);
									}
								} break;
								case DBG_ITEM_S16:
								{
									s32 tmpValue = (s32)(*dbgItems[i].valueS16);
									if (ImGui::InputInt(label, &tmpValue, 1, 100, ImGuiInputTextFlags_NoStepButtons | (dbgItems[i].readOnly ? ImGuiInputTextFlags_ReadOnly : 0)))
									{
										*dbgItems[i].valueS16 = s16(tmpValue);
									}
								} break;
								case DBG_ITEM_U32:
								{
									ImGui::InputUInt(label, dbgItems[i].valueU32, 1, 100, ImGuiInputTextFlags_NoStepButtons | (dbgItems[i].readOnly ? ImGuiInputTextFlags_ReadOnly : 0));
								} break;
								case DBG_ITEM_S32:
								{
									ImGui::InputInt(label, dbgItems[i].valueS32, 1, 100, ImGuiInputTextFlags_NoStepButtons | (dbgItems[i].readOnly ? ImGuiInputTextFlags_ReadOnly : 0));
								} break;
								case DBG_ITEM_F32:
								{
									ImGui::InputFloat(label, dbgItems[i].valueF32, 1.0f, 100.0f, "%.3f", ImGuiInputTextFlags_NoStepButtons | (dbgItems[i].readOnly ? ImGuiInputTextFlags_ReadOnly : 0));
								} break;
								case DBG_ITEM_STR:
								{
									// TODO
								} break;
							};
						}
						ImGui::EndChild();
					}
					ImGui::SetNextWindowPos(ImVec2(widthName + widthValue, startY));
					if (ImGui::BeginChild("##StateType", ImVec2(widthType, h - 56), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
					{
						for (size_t i = 0; i < dbgVarCount; i++)
						{
							char label[256];
							sprintf(label, "##StateType_%d", i);

							ImGui::SetNextItemWidth(widthType - 8.0f);
							ImGui::InputText(label, (char*)c_debugTypeStr[dbgItems[i].type], strlen(c_debugTypeStr[dbgItems[i].type]), ImGuiInputTextFlags_ReadOnly);
						}
						ImGui::EndChild();
					}
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Breakpoints"))
			{
				ImGui::Text("Breakpoints tab...");
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::End();
	}

	void updateItemList()
	{
		const s32 w = s_itemListWidth, h = s_displayInfo.height - s_outputHeight;
		const s32 nameWidth = 160, classWidth = 80, subclassWidth = 120;
				
		ImGui::SetNextWindowPos(ImVec2(s_displayInfo.width - w, 0));
		ImGui::SetNextWindowSize(ImVec2(w, h));

		bool open = true;
		if (ImGui::Begin("Items", &open, ImGuiWindowFlags_NoScrollbar))
		{
			f32 startY = ImGui::GetWindowContentRegionMin().y - 8;
			const s32 itemCount = TFE_InfSystem::getItemCount();

			ImGui::SetNextWindowPos(ImVec2(s_displayInfo.width - w, startY));
			if (ImGui::BeginChild("##NameItemColumn", ImVec2(nameWidth, h-startY), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
			{
				ImGui::Button("Name##Items", ImVec2(nameWidth - 16, 0));
				for (s32 i = 0; i < itemCount; i++)
				{
					char name[256];
					TFE_InfSystem::getItemName(i, name);

					char label[512];
					sprintf(label, "%s##ItemName%d", name, i);
					if (ImGui::Selectable(label, s_selectedItem == i, ImGuiSelectableFlags_AllowDoubleClick))
					{
						s_selectedItem = i;
					}
				}
				ImGui::EndChild();
			}
			ImGui::SetNextWindowPos(ImVec2(s_displayInfo.width - w + nameWidth, startY));
			if (ImGui::BeginChild("##ClassItemColumn", ImVec2(classWidth, h - startY), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
			{
				ImGui::Button("Class##Items", ImVec2(classWidth - 16, 0));
				for (s32 i = 0; i < itemCount; i++)
				{
					char label[512];
					sprintf(label, "%s##ItemClass%d", TFE_InfSystem::getItemClass(i), i);
					if (ImGui::Selectable(label, s_selectedItem == i, ImGuiSelectableFlags_AllowDoubleClick))
					{
						s_selectedItem = i;
					}
				}
				ImGui::EndChild();
			}
			ImGui::SetNextWindowPos(ImVec2(s_displayInfo.width - w + nameWidth + classWidth, startY));
			if (ImGui::BeginChild("##Sub-ClassItemColumn", ImVec2(subclassWidth, h - startY), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
			{
				ImGui::Button("Sub-Class##Items", ImVec2(subclassWidth-16, 0));
				for (s32 i = 0; i < itemCount; i++)
				{
					char label[512];
					sprintf(label, "%s##ItemSubClass%d", TFE_InfSystem::getItemSubClass(i), i);
					if (ImGui::Selectable(label, s_selectedItem == i, ImGuiSelectableFlags_AllowDoubleClick))
					{
						s_selectedItem = i;
					}
				}
				ImGui::EndChild();
			}
			ImGui::End();
		}
	}
}
