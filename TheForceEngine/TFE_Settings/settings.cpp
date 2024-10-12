#include "settings.h"
#include "gameSourceData.h"
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FrontEndUI/console.h>
#include <TFE_System/cJSON.h>
#include <TFE_System/parser.h>
#include <TFE_System/system.h>
#include <TFE_A11y/accessibility.h>
#include <TFE_Ui/imGUI/imgui.h>
#include <assert.h>
#include <algorithm>
#include <vector>

#ifdef _WIN32
#include "windows/registry.h"
#endif

#ifdef __linux__
#include "linux/steamlnx.h"
#endif

using namespace TFE_IniParser;

namespace TFE_Settings
{
	//////////////////////////////////////////////////////////////////////////////////
	// Local State
	//////////////////////////////////////////////////////////////////////////////////
	static char s_settingsPath[TFE_MAX_PATH];
	static TFE_Settings_Window s_windowSettings = {};
	static TFE_Settings_Graphics s_graphicsSettings = {};
	static TFE_Settings_Enhancements s_enhancementsSettings = {};
	static TFE_Settings_Hud s_hudSettings = {};
	static TFE_Settings_Sound s_soundSettings = {};
	static TFE_Settings_System s_systemSettings = {};
	static TFE_Settings_Temp s_tempSettings = {};
	static TFE_Settings_A11y s_a11ySettings = {};
	static TFE_Game s_game = {};
	static TFE_Settings_Game s_gameSettings = {};
	static TFE_ModSettings s_modSettings = {};
	static std::vector<char> s_iniBuffer;

	enum SectionID
	{
		SECTION_WINDOW = 0,
		SECTION_GRAPHICS,
		SECTION_ENHANCEMENTS,
		SECTION_HUD,
		SECTION_SOUND,
		SECTION_SYSTEM,
		SECTION_A11Y, //accessibility
		SECTION_GAME,
		SECTION_DARK_FORCES,
		SECTION_OUTLAWS,
		SECTION_CVAR,
		SECTION_COUNT,
		SECTION_INVALID = SECTION_COUNT,
		SECTION_GAME_START = SECTION_DARK_FORCES
	};

	static const char* c_sectionNames[SECTION_COUNT] =
	{
		"Window",
		"Graphics",
		"Enhancements",
		"Hud",
		"Sound",
		"System",
		"A11y",
		"Game",
		"Dark_Forces",
		"Outlaws",
		"CVar",
	};

	//////////////////////////////////////////////////////////////////////////////////
	// Forward Declarations
	//////////////////////////////////////////////////////////////////////////////////
	bool settingsFileEmpty();

	// Write
	void writeWindowSettings(FileStream& settings);
	void writeGraphicsSettings(FileStream& settings);
	void writeEnhancementsSettings(FileStream& settings);
	void writeHudSettings(FileStream& settings);
	void writeSoundSettings(FileStream& settings);
	void writeSystemSettings(FileStream& settings);
	void writeA11ySettings(FileStream& settings);
	void writeGameSettings(FileStream& settings);
	void writePerGameSettings(FileStream& settings);
	void writeCVars(FileStream& settings);

	// Read
	bool readFromDisk();
	void parseIniFile(const char* buffer, size_t len);
	void parseWindowSettings(const char* key, const char* value);
	void parseGraphicsSettings(const char* key, const char* value);
	void parseEnhancementsSettings(const char* key, const char* value);
	void parseHudSettings(const char* key, const char* value);
	void parseSoundSettings(const char* key, const char* value);
	void parseSystemSettings(const char* key, const char* value);
	void parseA11ySettings(const char* key, const char* value);
	void parseGame(const char* key, const char* value);
	void parseDark_ForcesSettings(const char* key, const char* value);
	void parseOutlawsSettings(const char* key, const char* value);
	void parseCVars(const char* key, const char* value);
	void checkGameData();

	//////////////////////////////////////////////////////////////////////////////////
	// Implementation
	//////////////////////////////////////////////////////////////////////////////////
	bool init(bool& firstRun)
	{
		// Clear out game settings.
		s_gameSettings = {};
		for (u32 i = 0; i < Game_Count; i++)
		{
			strcpy(s_gameSettings.header[i].gameName, c_gameName[i]);
		}
		strcpy(s_game.game, s_gameSettings.header[0].gameName);

		TFE_Paths::appendPath(PATH_USER_DOCUMENTS, "settings.ini", s_settingsPath);
		if (FileUtil::exists(s_settingsPath))
		{
			// This is still the first run if the settings.ini file is empty.
			firstRun = settingsFileEmpty();

			if (readFromDisk())
			{
				return true;
			}
			firstRun = false;
			TFE_System::logWrite(LOG_WARNING, "Settings", "Cannot parse 'settings.ini' - recreating it.");			
		}

		firstRun = true;
		autodetectGamePaths();
		return writeToDisk();
	}

	void shutdown()
	{
		// Write any settings to disk before shutting down.
		writeToDisk();
	}

	void autodetectGamePaths()
	{
		for (u32 gameId = 0; gameId < Game_Count; gameId++)
		{
			const size_t sourcePathLen = strlen(s_gameSettings.header[gameId].sourcePath);
			bool pathValid = sourcePathLen && validatePath(s_gameSettings.header[gameId].sourcePath, c_validationFile[gameId]);

			// First check locally, and then check the registry.
			if (!pathValid && gameId == Game_Dark_Forces)	// Only Dark Forces for now.
			{
				// First try the local path.
				char localPath[TFE_MAX_PATH];
				TFE_Paths::appendPath(PATH_PROGRAM, "DARK.GOB", localPath);

				FileStream file;
				if (file.open(localPath, Stream::MODE_READ))
				{
					if (file.getSize() > 1)
					{
						strcpy(s_gameSettings.header[gameId].sourcePath, TFE_Paths::getPath(PATH_PROGRAM));
						FileUtil::fixupPath(s_gameSettings.header[gameId].sourcePath);
						pathValid = true;
					}
					file.close();
				}

				// Then try local/Games/Dark Forces/
				if (!pathValid)
				{
					char gamePath[TFE_MAX_PATH];
					sprintf(gamePath, "%sGames/Dark Forces/", TFE_Paths::getPath(PATH_PROGRAM));
					FileUtil::fixupPath(gamePath);

					sprintf(localPath, "%sDARK.GOB", gamePath);
					if (file.open(localPath, Stream::MODE_READ))
					{
						if (file.getSize() > 1)
						{
							strcpy(s_gameSettings.header[gameId].sourcePath, gamePath);
							pathValid = true;
						}
						file.close();
					}
				}
			}

#ifdef _WIN32
			// Next try looking through the registry.
			if (!pathValid)
			{
				// Remaster - search here first so that new assets are readily available.
				pathValid = WindowsRegistry::getSteamPathFromRegistry(c_steamRemasterProductId[gameId], c_steamRemasterLocalPath[gameId], c_steamRemasterLocalSubPath[gameId], c_validationFile[gameId], s_gameSettings.header[gameId].sourcePath);
				// Remaster on GOG.
				if (!pathValid)
				{
					pathValid = WindowsRegistry::getGogPathFromRegistry(c_gogRemasterProductId[gameId], c_validationFile[gameId], s_gameSettings.header[gameId].sourcePath);
				}

				// Then try the vanilla version on Steam.
				if (!pathValid)
				{
					pathValid = WindowsRegistry::getSteamPathFromRegistry(c_steamProductId[gameId], c_steamLocalPath[gameId], c_steamLocalSubPath[gameId], c_validationFile[gameId], s_gameSettings.header[gameId].sourcePath);
				}
				// And the vanilla version on GOG.
				if (!pathValid)
				{
					pathValid = WindowsRegistry::getGogPathFromRegistry(c_gogProductId[gameId], c_validationFile[gameId], s_gameSettings.header[gameId].sourcePath);
				}
			}
#endif
#ifdef __linux__
			if (!pathValid)
			{
				pathValid = LinuxSteam::getSteamPath(c_steamRemasterProductId[gameId], c_steamRemasterLocalPath[gameId],
					c_validationFile[gameId], s_gameSettings.header[gameId].sourcePath);

				if (!pathValid)
				{
					pathValid = LinuxSteam::getSteamPath(c_steamProductId[gameId], c_steamLocalSubPath[gameId],
						c_validationFile[gameId], s_gameSettings.header[gameId].sourcePath);
				}
			}
#endif
			// If the registry approach fails, just try looking in the various hardcoded paths.
			if (!pathValid)
			{
				// Try various possible locations.
				const char* const * locations = c_gameLocations[gameId];
				for (u32 i = 0; i < c_hardcodedPathCount[gameId]; i++)
				{
					if (FileUtil::directoryExits(locations[i]))
					{
						strcpy(s_gameSettings.header[gameId].sourcePath, locations[i]);
						pathValid = true;
						break;
					}
				}
			}
		}
		writeToDisk();
	}

