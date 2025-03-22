#include "editorConfig.h"
#include "editor.h"
#include <TFE_Editor/LevelEditor/hotkeys.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <TFE_System/iniParser.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_Ui/ui.h>

namespace TFE_Editor
{
	static bool s_configLoaded = false;
	static bool s_configDefaultsSet = false;
	EditorConfig s_editorConfig = {};

	void parseValue(const char* key, const std::string* values, s32 valueCount);
	
	bool configSetupRequired()
	{
		bool setupRequired = !s_configLoaded;
		if (setupRequired)
		{
			// Compute default UI scale based on display resolution.
			if (!s_configDefaultsSet)
			{
				DisplayInfo displayInfo;
				TFE_RenderBackend::getDisplayInfo(&displayInfo);

				// 1080p defaults.
				s_editorConfig.fontScale = 100;
				s_editorConfig.thumbnailSize = 64;

				// 4k defaults.
				if (displayInfo.height >= 2160)
				{
					s_editorConfig.fontScale = 150;
					s_editorConfig.thumbnailSize = 128;
				}
				// 1440p defaults.
				else if (displayInfo.height >= 1440)
				{
					s_editorConfig.fontScale = 125;
					s_editorConfig.thumbnailSize = 64;
				}
				s_configDefaultsSet = true;
			}
		}
		return setupRequired;
	}

	bool loadConfig()
	{
		char editorPath[TFE_MAX_PATH];
		TFE_Paths::appendPath(PATH_USER_DOCUMENTS, "editor.ini", editorPath);
		// Set to defaults before trying to load.
		LevelEditor::setDefaultKeyboardShortcuts();

		FileStream configFile;
		if (!configFile.open(editorPath, Stream::MODE_READ))
		{
			return false;
		}

		const u32 len = (u32)configFile.getSize();
		WorkBuffer& buffer = getWorkBuffer();
		buffer.resize(len);
		configFile.readBuffer(buffer.data(), len);
		configFile.close();

		TFE_Parser parser;
		parser.init((char*)buffer.data(), len);
		parser.addCommentString(";");
		parser.addCommentString("#");

		s_editorConfig = EditorConfig{};
		clearRecents();
		
		size_t bufferPos = 0;
		while (bufferPos < len)
		{
			const char* line = parser.readLine(bufferPos);
			if (!line) { break; }

			TokenList tokens;
			parser.tokenizeLine(line, tokens);
			const s32 tokenCount = (s32)tokens.size();

			if (tokenCount < 2) { continue; }
			parseValue(tokens[0].c_str(), &tokens[1], tokenCount - 1);
		}
		s_configLoaded = true;
		return true;
	}

	bool saveConfig()
	{
		char editorPath[TFE_MAX_PATH];
		TFE_Paths::appendPath(PATH_USER_DOCUMENTS, "editor.ini", editorPath);

		FileStream configFile;
		if (!configFile.open(editorPath, Stream::MODE_WRITE))
		{
			return false;
		}

		TFE_IniParser::writeKeyValue_String(configFile, "EditorPath", s_editorConfig.editorPath);
		TFE_IniParser::writeKeyValue_String(configFile, "ExportPath", s_editorConfig.exportPath);
		TFE_IniParser::writeKeyValue_Int(configFile, "FontScale",     s_editorConfig.fontScale);
		TFE_IniParser::writeKeyValue_Int(configFile, "ThumbnailSize", s_editorConfig.thumbnailSize);

		// Level Editor
		TFE_IniParser::writeKeyValue_Int(configFile, "Interface_Flags", s_editorConfig.interfaceFlags);
		TFE_IniParser::writeKeyValue_Float(configFile, "Curve_SegmentSize", s_editorConfig.curve_segmentSize);
		TFE_IniParser::writeKeyValue_String(configFile, "DarkForcesPort", s_editorConfig.darkForcesPort);
		TFE_IniParser::writeKeyValue_String(configFile, "OutlawsPort", s_editorConfig.outlawsPort);
		TFE_IniParser::writeKeyValue_String(configFile, "DarkForcesAddCmdLine", s_editorConfig.darkForcesAddCmdLine);
		TFE_IniParser::writeKeyValue_String(configFile, "OutlawsAddCmdLine", s_editorConfig.outlawsAddCmdLine);
		TFE_IniParser::writeKeyValue_Int(configFile, "LevelEditorFlags", s_editorConfig.levelEditorFlags);

		// Recent files.
		std::vector<RecentProject>* recentProjects = getRecentProjects();
		if (recentProjects && !recentProjects->empty())
		{
			char key[256];
			const s32 count = (s32)recentProjects->size();
			for (s32 i = 0; i < count; i++)
			{
				sprintf(key, "Recent[%d]", i);
				TFE_IniParser::writeKeyValue_String(configFile, key, (*recentProjects)[i].path.c_str());
			}
		}

		// Keybindings
		LevelEditor::writeLevelEditorShortcuts(configFile);

		configFile.close();
		s_configLoaded = true;
		return true;
	}

	void fontScaleControl()
	{
		const char* fontScaleStr[] =
		{
			"100%",
			"125%",
			"150%",
			"175%",
			"200%",
		};

		s32 item = fontScaleToIndex(s_editorConfig.fontScale);
		if (ImGui::Combo("Font Scale", &item, fontScaleStr, IM_ARRAYSIZE(fontScaleStr)))
		{
			s_editorConfig.fontScale = fontIndexToScale(item);
		}
	}

