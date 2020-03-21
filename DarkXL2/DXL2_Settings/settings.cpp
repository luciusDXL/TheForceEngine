#include "settings.h"
#include <DXL2_FileSystem/filestream.h>
#include <DXL2_FileSystem/fileutil.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_System/parser.h>
#include <assert.h>
#include <vector>

namespace DXL2_Settings
{
	//////////////////////////////////////////////////////////////////////////////////
	// Resolution Info
	//////////////////////////////////////////////////////////////////////////////////
	// Base Cost: ~2.6 ms / 1M pixels.
	// 4K only possible with palette conversion during blit in the shader.
	// Possibly double buffer (+1 frame latency).
	//s32 w = 320,  h = 200;	// 200p
	//s32 w = 1440, h = 1080;	//1080p
	//s32 w = 1920, h = 1440;	//1440p
	//s32 w = 2880, h = 2160;	//4K
	//s32 w = 1024, h = 768;

	//////////////////////////////////////////////////////////////////////////////////
	// Local State
	//////////////////////////////////////////////////////////////////////////////////
#define LINEBUF_LEN 1024
	enum Games
	{
		Game_Dark_Forces = 0,
		Game_Outlaws,
		Game_Count
	};

	static const char* c_gameName[]=
	{
		"Dark Forces",
		"Outlaws",
	};

	static char s_settingsPath[DXL2_MAX_PATH];
	static DXL2_Settings_Window s_windowSettings = {};
	static DXL2_Settings_Graphics s_graphicsSettings = {};
	static DXL2_Settings_Game s_gameSettings[Game_Count];
	static char s_lineBuffer[LINEBUF_LEN];
	static std::vector<char> s_iniBuffer;

	enum SectionID
	{
		SECTION_WINDOW = 0,
		SECTION_GRAPHICS,
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
		"Dark_Forces",
		"Outlaws",
	};

	//////////////////////////////////////////////////////////////////////////////////
	// Forward Declarations
	//////////////////////////////////////////////////////////////////////////////////
	// Write
	void writeWindowSettings(FileStream& settings);
	void writeGraphicsSettings(FileStream& settings);
	void writeGameSettings(FileStream& settings);

	// Read
	bool readFromDisk();
	void parseIniFile(const char* buffer, size_t len);
	void parseWindowSettings(const char* key, const char* value);
	void parseGraphicsSettings(const char* key, const char* value);
	void parseDark_ForcesSettings(const char* key, const char* value);
	void parseOutlawsSettings(const char* key, const char* value);

	//////////////////////////////////////////////////////////////////////////////////
	// Implementation
	//////////////////////////////////////////////////////////////////////////////////
	bool init()
	{
		DXL2_Paths::appendPath(PATH_USER_DOCUMENTS, "settings.ini", s_settingsPath);
		if (FileUtil::exists(s_settingsPath)) { return readFromDisk(); }

		memset(s_gameSettings, 0, sizeof(DXL2_Settings_Game) * Game_Count);
		for (u32 i = 0; i < Game_Count; i++)
		{
			strcpy(s_gameSettings[i].gameName, c_gameName[i]);
		}

		return writeToDisk();
	}

	void shutdown()
	{
		// Write any settings to disk before shutting down.
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
			writeGameSettings(settings);
			settings.close();

			return true;
		}
		return false;
	}

	// Get and set settings.
	DXL2_Settings_Window* getWindowSettings()
	{
		return &s_windowSettings;
	}

	DXL2_Settings_Graphics* getGraphicsSettings()
	{
		return &s_graphicsSettings;
	}

	DXL2_Settings_Game* getGameSettings(const char* gameName)
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

	void writeKeyValue_Bool(FileStream& file, const char* key, bool value)
	{
		snprintf(s_lineBuffer, LINEBUF_LEN, "%s=%s\r\n", key, value ? "true" : "false");
		file.writeBuffer(s_lineBuffer, (u32)strlen(s_lineBuffer));
	}

	void writeWindowSettings(FileStream& settings)
	{
		writeHeader(settings, "Window");
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
		writeHeader(settings, "Graphics");
		writeKeyValue_Int(settings, "gameWidth", s_graphicsSettings.gameResolution.x);
		writeKeyValue_Int(settings, "gameHeight", s_graphicsSettings.gameResolution.z);
	}

	void writeGameSettings(FileStream& settings)
	{
		for (u32 i = 0; i < Game_Count; i++)
		{
			writeHeader(settings, c_sectionNames[SECTION_GAME_START + i]);
			writeKeyValue_String(settings, "sourcePath", s_gameSettings[i].sourcePath);
			if (s_gameSettings[i].emulatorPath[0])
			{
				writeKeyValue_String(settings, "emulatorPath", s_gameSettings[i].emulatorPath);
			}
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
		DXL2_Parser parser;
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
					assert(0);
					return;
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
	}

	void parseDark_ForcesSettings(const char* key, const char* value)
	{
		if (strcasecmp("sourcePath", key) == 0)
		{
			strcpy(s_gameSettings[Game_Dark_Forces].sourcePath, value);
		}
		else if (strcasecmp("emulatorPath", key) == 0)
		{
			strcpy(s_gameSettings[Game_Dark_Forces].emulatorPath, value);
		}
	}

	void parseOutlawsSettings(const char* key, const char* value)
	{
		if (strcasecmp("sourcePath", key) == 0)
		{
			strcpy(s_gameSettings[Game_Outlaws].sourcePath, value);
		}
	}
}
