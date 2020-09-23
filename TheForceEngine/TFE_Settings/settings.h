#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Settings
// This is a global repository for program settings in an INI like
// format.
//
// This includes reading and writing settings as well as storing an
// in-memory cache to get accessed at runtime.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>

struct TFE_Settings_Window
{
	s32 x = 0;
	s32 y = 0;
	u32 width = 1280;
	u32 height = 720;
	u32 baseWidth = 1280;
	u32 baseHeight = 720;
	bool fullscreen = false;
};

struct TFE_Settings_Graphics
{
	Vec2i gameResolution = { 320, 200 };
	bool  asyncFramebuffer = true;
};

struct TFE_Settings_Sound
{
	f32 soundFxVolume = 1.0f;
	f32 musicVolume = 1.0f;
};

struct TFE_Game
{
	char game[64] = "Dark Forces";
};

struct TFE_Settings_Game
{
	char gameName[64];
	char sourcePath[TFE_MAX_PATH];
	char emulatorPath[TFE_MAX_PATH];
};

namespace TFE_Settings
{
	bool init();
	void shutdown();

	bool writeToDisk();

	// Get and set settings.
	TFE_Settings_Window* getWindowSettings();
	TFE_Settings_Graphics* getGraphicsSettings();
	TFE_Settings_Sound* getSoundSettings();
	TFE_Game* getGame();
	TFE_Settings_Game* getGameSettings(const char* gameName);
}