	bool settingsFileEmpty()
	{
		bool isEmpty = true;
		FileStream settings;
		if (settings.open(s_settingsPath, Stream::MODE_READ))
		{
			isEmpty = settings.getSize() == 0u;
			settings.close();
		}
		return isEmpty;
	}

	bool readFromDisk()
	{
		FileStream settings;
		if (settings.open(s_settingsPath, Stream::MODE_READ))
		{
			const size_t len = settings.getSize();
			s_iniBuffer.resize(len + 1);
			s_iniBuffer[len] = 0;
			settings.readBuffer(s_iniBuffer.data(), (u32)len);
			settings.close();

			parseIniFile(s_iniBuffer.data(), len);
			autodetectGamePaths();

			return true;
		}
		return false;
	}

	bool writeToDisk()
	{
		FileStream settings;
		if (settings.open(s_settingsPath, Stream::MODE_WRITE))
		{
			writeWindowSettings(settings);
			writeGraphicsSettings(settings);
			writeEnhancementsSettings(settings);
			writeHudSettings(settings);
			writeSoundSettings(settings);
			writeSystemSettings(settings);
			writeA11ySettings(settings);
			writeGameSettings(settings);
			writePerGameSettings(settings);
			writeCVars(settings);
			settings.close();

			return true;
		}
		char msgBuffer[4096];
		sprintf(msgBuffer, "Cannot write 'settings.ini' to '%s',\nmost likely Documents/ has been set to read-only, is located on One-Drive (currently not supported), or has been added as a Controlled Folder if running on Windows.\n https://www.tenforums.com/tutorials/87858-add-protected-folders-controlled-folder-access-windows-10-a.html", s_settingsPath);
		TFE_System::postErrorMessageBox(msgBuffer, "Permissions Error");
		return false;
	}

	// Get and set settings.
	TFE_Settings_Window* getWindowSettings()
	{
		return &s_windowSettings;
	}

	TFE_Settings_Graphics* getGraphicsSettings()
	{
		return &s_graphicsSettings;
	}

	TFE_Settings_Enhancements* getEnhancementsSettings()
	{
		return &s_enhancementsSettings;
	}

	TFE_Settings_Hud* getHudSettings()
	{
		return &s_hudSettings;
	}

	TFE_Settings_Sound* getSoundSettings()
	{
		return &s_soundSettings;
	}

	TFE_Settings_System* getSystemSettings()
	{
		return &s_systemSettings;
	}

	TFE_Settings_Temp* getTempSettings()
	{
		return &s_tempSettings;
	}

	TFE_Settings_A11y* getA11ySettings()
	{
		return &s_a11ySettings;
	}

	TFE_ModSettings* getModSettings()
	{
		return &s_modSettings;
	}

	void clearModSettings()
	{
		s_modSettings = {};
	}

	TFE_Game* getGame()
	{
		return &s_game;
	}

	TFE_GameHeader* getGameHeader(const char* gameName)
	{
		for (u32 i = 0; i < Game_Count; i++)
		{
			if (strcasecmp(gameName, c_gameName[i]) == 0)
			{
				return &s_gameSettings.header[i];
			}
		}

		return nullptr;
	}

	TFE_Settings_Game* getGameSettings()
	{
		return &s_gameSettings;
	}

	void writeWindowSettings(FileStream& settings)
	{
		writeHeader(settings, c_sectionNames[SECTION_WINDOW]);
		writeKeyValue_Int(settings, "x", s_windowSettings.x);
		writeKeyValue_Int(settings, "y", s_windowSettings.y);
		writeKeyValue_Int(settings, "width", s_windowSettings.width);
		writeKeyValue_Int(settings, "height", s_windowSettings.height);
		writeKeyValue_Int(settings, "baseWidth", s_windowSettings.baseWidth);
		writeKeyValue_Int(settings, "baseHeight", s_windowSettings.baseHeight);
		writeKeyValue_Bool(settings, "fullscreen", s_windowSettings.fullscreen);
	}

	void writeGraphicsSettings(FileStream& settings)
	{
		writeHeader(settings, c_sectionNames[SECTION_GRAPHICS]);
		writeKeyValue_Int(settings, "gameWidth", s_graphicsSettings.gameResolution.x);
		writeKeyValue_Int(settings, "gameHeight", s_graphicsSettings.gameResolution.z);
		writeKeyValue_Int(settings, "fov", s_graphicsSettings.fov);
		writeKeyValue_Bool(settings, "widescreen", s_graphicsSettings.widescreen);
		writeKeyValue_Bool(settings, "asyncFramebuffer", s_graphicsSettings.asyncFramebuffer);
		writeKeyValue_Bool(settings, "gpuColorConvert", s_graphicsSettings.gpuColorConvert);
		writeKeyValue_Bool(settings, "colorCorrection", s_graphicsSettings.colorCorrection);
		writeKeyValue_Bool(settings, "perspectiveCorrect3DO", s_graphicsSettings.perspectiveCorrectTexturing);
		writeKeyValue_Bool(settings, "extendAjoinLimits", s_graphicsSettings.extendAjoinLimits);
		writeKeyValue_Bool(settings, "vsync", s_graphicsSettings.vsync);
		writeKeyValue_Bool(settings, "show_fps", s_graphicsSettings.showFps);
		writeKeyValue_Bool(settings, "3doNormalFix", s_graphicsSettings.fix3doNormalOverflow);
		writeKeyValue_Bool(settings, "ignore3doLimits", s_graphicsSettings.ignore3doLimits);
		writeKeyValue_Bool(settings, "ditheredBilinear", s_graphicsSettings.ditheredBilinear);

		writeKeyValue_Bool(settings, "useBilinear", s_graphicsSettings.useBilinear);
		writeKeyValue_Bool(settings, "useMipmapping", s_graphicsSettings.useMipmapping);
		writeKeyValue_Float(settings, "bilinearSharpness", s_graphicsSettings.bilinearSharpness);
		writeKeyValue_Float(settings, "anisotropyQuality", s_graphicsSettings.anisotropyQuality);

		writeKeyValue_Int(settings, "frameRateLimit", s_graphicsSettings.frameRateLimit);
		writeKeyValue_Float(settings, "brightness", s_graphicsSettings.brightness);
		writeKeyValue_Float(settings, "contrast", s_graphicsSettings.contrast);
		writeKeyValue_Float(settings, "saturation", s_graphicsSettings.saturation);
		writeKeyValue_Float(settings, "gamma", s_graphicsSettings.gamma);

		writeKeyValue_Bool(settings, "reticleEnable", s_graphicsSettings.reticleEnable);
		writeKeyValue_Int(settings, "reticleIndex", s_graphicsSettings.reticleIndex);
		writeKeyValue_Float(settings, "reticleRed", s_graphicsSettings.reticleRed);
		writeKeyValue_Float(settings, "reticleGreen", s_graphicsSettings.reticleGreen);
		writeKeyValue_Float(settings, "reticleBlue", s_graphicsSettings.reticleBlue);
		writeKeyValue_Float(settings, "reticleOpacity", s_graphicsSettings.reticleOpacity);
		writeKeyValue_Float(settings, "reticleScale", s_graphicsSettings.reticleScale);

		writeKeyValue_Bool(settings, "bloomEnabled", s_graphicsSettings.bloomEnabled);
		writeKeyValue_Float(settings, "bloomStrength", s_graphicsSettings.bloomStrength);
		writeKeyValue_Float(settings, "bloomSpread", s_graphicsSettings.bloomSpread);

		writeKeyValue_Int(settings, "renderer", s_graphicsSettings.rendererIndex);
		writeKeyValue_Int(settings, "colorMode", s_graphicsSettings.colorMode);
		writeKeyValue_Int(settings, "skyMode", s_graphicsSettings.skyMode);
	}

