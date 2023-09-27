#include "editorConfig.h"
#include "editor.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui.h>

namespace TFE_Editor
{
	static bool s_configLoaded = false;
	EditorConfig s_editorConfig = {};

	void parseValue(const char* key, const char* value);
	
	bool configSetupRequired()
	{
		return !s_configLoaded;
	}

	bool loadConfig()
	{
		char editorPath[TFE_MAX_PATH];
		TFE_Paths::appendPath(PATH_USER_DOCUMENTS, "editor.ini", editorPath);

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

		s_editorConfig = {};
		size_t bufferPos = 0;
		while (bufferPos < len)
		{
			const char* line = parser.readLine(bufferPos);
			if (!line) { break; }

			TokenList tokens;
			parser.tokenizeLine(line, tokens);
			if (tokens.size() < 1) { continue; }

			if (tokens.size() == 2)
			{
				parseValue(tokens[0].c_str(), tokens[1].c_str());
			}
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

		char lineBuffer[1024];
		snprintf(lineBuffer, 1024, "%s=\"%s\"\r\n", "EditorPath", s_editorConfig.editorPath);
		configFile.writeBuffer(lineBuffer, (u32)strlen(lineBuffer));

		snprintf(lineBuffer, 1024, "%s=\"%s\"\r\n", "ExportPath", s_editorConfig.exportPath);
		configFile.writeBuffer(lineBuffer, (u32)strlen(lineBuffer));

		snprintf(lineBuffer, 1024, "%s=%d\r\n", "FontScale", s_editorConfig.fontScale);
		configFile.writeBuffer(lineBuffer, (u32)strlen(lineBuffer));

		snprintf(lineBuffer, 1024, "%s=%d\r\n", "ThumbnailSize", s_editorConfig.thumbnailSize);
		configFile.writeBuffer(lineBuffer, (u32)strlen(lineBuffer));

		configFile.close();
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

		s32 item = (s_editorConfig.fontScale - 100) / 25;
		if (ImGui::Combo("Font Scale", &item, fontScaleStr, IM_ARRAYSIZE(fontScaleStr)))
		{
			s_editorConfig.fontScale = item * 25 + 100;
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
		bool active = true;
		bool finished = false;
		ImGui::SetWindowPos("Editor Config", { 0.0f, 20.0f });
		ImGui::SetWindowSize("Editor Config", { 512.0f, 196.0f });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

		ImGui::Begin("Editor Config", &active, window_flags);

		s32 browseWinOpen = -1;
		ImGui::Text("Editor Path:"); ImGui::SameLine(100);
		ImGui::InputText("##EditorPath", s_editorConfig.editorPath, TFE_MAX_PATH);
		ImGui::SameLine();
		if (ImGui::Button("Browse##EditorPath"))
		{
			browseWinOpen = 0;
		}

		ImGui::Text("Export Path:"); ImGui::SameLine(100);
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
			s_configLoaded = true;
			finished = true;
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

		ImGui::End();
		return finished;
	}

	//////////////////////////////////////////
	// Internal
	//////////////////////////////////////////
	void parseValue(const char* key, const char* value)
	{
		char* endPtr = nullptr;
		if (strcasecmp(key, "EditorPath") == 0)
		{
			strcpy(s_editorConfig.editorPath, value);
		}
		else if (strcasecmp(key, "ExportPath") == 0)
		{
			strcpy(s_editorConfig.exportPath, value);
		}
		else if (strcasecmp(key, "FontScale") == 0)
		{
			s_editorConfig.fontScale = (s32)strtol(value, &endPtr, 10);
		}
		else if (strcasecmp(key, "ThumbnailSize") == 0)
		{
			s_editorConfig.thumbnailSize = (s32)strtol(value, &endPtr, 10);
		}
	}
}