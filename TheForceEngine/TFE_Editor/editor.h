#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Archive/archive.h>
#include <TFE_Game/igame.h>
#include <vector>

struct ImVec4;

namespace TFE_Editor
{
	typedef std::vector<u8> WorkBuffer;
	enum FontType
	{
		FONT_SMALL = 0,
		FONT_COUNT,
	};

	enum EditorTextColor
	{
		TEXTCLR_NORMAL = 0,
		TEXTCLR_TITLE_ACTIVE,
		TEXTCLR_TITLE_INACTIVE,
		TEXTCLR_COUNT
	};

	void enable();
	void disable();
	bool update(bool consoleOpen = false);
	bool render();

	void pushFont(FontType type);
	void popFont();

	void showMessageBox(const char* type, const char* msg, ...);

	// Resizable temporary memory.
	WorkBuffer& getWorkBuffer();
	ArchiveType getArchiveType(const char* filename);
	Archive* getArchive(const char* name, GameID gameId);
	void getTempDirectory(char* tmpDir);

	ImVec4 getTextColor(EditorTextColor color);
}