	void writeEnhancementsSettings(FileStream& settings)
	{
		writeHeader(settings, c_sectionNames[SECTION_ENHANCEMENTS]);
		writeKeyValue_Int(settings, "hdTextures", s_enhancementsSettings.enableHdTextures);
		writeKeyValue_Int(settings, "hdSprites", s_enhancementsSettings.enableHdSprites);
		writeKeyValue_Int(settings, "hdHud", s_enhancementsSettings.enableHdHud);
	}

	void writeHudSettings(FileStream& settings)
	{
		writeHeader(settings, c_sectionNames[SECTION_HUD]);
		writeKeyValue_String(settings, "hudScale", c_tfeHudScaleStrings[s_hudSettings.hudScale]);
		writeKeyValue_String(settings, "hudPos", c_tfeHudPosStrings[s_hudSettings.hudPos]);
		writeKeyValue_Float(settings, "scale", s_hudSettings.scale);
		writeKeyValue_Int(settings, "pixelOffsetLeft", s_hudSettings.pixelOffset[0]);
		writeKeyValue_Int(settings, "pixelOffsetRight", s_hudSettings.pixelOffset[1]);
		writeKeyValue_Int(settings, "pixelOffsetY", s_hudSettings.pixelOffset[2]);
	}

	void writeSoundSettings(FileStream& settings)
	{
		writeHeader(settings, c_sectionNames[SECTION_SOUND]);
		writeKeyValue_Float(settings, "masterVolume", s_soundSettings.masterVolume);
		writeKeyValue_Float(settings, "soundFxVolume", s_soundSettings.soundFxVolume);
		writeKeyValue_Float(settings, "musicVolume", s_soundSettings.musicVolume);
		writeKeyValue_Float(settings, "cutsceneSoundFxVolume", s_soundSettings.cutsceneSoundFxVolume);
		writeKeyValue_Float(settings, "cutsceneMusicVolume", s_soundSettings.cutsceneMusicVolume);
		writeKeyValue_Int(settings, "audioDevice", s_soundSettings.audioDevice);
		writeKeyValue_Int(settings, "midiOutput", s_soundSettings.midiOutput);
		writeKeyValue_Int(settings, "midiType", s_soundSettings.midiType);
		writeKeyValue_Bool(settings, "use16Channels", s_soundSettings.use16Channels);
		writeKeyValue_Bool(settings, "disableSoundInMenus", s_soundSettings.disableSoundInMenus);
	}

	void writeSystemSettings(FileStream& settings)
	{
		writeHeader(settings, c_sectionNames[SECTION_SYSTEM]);
		writeKeyValue_Bool(settings, "gameExitsToMenu", s_systemSettings.gameQuitExitsToMenu);
		writeKeyValue_Bool(settings, "returnToModLoader", s_systemSettings.returnToModLoader);
		writeKeyValue_Float(settings, "gifRecordingFramerate", s_systemSettings.gifRecordingFramerate);
	}

	void writeA11ySettings(FileStream& settings)
	{
		writeHeader(settings, c_sectionNames[SECTION_A11Y]);
		writeKeyValue_String(settings, "language", s_a11ySettings.language.c_str());
		writeKeyValue_String(settings, "lastFontPath", s_a11ySettings.lastFontPath.c_str());

		writeKeyValue_Bool(settings, "showCutsceneSubtitles", s_a11ySettings.showCutsceneSubtitles);
		writeKeyValue_Bool(settings, "showCutsceneCaptions", s_a11ySettings.showCutsceneCaptions);
		writeKeyValue_Int(settings, "cutsceneFontSize", s_a11ySettings.cutsceneFontSize);
		writeKeyValue_RGBA(settings, "cutsceneFontColor", s_a11ySettings.cutsceneFontColor);
		writeKeyValue_Float(settings, "cutsceneTextBackgroundAlpha", s_a11ySettings.cutsceneTextBackgroundAlpha);
		writeKeyValue_Bool(settings, "showCutsceneTextBorder", s_a11ySettings.showCutsceneTextBorder);
		writeKeyValue_Float(settings, "cutsceneTextSpeed", s_a11ySettings.cutsceneTextSpeed);

		writeKeyValue_Bool(settings, "showGameplaySubtitles", s_a11ySettings.showGameplaySubtitles);
		writeKeyValue_Bool(settings, "showGameplayCaptions", s_a11ySettings.showGameplayCaptions);
		writeKeyValue_Int(settings, "gameplayFontSize", s_a11ySettings.gameplayFontSize);
		writeKeyValue_RGBA(settings, "gameplayFontColor", s_a11ySettings.gameplayFontColor);
		writeKeyValue_Float(settings, "gameplayTextBackgroundAlpha", s_a11ySettings.gameplayTextBackgroundAlpha);
		writeKeyValue_Bool(settings, "showGameplayTextBorder", s_a11ySettings.showGameplayTextBorder);
		writeKeyValue_Int(settings, "gameplayMaxTextLines", s_a11ySettings.gameplayMaxTextLines);
		writeKeyValue_Float(settings, "gameplayTextSpeed", s_a11ySettings.gameplayTextSpeed);
		writeKeyValue_Int(settings, "gameplayCaptionMinVolume", s_a11ySettings.gameplayCaptionMinVolume);

		writeKeyValue_Bool(settings, "enableHeadwave", s_a11ySettings.enableHeadwave);
		writeKeyValue_Bool(settings, "disableScreenFlashes", s_a11ySettings.disableScreenFlashes);
		writeKeyValue_Bool(settings, "disablePlayerWeaponLighting", s_a11ySettings.disablePlayerWeaponLighting);
	}

	void writeGameSettings(FileStream& settings)
	{
		writeHeader(settings, c_sectionNames[SECTION_GAME]);
		writeKeyValue_String(settings, "game", s_game.game);
	}

