#include "userPreferences.h"
#include "levelEditor.h"
#include "levelEditorData.h"
#include "tabControl.h"
#include "sharedState.h"
#include "hotkeys.h"
#include <TFE_Editor/editor.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Input/input.h>
#include <TFE_System/math.h>
#include <TFE_Ui/ui.h>
#include <algorithm>
#include <map>

using namespace TFE_Editor;

namespace LevelEditor
{
	enum PreferencesTab
	{
		PTAB_INTERFACE = 0,
		PTAB_EDITING,
		PTAB_INPUT,
		PTAB_THEME,
		PTAB_COUNT
	};
	const char* c_prefTabs[PTAB_COUNT] = { "Interface", "Editing", "Input", "Theme" };
	const char* c_prefToolTips[PTAB_COUNT] =
	{
		"Interface.\nLevel editor interface options.",
		"Editing.\nEditing options, such as undo and curve settings.",
		"Input.\nShortcuts and input settings.",
		"Theme.\nEditor theme, UI and viewport colors.",
	};
	static PreferencesTab s_prefTab = PTAB_INTERFACE;
	static s32 s_inputIndex = -1;

	void commitCurChanges(void);
	void interfacePref();
	void editingPref();
	void inputPref();
	void themePref();
								
	bool userPreferences()
	{
		pushFont(FONT_SMALL);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

		bool applyChanges = false;
		bool cancel = false;
		if (ImGui::BeginPopupModal("User Preferences", nullptr, window_flags))
		{
			const s32 prevTab = s_prefTab;
			s_prefTab = (PreferencesTab)handleTabs(s_prefTab, 0, 0, PTAB_COUNT, c_prefTabs, c_prefToolTips, commitCurChanges);
			if (prevTab != s_prefTab)
			{
				// Clear state between tabs.
				s_inputIndex = -1;
			}
			// Several tabs: Interface (Prompt quit, UI settings), Editing (undo settings, curve settings, etc.), Input (keyboard bindings), Theme (colors), 
			ImGui::Separator();

			switch (s_prefTab)
			{
				case PTAB_INTERFACE:
				{
					interfacePref();
				} break;
				case PTAB_EDITING:
				{
					editingPref();
				} break;
				case PTAB_INPUT:
				{
					inputPref();
				} break;
				case PTAB_THEME:
				{
					themePref();
				} break;
			}

			ImGui::Separator();
			if (ImGui::Button("Apply"))
			{
				saveConfig();
				applyChanges = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				loadConfig();
				cancel = true;
			}
			ImGui::EndPopup();
		}
		popFont();

		return applyChanges || cancel;
	}

	void interfacePref()
	{
		sectionHeader("Viewport Options");
		optionCheckbox("Hide Level Notes", (u32*)&s_editorConfig.interfaceFlags, PIF_HIDE_NOTES, 140);
		optionCheckbox("Hide Guidelines", (u32*)&s_editorConfig.interfaceFlags, PIF_HIDE_GUIDELINES, 140);
	}

	void editingPref()
	{
		u32 flags = s_editorConfig.waitForPlayCompletion ? 1 : 0;
		optionCheckbox("On Play, wait for process completion", &flags, 1, 300);
		s_editorConfig.waitForPlayCompletion = (flags & 1) != 0;
		
		optionSliderEditFloat("Curve Segment Size", "%.2f", &s_editorConfig.curve_segmentSize, 0.1f, 100.0f, 0.1f);
	}
		
	void inputPref()
	{
		const u8* repeatKeys = hotkeys_checkForKeyRepeats();
		if (ImGui::Button("Reset To Defaults"))
		{
			setDefaultKeyboardShortcuts();
		}
		ImGui::Separator();
		bool buttonPressed = false;
		if (ImGui::BeginChild("###InputList", ImVec2(670.0f, 768.0f)))
		{
			for (s32 i = 0; i < SHORTCUT_COUNT; i++)
			{
				const ShortcutId id = ShortcutId(i);

				const char* keyCombo = getShortcutKeyComboText(id);
				const char* desc = getKeyboardShortcutDesc(id);

				ImGui::Text("%s", desc); ImGui::SameLine(512.0f);
				ImGui::SetNextItemWidth(128.0f);
				if (s_inputIndex != i)
				{
					if (repeatKeys[i])
					{
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
					}

					if (ImGui::Button(editor_getUniqueLabel(keyCombo), ImVec2(128.0f, 0.0f)))
					{
						s_inputIndex = i;
						buttonPressed = true;
					}

					if (repeatKeys[i])
					{
						ImGui::PopStyleColor();
					}
				}
				else
				{
					ImGui::Button(editor_getUniqueLabel("Press Key"), ImVec2(128.0f, 0.0f));
					const KeyboardCode key = TFE_Input::getKeyPressed(true);
					const KeyModifier mod = TFE_Input::getKeyModifierDown();
					const MouseButton mbtn = TFE_Input::getMouseButtonPressed();
					if (key != KEY_UNKNOWN || (mbtn != MBUTTON_UNKNOWN && mbtn != MBUTTON_LEFT))
					{
						addKeyboardShortcut(id, key, mod, mbtn);
						s_inputIndex = -1;
					}
				}
				ImGui::Separator();
			}
		}
		ImGui::EndChild();

		// Clear out the selected input if the mouse is pressed elsewhere...
		if (!buttonPressed && TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
		{
			s_inputIndex = -1;
		}
	}

	void themePref()
	{
		ImGui::LabelText("##Label", "Theme Placeholder");
	}

	void commitCurChanges(void)
	{
	}
}  // namespace LevelEditor