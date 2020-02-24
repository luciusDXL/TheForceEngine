#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Settings
// This is a global repository for program settings in an INI like
// format.
//
// This includes reading and writing settings as well as storing an
// in-memory cache to get accessed at runtime.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>

struct DXL2_Settings_Window
{
	s32 x = 0;
	s32 y = 0;
	u32 width = 1280;
	u32 height = 720;
	u32 baseWidth = 1280;
	u32 baseHeight = 720;
	bool fullscreen = false;
};

namespace DXL2_Settings
{
	bool init();
	void shutdown();

	bool writeToDisk();

	// Get and set settings.
	DXL2_Settings_Window* getWindowSettings();
}