	void writePerGameSettings(FileStream& settings)
	{
		for (u32 i = 0; i < Game_Count; i++)
		{
			writeHeader(settings, c_sectionNames[SECTION_GAME_START + i]);
			writeKeyValue_String(settings, "sourcePath", s_gameSettings.header[i].sourcePath);
			writeKeyValue_String(settings, "emulatorPath", s_gameSettings.header[i].emulatorPath);

			if (i == Game_Dark_Forces)
			{
				writeKeyValue_Int(settings, "airControl", s_gameSettings.df_airControl);
				writeKeyValue_Bool(settings, "bobaFettFacePlayer", s_gameSettings.df_bobaFettFacePlayer);
				writeKeyValue_Bool(settings, "smoothVUEs", s_gameSettings.df_smoothVUEs);
				writeKeyValue_Bool(settings, "disableFightMusic", s_gameSettings.df_disableFightMusic);
				writeKeyValue_Bool(settings, "enableAutoaim", s_gameSettings.df_enableAutoaim);
				writeKeyValue_Bool(settings, "showSecretFoundMsg", s_gameSettings.df_showSecretFoundMsg);
				writeKeyValue_Bool(settings, "autorun", s_gameSettings.df_autorun);
				writeKeyValue_Bool(settings, "crouchToggle", s_gameSettings.df_crouchToggle);
				writeKeyValue_Bool(settings, "ignoreInfLimit", s_gameSettings.df_ignoreInfLimit);
				writeKeyValue_Bool(settings, "stepSecondAlt", s_gameSettings.df_stepSecondAlt);
				writeKeyValue_Int(settings, "pitchLimit", s_gameSettings.df_pitchLimit);
				writeKeyValue_Bool(settings, "solidWallFlagFix", s_gameSettings.df_solidWallFlagFix);
			}
		}
	}

	void writeCVars(FileStream& settings)
	{
		writeHeader(settings, c_sectionNames[SECTION_CVAR]);
		u32 count = TFE_Console::getCVarCount();
		char value[256];
		for (u32 i = 0; i < count; i++)
		{
			const TFE_Console::CVar* cvar = TFE_Console::getCVarByIndex(i);
			if (cvar->flags & CVFLAG_DO_NOT_SERIALIZE)
			{
				continue;
			}

			switch (cvar->type)
			{
			case TFE_Console::CVAR_INT:
				sprintf(value, "int %d", cvar->valueInt ? (*cvar->valueInt) : cvar->serializedInt);
				break;
			case TFE_Console::CVAR_FLOAT:
				sprintf(value, "float %f", cvar->valueFloat ? (*cvar->valueFloat) : cvar->serializedFlt);
				break;
			case TFE_Console::CVAR_BOOL:
				sprintf(value, "bool %s", cvar->valueBool ? ((*cvar->valueBool) ? "true" : "false") : (cvar->serializedBool ? "true" : "false"));
				break;
			case TFE_Console::CVAR_STRING:
				sprintf(value, "string \"%s\"", cvar->valueString ? cvar->valueString : cvar->serializedString.c_str());
				break;
			default:
				continue;
			};

			writeKeyValue_String(settings, cvar->name.c_str(), value);
		}
	}

	SectionID parseSectionName(const char* name)
	{
		char sectionName[LINEBUF_LEN];
		memcpy(sectionName, name + 1, strlen(name) - 2);
		sectionName[strlen(name) - 2] = 0;

		for (u32 i = 0; i < SECTION_COUNT; i++)
		{
			if (strcasecmp(c_sectionNames[i], sectionName) == 0)
			{
				return SectionID(i);
			}
		}
		return SECTION_INVALID;
	}

	void parseIniFile(const char* buffer, size_t len)
	{
		TFE_Parser parser;
		parser.init(buffer, len);
		parser.addCommentString(";");
		parser.addCommentString("#");

		size_t bufferPos = 0;
		SectionID curSection = SECTION_INVALID;

		while (bufferPos < len)
		{
			const char* line = parser.readLine(bufferPos);
			if (!line) { break; }

			TokenList tokens;
			parser.tokenizeLine(line, tokens);
			if (tokens.size() < 1) { continue; }

			if (tokens.size() == 1)
			{
				curSection = parseSectionName(tokens[0].c_str());
				if (curSection == SECTION_INVALID)
				{
					continue;
				}
			}
			else if (tokens.size() == 2)
			{
				switch (curSection)
				{
				case SECTION_WINDOW:
					parseWindowSettings(tokens[0].c_str(), tokens[1].c_str());
					break;
				case SECTION_GRAPHICS:
					parseGraphicsSettings(tokens[0].c_str(), tokens[1].c_str());
					break;
				case SECTION_ENHANCEMENTS:
					parseEnhancementsSettings(tokens[0].c_str(), tokens[1].c_str());
					break;
				case SECTION_HUD:
					parseHudSettings(tokens[0].c_str(), tokens[1].c_str());
					break;
				case SECTION_SOUND:
					parseSoundSettings(tokens[0].c_str(), tokens[1].c_str());
					break;
				case SECTION_SYSTEM:
					parseSystemSettings(tokens[0].c_str(), tokens[1].c_str());
					break;
				case SECTION_A11Y:
					parseA11ySettings(tokens[0].c_str(), tokens[1].c_str());
					break;
				case SECTION_GAME:
					parseGame(tokens[0].c_str(), tokens[1].c_str());
					break;
				case SECTION_DARK_FORCES:
					parseDark_ForcesSettings(tokens[0].c_str(), tokens[1].c_str());
					break;
				case SECTION_OUTLAWS:
					parseOutlawsSettings(tokens[0].c_str(), tokens[1].c_str());
					break;
				case SECTION_CVAR:
					parseCVars(tokens[0].c_str(), tokens[1].c_str());
					break;
				default:
					assert(0);
				};
			}
		}
	}

	void parseWindowSettings(const char* key, const char* value)
	{
		if (strcasecmp("x", key) == 0)
		{
			s_windowSettings.x = parseInt(value);
		}
		else if (strcasecmp("y", key) == 0)
		{
			s_windowSettings.y = parseInt(value);
		}
		else if (strcasecmp("width", key) == 0)
		{
			s_windowSettings.width = parseInt(value);
		}
		else if (strcasecmp("height", key) == 0)
		{
			s_windowSettings.height = parseInt(value);
		}
		else if (strcasecmp("baseWidth", key) == 0)
		{
			s_windowSettings.baseWidth = parseInt(value);
		}
		else if (strcasecmp("baseHeight", key) == 0)
		{
			s_windowSettings.baseHeight = parseInt(value);
		}
		else if (strcasecmp("fullscreen", key) == 0)
		{
			s_windowSettings.fullscreen = parseBool(value);
		}
	}

