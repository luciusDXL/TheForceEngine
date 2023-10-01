#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <vector>

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
		TEXTCLR_ERROR,
		TEXTCLR_WARNING,
		TEXTCLR_SPECIAL,
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
}
