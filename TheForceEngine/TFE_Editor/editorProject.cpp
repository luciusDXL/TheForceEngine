#include "editorProject.h"
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/editor.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <TFE_System/iniParser.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Game/igame.h>
#include <TFE_Archive/archive.h>
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>

namespace TFE_Editor
{
	enum
	{
		MAX_DESCR_LEN = 65536
	};

	const char* c_projTypes[] =
	{
		"Levels",
		"Resources Only",
	};
	const char* c_featureSet[] =
	{
		"Vanilla",
		"TFE",
	};

	static Project s_newProject = {};
	static Project s_curProject = {};
	static char s_descrBuffer[MAX_DESCR_LEN];
	static bool s_createDir = true;
	static std::string* s_curStringOut = nullptr;

	void parseProjectValue(const char* key, const char* value);

	Project* project_get()
	{
		return &s_curProject;
	}

	void project_close()
	{
		s_curProject = {};
	}

	void project_prepareNew()
	{
		s_newProject = {};
		s_createDir = true;
	}

	void project_prepareEdit()
	{
		s_newProject = s_curProject;
	}
		
	void project_save()
	{
		char projFile[TFE_MAX_PATH];
		sprintf(projFile, "%s/%s.ini", s_curProject.path, s_curProject.name);
		FileUtil::fixupPath(projFile);

		FileStream proj;
		if (!proj.open(projFile, Stream::MODE_WRITE))
		{
			return;
		}

		TFE_IniParser::writeKeyValue_String(proj, "Name", s_curProject.name);
		TFE_IniParser::writeKeyValue_String(proj, "Path", s_curProject.path);
		TFE_IniParser::writeKeyValue_StringBlock(proj, "Description", s_curProject.desc.c_str());
		TFE_IniParser::writeKeyValue_StringBlock(proj, "Authors", s_curProject.authors.c_str());
		TFE_IniParser::writeKeyValue_StringBlock(proj, "Credits", s_curProject.credits.c_str());

		TFE_IniParser::writeKeyValue_String(proj, "ProjectType", c_projTypes[s_curProject.type]);
		TFE_IniParser::writeKeyValue_String(proj, "Game", TFE_Settings::c_gameName[s_curProject.game]);
		TFE_IniParser::writeKeyValue_String(proj, "FeatureSet", c_featureSet[s_curProject.game]);
		TFE_IniParser::writeKeyValue_Int(proj, "Flags", (s32)s_curProject.flags);

		proj.close();

		addToRecents(projFile);
	}
		
	bool project_load(const char* filepath)
	{
		FileStream projectFile;
		if (!projectFile.open(filepath, Stream::MODE_READ))
		{
			return false;
		}

		const u32 len = (u32)projectFile.getSize();
		WorkBuffer& buffer = getWorkBuffer();
		buffer.resize(len);
		projectFile.readBuffer(buffer.data(), len);
		projectFile.close();

		TFE_Parser parser;
		parser.init((char*)buffer.data(), len);
		parser.addCommentString(";");
		parser.addCommentString("#");

		s_curProject = {};
		s_curStringOut = nullptr;
		size_t bufferPos = 0;
		while (bufferPos < len)
		{
			const char* line = parser.readLine(bufferPos);
			if (!line) { break; }

			if (s_curStringOut)
			{
				if (line[0] == '}')
				{
					// Done.
					s_curStringOut = nullptr;
				}
				else
				{
					*s_curStringOut += line;
					*s_curStringOut += "\n";
				}
				continue;
			}

			TokenList tokens;
			parser.tokenizeLine(line, tokens);
			if (tokens.size() < 1) { continue; }

			if (tokens.size() == 2)
			{
				parseProjectValue(tokens[0].c_str(), tokens[1].c_str());
			}
		}

		// Validate that the path exists.
		if (!FileUtil::directoryExits(s_curProject.path))
		{
			TFE_System::logWrite(LOG_ERROR, "Editor Project", "Editor Project Path '%s' does not exist.", s_curProject.path);
			removeFromRecents(s_curProject.path);

			s_curProject = {};
			return false;
		}

		s_curProject.active = true;
		addToRecents(filepath);
		return true;
	}
		