	void parseGraphicsSettings(const char* key, const char* value)
	{
		if (strcasecmp("gameWidth", key) == 0)
		{
			s_graphicsSettings.gameResolution.x = parseInt(value);
		}
		else if (strcasecmp("gameHeight", key) == 0)
		{
			s_graphicsSettings.gameResolution.z = parseInt(value);
		}
		else if (strcasecmp("fov", key) == 0)
		{
			s_graphicsSettings.fov = parseInt(value);
		}
		else if (strcasecmp("widescreen", key) == 0)
		{
			s_graphicsSettings.widescreen = parseBool(value);
		}
		else if (strcasecmp("asyncFramebuffer", key) == 0)
		{
			s_graphicsSettings.asyncFramebuffer = parseBool(value);
		}
		else if (strcasecmp("gpuColorConvert", key) == 0)
		{
			s_graphicsSettings.gpuColorConvert = parseBool(value);
		}
		else if (strcasecmp("colorCorrection", key) == 0)
		{
			s_graphicsSettings.colorCorrection = parseBool(value);
		}
		else if (strcasecmp("perspectiveCorrect3DO", key) == 0)
		{
			s_graphicsSettings.perspectiveCorrectTexturing = parseBool(value);
		}
		else if (strcasecmp("extendAjoinLimits", key) == 0)
		{
			s_graphicsSettings.extendAjoinLimits = parseBool(value);
		}
		else if (strcasecmp("vsync", key) == 0)
		{
			s_graphicsSettings.vsync = parseBool(value);
		}
		else if (strcasecmp("show_fps", key) == 0)
		{
			s_graphicsSettings.showFps = parseBool(value);
		}
		else if (strcasecmp("3doNormalFix", key) == 0)
		{
			s_graphicsSettings.fix3doNormalOverflow = parseBool(value);
		}
		else if (strcasecmp("ignore3doLimits", key) == 0)
		{
			s_graphicsSettings.ignore3doLimits = parseBool(value);
		}
		else if (strcasecmp("ditheredBilinear", key) == 0)
		{
			s_graphicsSettings.ditheredBilinear = parseBool(value);
		}
		else if (strcasecmp("useBilinear", key) == 0)
		{
			s_graphicsSettings.useBilinear = parseBool(value);
		}
		else if (strcasecmp("useMipmapping", key) == 0)
		{
			s_graphicsSettings.useMipmapping = parseBool(value);
		}
		else if (strcasecmp("bilinearSharpness", key) == 0)
		{
			s_graphicsSettings.bilinearSharpness = parseFloat(value);
		}
		else if (strcasecmp("anisotropyQuality", key) == 0)
		{
			s_graphicsSettings.anisotropyQuality = parseFloat(value);
		}
		else if (strcasecmp("frameRateLimit", key) == 0)
		{
			s_graphicsSettings.frameRateLimit = parseInt(value);
		}
		else if (strcasecmp("brightness", key) == 0)
		{
			s_graphicsSettings.brightness = parseFloat(value);
		}
		else if (strcasecmp("contrast", key) == 0)
		{
			s_graphicsSettings.contrast = parseFloat(value);
		}
		else if (strcasecmp("saturation", key) == 0)
		{
			s_graphicsSettings.saturation = parseFloat(value);
		}
		else if (strcasecmp("gamma", key) == 0)
		{
			s_graphicsSettings.gamma = parseFloat(value);
		}
		else if (strcasecmp("reticleEnable", key) == 0)
		{
			s_graphicsSettings.reticleEnable = parseBool(value);
		}
		else if (strcasecmp("reticleIndex", key) == 0)
		{
			s_graphicsSettings.reticleIndex = parseInt(value);
		}
		else if (strcasecmp("reticleRed", key) == 0)
		{
			s_graphicsSettings.reticleRed = parseFloat(value);
		}
		else if (strcasecmp("reticleGreen", key) == 0)
		{
			s_graphicsSettings.reticleGreen = parseFloat(value);
		}
		else if (strcasecmp("reticleBlue", key) == 0)
		{
			s_graphicsSettings.reticleBlue = parseFloat(value);
		}
		else if (strcasecmp("reticleOpacity", key) == 0)
		{
			s_graphicsSettings.reticleOpacity = parseFloat(value);
		}
		else if (strcasecmp("reticleScale", key) == 0)
		{
			s_graphicsSettings.reticleScale = parseFloat(value);
		}
		else if (strcasecmp("bloomEnabled", key) == 0)
		{
			s_graphicsSettings.bloomEnabled = parseBool(value);
		}
		else if (strcasecmp("bloomStrength", key) == 0)
		{
			s_graphicsSettings.bloomStrength = parseFloat(value);
		}
		else if (strcasecmp("bloomSpread", key) == 0)
		{
			s_graphicsSettings.bloomSpread = parseFloat(value);
		}
		else if (strcasecmp("renderer", key) == 0)
		{
			s_graphicsSettings.rendererIndex = parseInt(value);
		}
		else if (strcasecmp("colorMode", key) == 0)
		{
			s_graphicsSettings.colorMode = parseInt(value);
		}
		else if (strcasecmp("skyMode", key) == 0)
		{
			s_graphicsSettings.skyMode = SkyMode(parseInt(value));
		}
	}

	void parseEnhancementsSettings(const char* key, const char* value)
	{
		if (strcasecmp("hdTextures", key) == 0)
		{
			s_enhancementsSettings.enableHdTextures = parseBool(value);
		}
		else if (strcasecmp("hdSprites", key) == 0)
		{
			s_enhancementsSettings.enableHdSprites = parseBool(value);
		}
		else if (strcasecmp("hdHud", key) == 0)
		{
			s_enhancementsSettings.enableHdHud = parseBool(value);
		}
	}

	void parseHudSettings(const char* key, const char* value)
	{
		if (strcasecmp("hudScale", key) == 0)
		{
			for (size_t i = 0; i < TFE_ARRAYSIZE(c_tfeHudScaleStrings); i++)
			{
				if (strcasecmp(value, c_tfeHudScaleStrings[i]) == 0)
				{
					s_hudSettings.hudScale = TFE_HudScale(i);
					break;
				}
			}
		}
		else if (strcasecmp("hudPos", key) == 0)
		{
			for (size_t i = 0; i < TFE_ARRAYSIZE(c_tfeHudPosStrings); i++)
			{
				if (strcasecmp(value, c_tfeHudPosStrings[i]) == 0)
				{
					s_hudSettings.hudPos = TFE_HudPosition(i);
					break;
				}
			}
		}
		else if (strcasecmp("scale", key) == 0)
		{
			s_hudSettings.scale = parseFloat(value);
		}
		else if (strcasecmp("pixelOffsetLeft", key) == 0)
		{
			s_hudSettings.pixelOffset[0] = parseInt(value);
		}
		else if (strcasecmp("pixelOffsetRight", key) == 0)
		{
			s_hudSettings.pixelOffset[1] = parseInt(value);
		}
		else if (strcasecmp("pixelOffsetY", key) == 0)
		{
			s_hudSettings.pixelOffset[2] = parseInt(value);
		}
	}

	void parseSoundSettings(const char* key, const char* value)
	{
		if (strcasecmp("masterVolume", key) == 0)
		{
			s_soundSettings.masterVolume = parseFloat(value);
		}
		else if (strcasecmp("soundFxVolume", key) == 0)
		{
			s_soundSettings.soundFxVolume = parseFloat(value);
		}
		else if (strcasecmp("musicVolume", key) == 0)
		{
			s_soundSettings.musicVolume = parseFloat(value);
		}
		else if (strcasecmp("cutsceneSoundFxVolume", key) == 0)
		{
			s_soundSettings.cutsceneSoundFxVolume = parseFloat(value);
		}
		else if (strcasecmp("cutsceneMusicVolume", key) == 0)
		{
			s_soundSettings.cutsceneMusicVolume = parseFloat(value);
		}
		else if (strcasecmp("audioDevice", key) == 0)
		{
			s_soundSettings.audioDevice = parseInt(value);
		}
		else if (strcasecmp("midiOutput", key) == 0)
		{
			s_soundSettings.midiOutput = parseInt(value);
		}
		else if (strcasecmp("midiType", key) == 0)
		{
			s_soundSettings.midiType = parseInt(value);
		}
		else if (strcasecmp("use16Channels", key) == 0)
		{
			s_soundSettings.use16Channels = parseBool(value);
		}
		else if (strcasecmp("disableSoundInMenus", key) == 0)
		{
			s_soundSettings.disableSoundInMenus = parseBool(value);
		}
	}

