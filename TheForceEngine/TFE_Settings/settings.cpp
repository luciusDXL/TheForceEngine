#include "settings.h"
#include "gameSourceData.h"
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_System/parser.h>
#include <assert.h>
#include <vector>

#ifdef _WIN32
#include "windows/registry.h"
#endif

namespace TFE_Settings
{
	//////////////////////////////////////////////////////////////////////////////////
	// Local State
	//////////////////////////////////////////////////////////////////////////////////
	#define LINEBUF_LEN 1024
	
	static char s_settingsPath[TFE_MAX_PATH];
	static TFE_Settings_Window s_windowSettings = {};
	static TFE_Settings_Graphics s_graphicsSettings = {};
	static TFE_Settings_Sound s_soundSettings = {};
	static TFE_Game s_game = {};
	static TFE_Settings_Game s_gameSettings[Game_Count];
	static char s_lineBuffer[LINEBUF_LEN];
	static std::vector<char> s_iniBuffer;

	enum SectionID
	{
		SECTION_WINDOW = 0,
		SECTION_GRAPHICS,
		SECTION_SOUND,
		SECTION_GAME,
		SECTION_DARK_FORCES,
		SECTION_OUTLAWS,
		SECTION_COUNT,
		SECTION_INVALID = SECTION_COUNT,
		SECTION_GAME_START = SECTION_DARK_FORCES
	};

	static const char* c_sectionNames[SECTION_COUNT] =
	{
		"Window",
		"Graphics",
		"Sound",
		"Game",
		"Dark_Forces",
		"Outlaws",
	};

	//////////////////////////////////////////////////////////////////////////////////
	// Forward Declarations
	//////////////////////////////////////////////////////////////////////////////////
	// Write
	void writeWindowSettings(FileStream& settings);
	void writeGraphicsSettings(FileStream& settings);
	void writeSoundSettings(FileStream& settings);
	void writeGameSettings(FileStream& settings);
	void writePerGameSettings(FileStream& settings);

	// Read
	bool readFromDisk();
	void parseIniFile(const char* buffer, size_t len);
	void parseWindowSettings(const char* key, const char* value);
	void parseGraphicsSettings(const char* key, const char* value);
	void parseSoundSettings(const char* key, const char* value);
	void parseGame(const char* key, const char* value);
	void parseDark_ForcesSettings(const char* key, const char* value);
	void parseOutlawsSettings(const char* key, const char* value);
	void checkGameData();

	//////////////////////////////////////////////////////////////////////////////////
	// Implementation
	//////////////////////////////////////////////////////////////////////////////////
	bool init()
	{
		// Clear out game settings.
		memset(s_gameSettings, 0, sizeof(TFE_Settings_Game) * Game_Count);
		for (u32 i = 0; i < Game_Count; i++)
		{
			strcpy(s_gameSettings[i].gameName, c_gameName[i]);
		}
		strcpy(s_game.game, s_gameSettings[0].gameName);

		TFE_Paths::appendPath(PATH_USER_DOCUMENTS, "settings.ini", s_settingsPath);
		if (FileUtil::exists(s_settingsPath)) { return readFromDisk(); }
			
		checkGameData();
		return writeToDisk();
	}

	void shutdown()
	{
		// Write any settings to disk before shutting down.
		writeToDisk();
	}
		
	void checkGameData()
	{
		for (u32 gameId = 0; gameId < Game_Count; gameId++)
		{
			const size_t sourcePathLen = strlen(s_gameSettings[gameId].sourcePath);
			bool pathValid = sourcePathLen && FileUtil::directoryExits(s_gameSettings[gameId].sourcePath);
		#ifdef _WIN32
			// First try looking through the registry.
			if (!pathValid)
			{
				pathValid = WindowsRegistry::getSteamPathFromRegistry(c_steamLocalPath[gameId], s_gameSettings[gameId].sourcePath);
			
				if (!pathValid)
				{
					pathValid = WindowsRegistry::getGogPathFromRegistry(c_gogProductId[gameId], s_gameSettings[gameId].sourcePath);
				}
			}
		#endif
			// If the registry approach fails, just try looking in the various hardcoded paths.
			if (!pathValid)
			{
				// Try various possible locations.
				const char** locations = c_gameLocations[gameId];
				for (u32 i = 0; i < c_hardcodedPathCount; i++)
				{
					if (FileUtil::directoryExits(locations[i]))
					{
						strcpy(s_gameSettings[gameId].sourcePath, locations[i]);
						pathValid = true;
						break;
					}
				}
			}
		}
		writeToDisk();
	}

	bool readFromDisk()
	{
		FileStream settings;
		if (settings.open(s_settingsPath, FileStream::MODE_READ))
		{
			const size_t len = settings.getSize();
			s_iniBuffer.resize(len + 1);
			s_iniBuffer[len] = 0;
			settings.readBuffer(s_iniBuffer.data(), (u32)len);
			settings.close();

			parseIniFile(s_iniBuffer.data(), len);
			checkGameData();
			
			return true;
		}
		return false;
	}

