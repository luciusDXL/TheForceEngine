#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Settings
// This is a global repository for program settings in an INI like
// format.
//
// This includes reading and writing settings as well as storing an
// in-memory cache to get accessed at runtime.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_FileSystem/paths.h>

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

struct DXL2_Settings_Graphics
{
	Vec2i gameResolution = { 320, 200 };
};

struct DXL2_Game
{
	char game[64] = "Dark Forces";
};

struct DXL2_Settings_Game
{
	char gameName[64];
	char sourcePath[DXL2_MAX_PATH];
	char emulatorPath[DXL2_MAX_PATH];
};

namespace DXL2_Settings
{
	bool init();
	void shutdown();

	bool writeToDisk();

	// Get and set settings.
	DXL2_Settings_Window* getWindowSettings();
	DXL2_Settings_Graphics* getGraphicsSettings();
	DXL2_Game* getGame();
	DXL2_Settings_Game* getGameSettings(const char* gameName);
}