	void parseSystemSettings(const char* key, const char* value)
	{
		if (strcasecmp("gameExitsToMenu", key) == 0)
		{
			s_systemSettings.gameQuitExitsToMenu = parseBool(value);
		}
		else if (strcasecmp("returnToModLoader", key) == 0)
		{
			s_systemSettings.returnToModLoader = parseBool(value);
		}
		else if (strcasecmp("gifRecordingFramerate", key) == 0)
		{
			s_systemSettings.gifRecordingFramerate = parseFloat(value);
		}
	}
	
	void parseA11ySettings(const char* key, const char* value)
	{
		if (strcasecmp("language", key) == 0)
		{
			s_a11ySettings.language = value;
		} 
		else if (strcasecmp("lastFontPath", key) == 0)
		{
			s_a11ySettings.lastFontPath = value;
		} 
		else if (strcasecmp("showCutsceneSubtitles", key) == 0)
		{
			s_a11ySettings.showCutsceneSubtitles = parseBool(value);
		}
		else if (strcasecmp("showCutsceneCaptions", key) == 0)
		{
			s_a11ySettings.showCutsceneCaptions = parseBool(value);
		}
		else if (strcasecmp("showGameplaySubtitles", key) == 0)
		{
			s_a11ySettings.showGameplaySubtitles = parseBool(value);
		}
		else if (strcasecmp("showGameplayCaptions", key) == 0)
		{
			s_a11ySettings.showGameplayCaptions = parseBool(value);
		}
		else if (strcasecmp("cutsceneFontSize", key) == 0)
		{
			s_a11ySettings.cutsceneFontSize = (FontSize)parseInt(value);
		}
		else if (strcasecmp("gameplayFontSize", key) == 0)
		{
			s_a11ySettings.gameplayFontSize = (FontSize)parseInt(value);
		}
		else if (strcasecmp("cutsceneFontColor", key) == 0)
		{
			s_a11ySettings.cutsceneFontColor = parseColor(value);
		}
		else if (strcasecmp("gameplayFontColor", key) == 0)
		{
			s_a11ySettings.gameplayFontColor = parseColor(value);
		}
		else if (strcasecmp("cutsceneTextBackgroundAlpha", key) == 0)
		{
			s_a11ySettings.cutsceneTextBackgroundAlpha = parseFloat(value);
		}
		else if (strcasecmp("gameplayTextBackgroundAlpha", key) == 0)
		{
			s_a11ySettings.gameplayTextBackgroundAlpha = parseFloat(value);
		}
		else if (strcasecmp("gameplayMaxTextLines", key) == 0)
		{
			s_a11ySettings.gameplayMaxTextLines = parseInt(value);
		}
		else if (strcasecmp("showCutsceneTextBorder", key) == 0)
		{
			s_a11ySettings.showCutsceneTextBorder = parseBool(value);
		}
		else if (strcasecmp("showGameplayTextBorder", key) == 0)
		{
			s_a11ySettings.showGameplayTextBorder = parseBool(value);
		}
		else if (strcasecmp("cutsceneTextSpeed", key) == 0)
		{
			s_a11ySettings.cutsceneTextSpeed = parseFloat(value);
		}
		else if (strcasecmp("gameplayTextSpeed", key) == 0)
		{
			s_a11ySettings.gameplayTextSpeed = parseFloat(value);
		}
		else if (strcasecmp("gameplayCaptionMinVolume", key) == 0)
		{
			s_a11ySettings.gameplayCaptionMinVolume = parseInt(value);
		}
		else if (strcasecmp("enableHeadwave", key) == 0)
		{
			s_a11ySettings.enableHeadwave = parseBool(value);
		}
		else if (strcasecmp("disableScreenFlashes", key) == 0)
		{
			s_a11ySettings.disableScreenFlashes = parseBool(value);
		}
		else if (strcasecmp("disablePlayerWeaponLighting", key) == 0)
		{
			s_a11ySettings.disablePlayerWeaponLighting = parseBool(value);
		}
	}

	void parseGame(const char* key, const char* value)
	{
		if (strcasecmp("game", key) == 0)
		{
			strcpy(s_game.game, value);
			// Get the game ID.
			s_game.id = Game_Count;
			for (u32 i = 0; i < Game_Count; i++)
			{
				if (strcasecmp(s_game.game, c_gameName[i]) == 0)
				{
					s_game.id = GameID(i);
					break;
				}
			}
		}
	}

	void appendSlash(char* path)
	{
		size_t len = strlen(path);
		if (len == 0) { return; }
		if (path[len - 1] == '\\' || path[len - 1] == '/') { return; }
		
		path[len] = '/';
		path[len + 1] = 0;
	}

	void parseDark_ForcesSettings(const char* key, const char* value)
	{
		if (strcasecmp("sourcePath", key) == 0)
		{
			strcpy(s_gameSettings.header[Game_Dark_Forces].sourcePath, value);
			appendSlash(s_gameSettings.header[Game_Dark_Forces].sourcePath);
		}
		else if (strcasecmp("emulatorPath", key) == 0)
		{
			strcpy(s_gameSettings.header[Game_Dark_Forces].emulatorPath, value);
			appendSlash(s_gameSettings.header[Game_Dark_Forces].emulatorPath);
		}
		else if (strcasecmp("airControl", key) == 0)
		{
			s_gameSettings.df_airControl = std::min(std::max(parseInt(value), 0), 8);
		}
		else if (strcasecmp("bobaFettFacePlayer", key) == 0)
		{
			s_gameSettings.df_bobaFettFacePlayer = parseBool(value);
		}	
		else if (strcasecmp("smoothVUEs", key) == 0)
		{
			s_gameSettings.df_smoothVUEs = parseBool(value);
		}
		else if (strcasecmp("disableFightMusic", key) == 0)
		{
			s_gameSettings.df_disableFightMusic = parseBool(value);
		}
		else if (strcasecmp("enableAutoaim", key) == 0)
		{
			s_gameSettings.df_enableAutoaim = parseBool(value);
		}
		else if (strcasecmp("showSecretFoundMsg", key) == 0)
		{
			s_gameSettings.df_showSecretFoundMsg = parseBool(value);
		}
		else if (strcasecmp("autorun", key) == 0)
		{
			s_gameSettings.df_autorun = parseBool(value);
		}
		else if (strcasecmp("crouchToggle", key) == 0)
		{
			s_gameSettings.df_crouchToggle = parseBool(value);
		}
		else if (strcasecmp("ignoreInfLimit", key) == 0)
		{
			s_gameSettings.df_ignoreInfLimit = parseBool(value);
		}
		else if (strcasecmp("stepSecondAlt", key) == 0)
		{
			s_gameSettings.df_stepSecondAlt = parseBool(value);
		}
		else if (strcasecmp("pitchLimit", key) == 0)
		{
			s_gameSettings.df_pitchLimit = PitchLimit(parseInt(value));
		}
		else if (strcasecmp("solidWallFlagFix", key) == 0)
		{
			s_gameSettings.df_solidWallFlagFix = parseBool(value);
		}
	}

	void parseOutlawsSettings(const char* key, const char* value)
	{
		if (strcasecmp("sourcePath", key) == 0)
		{
			strcpy(s_gameSettings.header[Game_Outlaws].sourcePath, value);
			appendSlash(s_gameSettings.header[Game_Outlaws].sourcePath);
		}
	}
	