	bool writeToDisk()
	{
		FileStream settings;
		if (settings.open(s_settingsPath, FileStream::MODE_WRITE))
		{
			writeWindowSettings(settings);
			writeGraphicsSettings(settings);
			writeSoundSettings(settings);
			writeGameSettings(settings);
			writePerGameSettings(settings);
			settings.close();

			return true;
		}
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

	TFE_Settings_Sound* getSoundSettings()
	{
		return &s_soundSettings;
	}

	TFE_Game* getGame()
	{
		return &s_game;
	}

	TFE_Settings_Game* getGameSettings(const char* gameName)
	{
		for (u32 i = 0; i < Game_Count; i++)
		{
			if (strcasecmp(gameName, c_gameName[i]) == 0)
			{
				return &s_gameSettings[i];
			}
		}

		return nullptr;
	}

	void writeHeader(FileStream& file, const char* section)
	{
		snprintf(s_lineBuffer, LINEBUF_LEN, "[%s]\r\n", section);
		file.writeBuffer(s_lineBuffer, (u32)strlen(s_lineBuffer));
	}

	void writeComment(FileStream& file, const char* comment)
	{
		snprintf(s_lineBuffer, LINEBUF_LEN, ";%s\r\n", comment);
		file.writeBuffer(s_lineBuffer, (u32)strlen(s_lineBuffer));
	}

	void writeKeyValue_String(FileStream& file, const char* key, const char* value)
	{
		snprintf(s_lineBuffer, LINEBUF_LEN, "%s=\"%s\"\r\n", key, value);
		file.writeBuffer(s_lineBuffer, (u32)strlen(s_lineBuffer));
	}

	void writeKeyValue_Int(FileStream& file, const char* key, s32 value)
	{
		snprintf(s_lineBuffer, LINEBUF_LEN, "%s=%d\r\n", key, value);
		file.writeBuffer(s_lineBuffer, (u32)strlen(s_lineBuffer));
	}

	void writeKeyValue_Float(FileStream& file, const char* key, f32 value)
	{
		snprintf(s_lineBuffer, LINEBUF_LEN, "%s=%0.3f\r\n", key, value);
		file.writeBuffer(s_lineBuffer, (u32)strlen(s_lineBuffer));
	}

	void writeKeyValue_Bool(FileStream& file, const char* key, bool value)
	{
		snprintf(s_lineBuffer, LINEBUF_LEN, "%s=%s\r\n", key, value ? "true" : "false");
		file.writeBuffer(s_lineBuffer, (u32)strlen(s_lineBuffer));
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
		writeKeyValue_Bool(settings, "asyncFramebuffer", s_graphicsSettings.asyncFramebuffer);
	}

	void writeSoundSettings(FileStream& settings)
	{
		writeHeader(settings, c_sectionNames[SECTION_SOUND]);
		writeKeyValue_Float(settings, "soundFxVolume", s_soundSettings.soundFxVolume);
		writeKeyValue_Float(settings, "musicVolume", s_soundSettings.musicVolume);
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
			writeKeyValue_String(settings, "sourcePath", s_gameSettings[i].sourcePath);
			writeKeyValue_String(settings, "emulatorPath", s_gameSettings[i].emulatorPath);
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
				case SECTION_SOUND:
					parseSoundSettings(tokens[0].c_str(), tokens[1].c_str());
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
				default:
					assert(0);
				};
			}
		}
	}

	s32 parseInt(const char* value)
	{
		char* endPtr = nullptr;
		return strtol(value, &endPtr, 10);
	}

	f32 parseFloat(const char* value)
	{
		char* endPtr = nullptr;
		return (f32)strtod(value, &endPtr);
	}

	bool parseBool(const char* value)
	{
		if (value[0] == 'f' || value[0] == '0') { return false; }
		return true;
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
		else if (strcasecmp("asyncFramebuffer", key) == 0)
		{
			s_graphicsSettings.asyncFramebuffer = parseBool(value);
		}
	}

	void parseSoundSettings(const char* key, const char* value)
	{
		if (strcasecmp("soundFxVolume", key) == 0)
		{
			s_soundSettings.soundFxVolume = parseFloat(value);
		}
		else if (strcasecmp("musicVolume", key) == 0)
		{
			s_soundSettings.musicVolume = parseFloat(value);
		}
	}

	void parseGame(const char* key, const char* value)
	{
		if (strcasecmp("game", key) == 0)
		{
			strcpy(s_game.game, value);
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
			strcpy(s_gameSettings[Game_Dark_Forces].sourcePath, value);
			appendSlash(s_gameSettings[Game_Dark_Forces].sourcePath);
		}
		else if (strcasecmp("emulatorPath", key) == 0)
		{
			strcpy(s_gameSettings[Game_Dark_Forces].emulatorPath, value);
			appendSlash(s_gameSettings[Game_Dark_Forces].emulatorPath);
		}
	}

	void parseOutlawsSettings(const char* key, const char* value)
	{
		if (strcasecmp("sourcePath", key) == 0)
		{
			strcpy(s_gameSettings[Game_Outlaws].sourcePath, value);
			appendSlash(s_gameSettings[Game_Outlaws].sourcePath);
		}
	}
}
