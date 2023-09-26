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
	static EditorConfig s_editorConfig = {};

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

		const size_t len = configFile.getSize();
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

		snprintf(lineBuffer, 1024, "%s=%0.3f\r\n", "FontScale", s_editorConfig.fontScale);
		configFile.writeBuffer(lineBuffer, (u32)strlen(lineBuffer));

		snprintf(lineBuffer, 1024, "%s=%0.3f\r\n", "ThumbnailScale", s_editorConfig.thumbnailScale);
		configFile.writeBuffer(lineBuffer, (u32)strlen(lineBuffer));

		configFile.close();
		return true;
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

		ImGui::LabelText("##ConfigLabel", "Font Scale:");
		ImGui::SetNextItemWidth(196);
		ImGui::SliderFloat("##FontScaleSlider", &s_editorConfig.fontScale, 0.0f, 4.0f); ImGui::SameLine(220);
		ImGui::SetNextItemWidth(64);
		ImGui::InputFloat("##FontScaleInput", &s_editorConfig.fontScale);

		ImGui::LabelText("##ConfigLabel", "Thumbnail Scale:");
		ImGui::SetNextItemWidth(196);
		ImGui::SliderFloat("##ThumbnailScaleSlider", &s_editorConfig.thumbnailScale, 0.0f, 4.0f); ImGui::SameLine(220);
		ImGui::SetNextItemWidth(64);
		ImGui::InputFloat("##ThumbnailScaleInput", &s_editorConfig.thumbnailScale);

		ImGui::Separator();

		if (ImGui::Button("Save Config"))
		{
			saveConfig();
			s_configLoaded = true;
			finished = true;
		}

		// File dialogs...
		if (browseWinOpen >= 0)
		{
			if (browseWinOpen == 0)
			{
				FileResult res = TFE_Ui::directorySelectDialog("Editor Path", s_editorConfig.editorPath);
				if (!res.empty())
				{
					strcpy(s_editorConfig.editorPath, res[0].c_str());
				}
			}
			else
			{
				FileResult res = TFE_Ui::directorySelectDialog("Export Path", s_editorConfig.exportPath);
				if (!res.empty())
				{
					strcpy(s_editorConfig.exportPath, res[0].c_str());
				}
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
			s_editorConfig.fontScale = (f32)strtod(value, &endPtr);
		}
		else if (strcasecmp(key, "ThumbnailScale") == 0)
		{
			s_editorConfig.thumbnailScale = (f32)strtod(value, &endPtr);
		}
	}
}