	void parseCVars(const char* key, const char* _value)
	{
		// Split the value into <type value>
		char tmp[256];
		strcpy(tmp, _value);

		char* type = strtok(tmp, " ");
		char* value = strtok(nullptr, " ");
		if (!type || !value) { return; }
		
		if (strcasecmp(type, "int") == 0)
		{
			TFE_Console::addSerializedCVarInt(key, parseInt(value));
		}
		else if (strcasecmp(type, "float") == 0)
		{
			TFE_Console::addSerializedCVarFloat(key, parseFloat(value));
		}
		else if (strcasecmp(type, "bool") == 0)
		{
			TFE_Console::addSerializedCVarBool(key, parseBool(value));
		}
		else if (strcasecmp(type, "string") == 0)
		{
			value = &value[1];
			value[strlen(value) - 1] = 0;
			TFE_Console::addSerializedCVarString(key, value);
		}
	}

	bool validatePath(const char* path, const char* sentinel)
	{
		if (!FileUtil::directoryExits(path)) { return false; }

		char sentinelPath[TFE_MAX_PATH];
		sprintf(sentinelPath, "%s%s", path, sentinel);
		FileStream file;
		if (!file.open(sentinelPath, Stream::MODE_READ))
		{
			return false;
		}
		bool valid = file.getSize() > 1;
		file.close();

		return valid;
	}

	////////////////////////////////////////////////////////////////////////////
	// Helper functions to determine valid HD assets.
	////////////////////////////////////////////////////////////////////////////
	static char s_levelName[TFE_MAX_PATH];
	
	void setLevelName(const char* levelName)
	{
		if (!levelName) { return; }
		strcpy(s_levelName, levelName);
		strcat(s_levelName, ".LEV");
	}

	bool isHdAssetValid(const char* assetName, HdAssetType type)
	{
		bool valid = true;

		const size_t listCount = s_modSettings.ignoreList.size();
		const ModHdIgnoreList* list = s_modSettings.ignoreList.data();
		for (size_t l = 0; l < listCount; l++)
		{
			if (strcasecmp(list[l].levName.c_str(), s_levelName) != 0)
			{
				continue;
			}
			size_t ignoreListCount = 0;
			const std::string* ignoreList = nullptr;

			switch (type)
			{
				case HD_ASSET_TYPE_BM:
				{
					ignoreListCount = list[l].bmIgnoreList.size();
					ignoreList = list[l].bmIgnoreList.data();
				} break;
				case HD_ASSET_TYPE_FME:
				{
					ignoreListCount = list[l].fmeIgnoreList.size();
					ignoreList = list[l].fmeIgnoreList.data();
				} break;
				case HD_ASSET_TYPE_WAX:
				{
					ignoreListCount = list[l].waxIgnoreList.size();
					ignoreList = list[l].waxIgnoreList.data();
				} break;
			}

			for (size_t i = 0; i < ignoreListCount; i++)
			{
				if (strcasecmp(ignoreList[i].c_str(), assetName) == 0)
				{
					valid = false;
					break;
				}
			}
			break;
		}

		return valid;
	}

	////////////////////////////////////////////////////////////////////////////
	// Handle overrides from MOD_CONF.txt
	// By default overrides are not set (MSO_NOT_SET), in which case the user
	// settings are used.
	////////////////////////////////////////////////////////////////////////////
	bool ignoreInfLimits()
	{
		if (s_modSettings.ignoreInfLimits != MSO_NOT_SET)
		{
			return s_modSettings.ignoreInfLimits == MSO_TRUE ? true : false;
		}
		return s_gameSettings.df_ignoreInfLimit;
	}

	bool stepSecondAlt()
	{
		if (s_modSettings.stepSecondAlt != MSO_NOT_SET)
		{
			return s_modSettings.stepSecondAlt == MSO_TRUE ? true : false;
		}
		return s_gameSettings.df_stepSecondAlt;
	}

	bool soidWallFlagFix()
	{
		if (s_modSettings.solidWallFlagFix != MSO_NOT_SET)
		{
			return s_modSettings.solidWallFlagFix == MSO_TRUE ? true : false;
		}
		return s_gameSettings.df_solidWallFlagFix;
	}

	bool extendAdjoinLimits()
	{
		if (s_modSettings.extendAjoinLimits != MSO_NOT_SET)
		{
			return s_modSettings.extendAjoinLimits == MSO_TRUE ? true : false;
		}
		return s_graphicsSettings.extendAjoinLimits;
	}

	bool ignore3doLimits()
	{
		if (s_modSettings.ignore3doLimits != MSO_NOT_SET)
		{
			return s_modSettings.ignore3doLimits == MSO_TRUE ? true : false;
		}
		return s_graphicsSettings.ignore3doLimits;
	}

	bool normalFix3do()
	{
		if (s_modSettings.normalFix3do != MSO_NOT_SET)
		{
			return s_modSettings.normalFix3do == MSO_TRUE ? true : false;
		}
		return s_graphicsSettings.fix3doNormalOverflow;
	}

	//////////////////////////////////////////////////
	// Mod Level Settings/Overrides.
	//////////////////////////////////////////////////

	ModSettingLevelOverride getLevelOverrides(string levelName)
	{
		string lowerLevel = TFE_A11Y::toLower(levelName);
		if (s_modSettings.levelOverrides.find(lowerLevel) != s_modSettings.levelOverrides.end())
		{
			return s_modSettings.levelOverrides[lowerLevel];
		}
		ModSettingLevelOverride empty;
		return empty;
	}
		
	//////////////////////////////////////////////////
	// Mod Settings/Overrides.
	//////////////////////////////////////////////////
	ModSettingOverride parseJSonBoolToOverride(const cJSON* item)
	{
		ModSettingOverride value = MSO_NOT_SET;  // default
		if (cJSON_IsString(item))
		{
			value = (strcasecmp(item->valuestring, "true") == 0) ? MSO_TRUE : MSO_FALSE;
		}
		else if (cJSON_IsBool(item))
		{
			value = cJSON_IsTrue(item) ? MSO_TRUE : MSO_FALSE;
		}
		else
		{
			TFE_System::logWrite(LOG_WARNING, "MOD_CONF", "Override '%s' is an invalid type and should be a bool. Ignoring override.", item->string);
		}
		return value;
	}

	int parseJSonIntToOverride(const cJSON* item)
	{
		int value = -1;  // default is -1

		// Check if it is a json numerical
		if (cJSON_IsNumber(item))
		{
			value = cJSON_GetNumberValue(item);
		}
		else
		{
			std::string valueString = item->valuestring;
			if (valueString.find_first_not_of("0123456789") == string::npos)
			{
				value = std::stoi(valueString);
			}
			else
			{
				TFE_System::logWrite(LOG_WARNING, "MOD_CONF", "Override '%s' is an invalid type and should be an integer. Ignoring override.", item->string);
			}
		}
		return value;
	};

