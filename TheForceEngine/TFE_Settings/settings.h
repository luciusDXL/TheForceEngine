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
#include <TFE_Audio/midiDevice.h>
#include "gameSourceData.h"

enum SkyMode
{
	SKYMODE_VANILLA = 0,
	SKYMODE_CYLINDER,
	SKYMODE_COUNT
};

static const char* c_tfeSkyModeStrings[] =
{
	"Vanilla",		// SKYMODE_VANILLA
	"Cylinder",		// SKYMODE_CYLINDER
};

struct TFE_Settings_Window
{
	s32 x = 0;
	s32 y = 64;
	u32 width = 1280;
	u32 height = 720;
	u32 baseWidth = 1280;
	u32 baseHeight = 720;
	bool fullscreen = true;
};

struct TFE_Settings_Graphics
{
	Vec2i gameResolution = { 320, 200 };
	bool  widescreen = false;
	bool  asyncFramebuffer = true;
	bool  gpuColorConvert = true;
	bool  colorCorrection = false;
	bool  perspectiveCorrectTexturing = false;
	bool  extendAjoinLimits = true;
	bool  vsync = true;
	bool  showFps = false;
	bool  fix3doNormalOverflow = true;
	bool  ignore3doLimits = true;
	s32   frameRateLimit = 0;
	f32   brightness = 1.0f;
	f32   contrast = 1.0f;
	f32   saturation = 1.0f;
	f32   gamma = 1.0f;
	s32   rendererIndex = 0;

	// Reticle
	bool reticleEnable  = false;
	s32  reticleIndex   = 6;
	f32  reticleRed     = 0.25f;
	f32  reticleGreen   = 1.00f;
	f32  reticleBlue    = 0.25f;
	f32  reticleOpacity = 1.00f;
	f32  reticleScale   = 1.0f;

	// Sky (Ignored when using the software renderer)
	s32  skyMode = SKYMODE_CYLINDER;
};

enum TFE_HudScale
{
	TFE_HUDSCALE_PROPORTIONAL = 0,
	TFE_HUDSCALE_SCALED,
};

enum TFE_HudPosition
{
	TFE_HUDPOS_EDGE = 0,	// Hud elements on the edges of the screen.
	TFE_HUDPOS_4_3,			// Hud elements locked to 4:3 (even in widescreen).
};

enum PitchLimit
{
	PITCH_VANILLA = 0,
	PITCH_VANILLA_PLUS,
	PITCH_HIGH,
	PITCH_MAXIMUM,
	PITCH_COUNT
};

enum FontSize
{
	FONT_SMALL,
	FONT_MEDIUM,
	FONT_LARGE
};

static const char* c_tfeHudScaleStrings[] =
{
	"Proportional",		// TFE_HUDSCALE_PROPORTIONAL
	"Scaled",			// TFE_HUDSCALE_SCALED
};

static const char* c_tfeHudPosStrings[] =
{
	"Edge",		// TFE_HUDPOS_EDGE
	"4:3",		// TFE_HUDPOS_4_3
};

static const char* c_tfePitchLimit[] =
{
	"Vanilla  (45 degrees)",
	"Vanilla+ (60 degrees)",
	"High     (75 degrees)",
	"Maximum"
};

struct TFE_Settings_Hud
{
	// Determines whether the HUD stays the same size on screen regardless of resolution or if it gets smaller with higher resolution.
	TFE_HudScale hudScale = TFE_HUDSCALE_PROPORTIONAL;
	// This setting determines how the left and right corners are calculated, which have an offset of (0,0).
	TFE_HudPosition hudPos = TFE_HUDPOS_EDGE;

	// Scale of the HUD, ignored if HudScale is TFE_HUDSCALE_PROPORTIONAL.
	f32 scale = 1.0f;
	// Pixel offset from the left hud corner, right is (-leftX, leftY)
	s32 pixelOffset[3] = { 0 };
};

struct TFE_Settings_Sound
{
	f32 masterVolume = 1.0f;
	f32 soundFxVolume = 0.75f;
	f32 musicVolume = 1.0f;
	f32 cutsceneSoundFxVolume = 0.9f;
	f32 cutsceneMusicVolume = 1.0f;
	s32 audioDevice = -1;			// Use the audio device default.
	s32 midiOutput  = -1;			// Use the midi type default.
	s32 midiType = MIDI_TYPE_DEFAULT;
	bool use16Channels = false;
	bool disableSoundInMenus = false;
};

struct TFE_Game
{
	char game[64] = "Dark Forces";
	GameID id;
};

struct TFE_GameHeader
{
	char gameName[64]="";
	char sourcePath[TFE_MAX_PATH]="";
	char emulatorPath[TFE_MAX_PATH]="";
};

struct TFE_Settings_Game
{
	TFE_GameHeader header[Game_Count];

	// Dark Forces
	s32  df_airControl = 0;				// Air control, default = 0, where 0 = speed/256 and 8 = speed; range = [0, 8]
	bool df_bobaFettFacePlayer = false;	// Make Boba Fett try to face the player in all his attack phases.
	bool df_disableFightMusic  = false;	// Set to true to disable fight music and music transitions during gameplay.
	bool df_enableAutoaim      = true;  // Set to true to enable autoaim, false to disable.
	bool df_showSecretFoundMsg = true;  // Show a message when the player finds a secret.
	bool df_autorun = false;			// Run by default instead of walk.
	bool df_ignoreInfLimit = true;		// Ignore the vanilla INF limit.
	PitchLimit df_pitchLimit  = PITCH_VANILLA_PLUS;
};

struct TFE_Settings_System
{
	bool gameQuitExitsToMenu = true;	// Quitting from the game returns to the main menu instead.
	bool returnToModLoader = true;		// Return to the Mod Loader if running a mod.
};

struct TFE_Settings_A11y 
{
	bool showCutsceneSubtitles; //voice
	bool showCutsceneCaptions; //descriptive (e.g. "[Mine beeping]", "[Engine roaring]"
	bool showGameplaySubtitles; //voice
	bool showGameplayCaptions; //descriptive
	FontSize cutsceneFontSize;
	FontSize gameplayFontSize;
};

namespace TFE_Settings
{
	bool init(bool& firstRun);
	void shutdown();

	bool writeToDisk();

	// Get and set settings.
	TFE_Settings_Window* getWindowSettings();
	TFE_Settings_Graphics* getGraphicsSettings();
	TFE_Settings_Hud* getHudSettings();
	TFE_Settings_Sound* getSoundSettings();
	TFE_Settings_System* getSystemSettings();
	TFE_Game* getGame();
	TFE_GameHeader* getGameHeader(const char* gameName);
	TFE_Settings_Game* getGameSettings();
	TFE_Settings_A11y* getA11ySettings();

	bool validatePath(const char* path, const char* sentinel);
	void autodetectGamePaths();
}