	bool project_editUi(bool newProject)
	{
		// Make sure we are *either* creating a new project or editing an active project.
		if (!newProject && !s_curProject.active) { return true; }

		pushFont(TFE_Editor::FONT_SMALL);

		DisplayInfo info;
		TFE_RenderBackend::getDisplayInfo(&info);
		f32 width  = std::min((f32)info.width - 64.0f, UI_SCALE(900));
		f32 height = std::min((f32)info.height - 64.0f, 70.0f + UI_SCALE(600));

		bool active = true;
		bool finished = false;
		ImGui::SetWindowSize("Project", { width, height });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize;

		if (ImGui::BeginPopupModal("Project", &active, window_flags))
		{
			f32 labelWidth = ImGui::CalcTextSize("Description").x + ImGui::GetFontSize();

			ImGui::Text("Name"); ImGui::SameLine(labelWidth);
			ImGui::InputText("##Name", s_newProject.name, 256);
			
			ImGui::Text("Path"); ImGui::SameLine(labelWidth);
			ImGui::InputText("##Path", s_newProject.path, TFE_MAX_PATH);
			ImGui::SameLine();
			if (ImGui::Button("Browse"))
			{
				FileResult res = TFE_Ui::directorySelectDialog("Project Path", s_newProject.path);
				if (!res.empty())
				{
					strcpy(s_newProject.path, res[0].c_str());
				}
			}

			ImGui::Checkbox("Create Directory for Project", &s_createDir);
			ImGui::Separator();

			// Settings.
			listSelection("Project Type", c_projTypes, TFE_ARRAYSIZE(c_projTypes), (s32*)&s_newProject.type, 128, 196);
			listSelection("Game", TFE_Settings::c_gameName, TFE_ARRAYSIZE(TFE_Settings::c_gameName), (s32*)&s_newProject.game, 128, 196);
			listSelection("Feature Set", c_featureSet, TFE_ARRAYSIZE(c_featureSet), (s32*)&s_newProject.featureSet, 128, 196);

			ImGui::CheckboxFlags("True Color Only", &s_newProject.flags, PFLAG_TRUE_COLOR);
			ImGui::Separator();

			// TODO: Handle word wrap...
			strcpy(s_descrBuffer, s_newProject.desc.c_str());
			ImGui::Text("Description"); ImGui::SameLine(labelWidth);
			if (ImGui::InputTextMultiline("##Description", s_descrBuffer, MAX_DESCR_LEN))
			{
				s_newProject.desc = s_descrBuffer;
			}

			strcpy(s_descrBuffer, s_newProject.authors.c_str());
			ImGui::Text("Authors"); ImGui::SameLine(labelWidth);
			if (ImGui::InputTextMultiline("##Authors", s_descrBuffer, MAX_DESCR_LEN))
			{
				s_newProject.authors = s_descrBuffer;
			}

			strcpy(s_descrBuffer, s_newProject.credits.c_str());
			ImGui::Text("Credits"); ImGui::SameLine(labelWidth);
			if (ImGui::InputTextMultiline("##Credits", s_descrBuffer, MAX_DESCR_LEN))
			{
				s_newProject.credits = s_descrBuffer;
			}

			ImGui::Separator();
			if (ImGui::Button(newProject ? "Create Project" : "Save Project"))
			{
				// Actually create the project.
				project_close();
				s_curProject = s_newProject;

				bool succeeded = true;
				if (s_createDir && newProject)
				{
					sprintf(s_curProject.path, "%s/%s", s_curProject.path, s_curProject.name);
					FileUtil::fixupPath(s_curProject.path);

					if (!FileUtil::directoryExits(s_curProject.path))
					{
						FileUtil::makeDirectory(s_curProject.path);
					}
				}
				else if (newProject)
				{
					if (!FileUtil::directoryExits(s_curProject.path))
					{
						// TODO: Show message box.
						TFE_System::logWrite(LOG_ERROR, "Editor Project", "Editor Project Path '%s' does not exist, creation failed.", s_curProject.path);
						succeeded = false;
					}
				}

				if (succeeded)
				{
					project_save();
					s_curProject.active = true;
				}
				finished = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				// Do not create the project.
				finished = true;
			}
			ImGui::EndPopup();
		}

		if (!active)
		{
			finished = true;
		}
		popFont();

		return finished;
	}

	void parseProjectValue(const char* key, const char* value)
	{
		if (strcasecmp(key, "Name") == 0)
		{
			strcpy(s_curProject.name, value);
		}
		else if (strcasecmp(key, "Path") == 0)
		{
			strcpy(s_curProject.path, value);
		}
		else if (strcasecmp(key, "ProjectType") == 0)
		{
			const size_t count = TFE_ARRAYSIZE(c_projTypes);
			for (size_t i = 0; i < count; i++)
			{
				if (strcasecmp(c_projTypes[i], value) == 0)
				{
					s_curProject.type = ProjectType(i);
					break;
				}
			}
		}
		else if (strcasecmp(key, "Game") == 0)
		{
			const size_t count = TFE_ARRAYSIZE(TFE_Settings::c_gameName);
			for (size_t i = 0; i < count; i++)
			{
				if (strcasecmp(TFE_Settings::c_gameName[i], value) == 0)
				{
					s_curProject.game = GameID(i);
					break;
				}
			}
		}
		else if (strcasecmp(key, "FeatureSet") == 0)
		{
			const size_t count = TFE_ARRAYSIZE(c_featureSet);
			for (size_t i = 0; i < count; i++)
			{
				if (strcasecmp(c_featureSet[i], value) == 0)
				{
					s_curProject.featureSet = FeatureSet(i);
					break;
				}
			}
		}
		else if (strcasecmp(key, "Description") == 0 && value[0] == '{')
		{
			s_curStringOut = &s_curProject.desc;
		}
		else if (strcasecmp(key, "Authors") == 0 && value[0] == '{')
		{
			s_curStringOut = &s_curProject.authors;
		}
		else if (strcasecmp(key, "Credits") == 0 && value[0] == '{')
		{
			s_curStringOut = &s_curProject.credits;
		}
	}
}