	void parseTfeOverride(TFE_ModSettings* modSettings, const cJSON* tfeOverride)
	{
		if (!tfeOverride || !tfeOverride->string) { return; }

		if (strcasecmp(tfeOverride->string, "ignoreInfLimit") == 0)
		{
			modSettings->ignoreInfLimits = parseJSonBoolToOverride(tfeOverride);
		}
		else if (strcasecmp(tfeOverride->string, "stepSecondAlt") == 0)
		{
			modSettings->stepSecondAlt = parseJSonBoolToOverride(tfeOverride);
		}
		else if (strcasecmp(tfeOverride->string, "solidWallFlagFix") == 0)
		{
			modSettings->solidWallFlagFix = parseJSonBoolToOverride(tfeOverride);
		}
		else if (strcasecmp(tfeOverride->string, "extendAjoinLimits") == 0)
		{
			modSettings->extendAjoinLimits = parseJSonBoolToOverride(tfeOverride);
		}
		else if (strcasecmp(tfeOverride->string, "ignore3doLimits") == 0)
		{
			modSettings->ignore3doLimits = parseJSonBoolToOverride(tfeOverride);
		}
		else if (strcasecmp(tfeOverride->string, "3doNormalFix") == 0)
		{
			modSettings->normalFix3do = parseJSonBoolToOverride(tfeOverride);
		}
		else if(strcasecmp(tfeOverride->string, "levelOverrides") == 0)
		{
			const cJSON* levelIter = tfeOverride->child;
			if (!levelIter)
			{
				TFE_System::logWrite(LOG_WARNING, "MOD_CONF", "Level String is Empty", tfeOverride->string);
			}
			for (; levelIter; levelIter = levelIter->next)
			{
				if (!levelIter->string)
				{
					TFE_System::logWrite(LOG_WARNING, "MOD_CONF", "Level override for '%s' has no name, skipping.", levelIter->string);
					continue;
				}
				else
				{

					// Remove the .lev extension if it exists.
					std::string levelName = TFE_A11Y::toLower(levelIter->string);
					if (levelName.size() >= 4 && levelName.rfind(".lev") == (levelName.size() - 4)) {
						levelName.erase(levelName.size() - 4, 4);
					}

					ModSettingLevelOverride levelOverride;
					levelOverride.levName = levelName;

					const cJSON* levelOverrideIter = levelIter->child;
					for (; levelOverrideIter; levelOverrideIter = levelOverrideIter->next)
					{
						if (!levelOverrideIter->string)
						{
							TFE_System::logWrite(LOG_WARNING, "MOD_CONF", "Level override for '%s' has no name, skipping.", levelOverrideIter->string);
							continue;
						}
						else
						{
							const char * overrideName = levelOverrideIter->string;

							// Check if it is an integer-type override
							int intArraySize = sizeof(modIntOverrides) / sizeof(modIntOverrides[0]);
							bool isIntParam = false;
							for (int i = 0; i < intArraySize; ++i) {
								if (strcmp(modIntOverrides[i], overrideName) == 0) {

									// Parse the integer value.
									int jsonIntResult = parseJSonIntToOverride(levelOverrideIter);
									if (jsonIntResult != -1)
									{
										levelOverride.intOverrideMap[overrideName] = jsonIntResult;
										isIntParam = true;
										break;
									}
								}
							}

							// Skip checking if parameter is a boolean if it is of type integer.
							if (isIntParam) {
								continue;
							}

							// Check if it is an bool-type override
							int boolArraySize = sizeof(modBoolOverrides) / sizeof(modBoolOverrides[0]);
							for (int i = 0; i < boolArraySize; ++i) {
								if (strcmp(modBoolOverrides[i], overrideName) == 0) {
									levelOverride.boolOverrideMap[overrideName] = parseJSonBoolToOverride(levelOverrideIter) == MSO_TRUE ? JTRUE : JFALSE;
									break;
								}
							}
						}						
					}
					modSettings->levelOverrides[levelName] = levelOverride;
				}
			}
		}
	}

	void parseJsonStringArray(const cJSON* item, std::vector<std::string>& stringList)
	{
		stringList.clear();
		if (!cJSON_IsArray(item))
		{
			TFE_System::logWrite(LOG_WARNING, "MOD_CONF", "Item '%s' is an invalid type, it should be an array of strings. Skipping.",
				item->string);
			return;
		}

		const cJSON* iter = item->child;
		if (!iter)
		{
			TFE_System::logWrite(LOG_WARNING, "MOD_CONF", "String array '%s' is empty.", item->string);
		}

		for (; iter; iter = iter->next)
		{
			if (cJSON_IsString(iter))
			{
				stringList.push_back(iter->valuestring);
			}
			else
			{
				TFE_System::logWrite(LOG_WARNING, "MOD_CONF", "Invalid type found in string array %s, ignoring.", item->string);
			}
		}
	}

	void loadCustomModSettings()
	{
		// Reset mod settings and overrides.
		TFE_Settings::clearModSettings();
		TFE_ModSettings* modSettings = TFE_Settings::getModSettings();

		FilePath filePath;
		FileStream file;
		if (!TFE_Paths::getFilePath("MOD_CONF.txt", &filePath)) { return; }
		if (!file.open(&filePath, FileStream::MODE_READ)) { return; }

		TFE_System::logWrite(LOG_MSG, "MOD_CONF", "Parsing MOD_CONF.txt for custom mod.");

		const size_t size = file.getSize();
		char* data = (char*)malloc(size + 1);
		if (!data || size == 0)
		{
			TFE_System::logWrite(LOG_ERROR, "MOD_CONF", "MOD_CONF.txt found but is %u bytes in size and cannot be read.", size);
			return;
		}
		file.readBuffer(data, (u32)size);
		data[size] = 0;
		file.close();

		cJSON* root = cJSON_Parse(data);
		if (root)
		{
			const cJSON* curElem = root->child;
			for (; curElem; curElem = curElem->next)
			{
				if (!curElem->string) { continue; }

				if (strcasecmp(curElem->string, "TFE_OVERRIDES") == 0 && cJSON_IsObject(curElem))
				{
					const cJSON* iter = curElem->child;
					for (; iter; iter = iter->next)
					{
						// These should be key-value pairs { name, value }.
						parseTfeOverride(modSettings, iter);
					}
				}
				else if (strstr(curElem->string, ".LEV") && cJSON_IsObject(curElem))
				{
					ModHdIgnoreList ignoreList;
					// Copy the level name.
					ignoreList.levName = curElem->string;
					// Loop through object items and read the individual ignore lists.
					const cJSON* iter = curElem->child;
					if (!iter)
					{
						TFE_System::logWrite(LOG_WARNING, "MOD_CONF", "Level overrides '%s' is empty, skipping.", ignoreList.levName.c_str());
					}

					for (; iter; iter = iter->next)
					{
						if (!iter->string)
						{
							TFE_System::logWrite(LOG_WARNING, "MOD_CONF", "Level override for '%s' has no name, skipping.", ignoreList.levName.c_str());
							continue;
						}

						if (strcasecmp(iter->string, "BM") == 0)
						{
							parseJsonStringArray(iter, ignoreList.bmIgnoreList);
						}
						else if (strcasecmp(iter->string, "FME") == 0)
						{
							parseJsonStringArray(iter, ignoreList.fmeIgnoreList);
						}
						else if (strcasecmp(iter->string, "WAX") == 0)
						{
							parseJsonStringArray(iter, ignoreList.waxIgnoreList);
						}
					}
					modSettings->ignoreList.push_back(ignoreList);
				}
			}
			cJSON_Delete(root);
		}
		else
		{
			const char* error = cJSON_GetErrorPtr();
			if (error)
			{
				TFE_System::logWrite(LOG_ERROR, "MOD_CONF", "Failed to parse MOD_CONF.txt before\n%s", error);
			}
			else
			{
				TFE_System::logWrite(LOG_ERROR, "MOD_CONF", "Failed to parse MOD_CONF.txt");
			}
		}
		free(data);
	}
}
