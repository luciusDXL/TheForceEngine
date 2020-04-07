#include "helpWindow.h"
#include <DXL2_Ui/imGUI/imgui.h>
#include <DXL2_Ui/markdown.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_FileSystem/filestream.h>

#include <vector>
#include <string>

namespace HelpWindow
{
	static const char* s_displayStr;
	static std::vector<std::string> s_helpNames;
	static std::vector<std::string> s_helpStrings;
	static Help s_curHelp;
	static bool s_minimized = false;
	static bool s_open = false;
	static bool s_resetScroll = false;

	const char* c_helpNameEntrypoints[]=
	{
		"About",	// HELP_ABOUT
		"Manual"	// HELP_MANUAL
	};

	void openLocalLink(const char* link)
	{
		s_resetScroll = true;

		// Search the help names until we find a match.
		const size_t helpCount = s_helpNames.size();
		for (size_t i = 0; i < helpCount; i++)
		{
			if (strcasecmp(link, s_helpNames[i].c_str()) == 0)
			{
				s_displayStr = s_helpStrings[i].c_str();
				return;
			}
		}

		// The document has not been loaded yet.
		char path[DXL2_MAX_PATH];
		char fileName[DXL2_MAX_PATH];
		sprintf(fileName, "Documentation/markdown/%s.md", link);
		DXL2_Paths::appendPath(PATH_PROGRAM, fileName, path);

		FileStream file;
		if (file.open(path, FileStream::MODE_READ))
		{
			std::string text;

			const size_t len = file.getSize();
			text.resize(len + 1);
			file.readBuffer(&text[0], (u32)len);
			text[len] = 0;

			file.close();

			s_helpNames.push_back(link);
			s_helpStrings.push_back(text);

			s_displayStr = s_helpStrings.back().c_str();
		}
	}

	void init()
	{
		DXL2_Markdown::registerLocalLinkCB(openLocalLink);
		s_displayStr = nullptr;
		s_curHelp = HELP_NONE;
	}

	bool show(Help help, u32 width, u32 height)
	{
		if (help == HELP_NONE) { return false; }
		if (help != s_curHelp)
		{
			openLocalLink(c_helpNameEntrypoints[help]);
			s_curHelp = help;
		}

		s_open = true;
		
		ImGui::SetNextWindowSize({ (f32)width, (f32)height });
		if (ImGui::Begin(c_helpNameEntrypoints[help], &s_open))
		{
			if (s_resetScroll)
			{
				ImGui::SetScrollY(0.0f);
				s_resetScroll = false;
			}

			DXL2_Markdown::draw(s_displayStr);
			s_minimized = false;
		}
		else
		{
			s_minimized = true;
		}
		ImGui::End();
		
		if (!s_open) { s_curHelp = HELP_NONE; s_minimized = false; }
		return s_open;
	}

	bool isMinimized()
	{
		return s_minimized;
	}

	bool isOpen()
	{
		return s_open;
	}
}