	void thumbnailSizeControl()
	{
		const char* thumbnailSizeStr[] =
		{
			"32",
			"64",
			"128",
			"256",
		};

		s32 item = s32(log2(s_editorConfig.thumbnailSize) + 0.5) - 5;
		if (ImGui::Combo("Thumbnail Size", &item, thumbnailSizeStr, IM_ARRAYSIZE(thumbnailSizeStr)))
		{
			s_editorConfig.thumbnailSize = 1 << (item + 5);
		}
	}

	bool configUi()
	{
		pushFont(TFE_Editor::FONT_SMALL);
		s32 menuHeight = 6 + (s32)ImGui::GetFontSize();

		bool finished = false;
		ImGui::SetWindowSize("Editor Config", { UI_SCALE(550), 70.0f + UI_SCALE(100) });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize;

		if (ImGui::BeginPopupModal("Editor Config", nullptr, window_flags))
		{
			s32 browseWinOpen = -1;
			ImGui::Text("Editor Path:"); ImGui::SameLine(UI_SCALE(120));
			ImGui::InputText("##EditorPath", s_editorConfig.editorPath, TFE_MAX_PATH);
			ImGui::SameLine();
			if (ImGui::Button("Browse##EditorPath"))
			{
				browseWinOpen = 0;
			}

			ImGui::Text("Export Path:"); ImGui::SameLine(UI_SCALE(120));
			ImGui::InputText("##ExportPath", s_editorConfig.exportPath, TFE_MAX_PATH);
			ImGui::SameLine();
			if (ImGui::Button("Browse##ExportPath"))
			{
				browseWinOpen = 1;
			}

			fontScaleControl();
			thumbnailSizeControl();
			ImGui::Separator();

			if (ImGui::Button("Save Config"))
			{
				saveConfig();
				finished = true;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if (ImGui::Button("Close"))
			{
				finished = true;
				ImGui::CloseCurrentPopup();
			}

			// File dialogs...
			if (browseWinOpen == 0)
			{
				FileResult res = TFE_Ui::directorySelectDialog("Editor Path", s_editorConfig.editorPath);
				if (!res.empty())
				{
					strcpy(s_editorConfig.editorPath, res[0].c_str());
				}
			}
			else if (browseWinOpen == 1)
			{
				FileResult res = TFE_Ui::directorySelectDialog("Export Path", s_editorConfig.exportPath);
				if (!res.empty())
				{
					strcpy(s_editorConfig.exportPath, res[0].c_str());
				}
			}

			ImGui::EndPopup();
		}
		popFont();

		return finished;
	}
		
	s32 fontScaleToIndex(s32 scale)
	{
		return (s_editorConfig.fontScale - 100) / 25;
	}

	s32 fontIndexToScale(s32 index)
	{
		return 100 + index * 25;
	}

	//////////////////////////////////////////
	// Internal
	//////////////////////////////////////////
	void parseValue(const char* key, const std::string* values, s32 valueCount)
	{
		const char* value1 = values[0].c_str();

		if (strcasecmp(key, "EditorPath") == 0)
		{
			strcpy(s_editorConfig.editorPath, value1);
		}
		else if (strcasecmp(key, "ExportPath") == 0)
		{
			strcpy(s_editorConfig.exportPath, value1);
		}
		else if (strcasecmp(key, "FontScale") == 0)
		{
			s_editorConfig.fontScale = TFE_IniParser::parseInt(value1);
		}
		else if (strcasecmp(key, "ThumbnailSize") == 0)
		{
			s_editorConfig.thumbnailSize = TFE_IniParser::parseInt(value1);
		}
		else if (strcasecmp(key, "Interface_Flags") == 0)
		{
			s_editorConfig.interfaceFlags = TFE_IniParser::parseInt(value1);
		}
		else if (strcasecmp(key, "Curve_SegmentSize") == 0)
		{
			s_editorConfig.curve_segmentSize = TFE_IniParser::parseFloat(value1);
		}
		else if (strcasecmp(key, "DarkForcesPort") == 0)
		{
			strcpy(s_editorConfig.darkForcesPort, value1);
		}
		else if (strcasecmp(key, "OutlawsPort") == 0)
		{
			strcpy(s_editorConfig.outlawsPort, value1);
		}
		else if (strcasecmp(key, "DarkForcesAddCmdLine") == 0)
		{
			strcpy(s_editorConfig.darkForcesAddCmdLine, value1);
		}
		else if (strcasecmp(key, "OutlawsAddCmdLine") == 0)
		{
			strcpy(s_editorConfig.outlawsAddCmdLine, value1);
		}
		else if (strcasecmp(key, "LevelEditorFlags") == 0)
		{
			s_editorConfig.levelEditorFlags = TFE_IniParser::parseInt(value1);
		}
		else if (strncasecmp(key, "Recent", strlen("Recent")) == 0)
		{
			addToRecents(value1);
		}
		else if (strncasecmp(key, "LEV_", 4) == 0)
		{
			LevelEditor::parseLevelEditorShortcut(key, values, valueCount);
		}
	}
}