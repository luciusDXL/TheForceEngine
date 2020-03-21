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
	static char s_settingsPath[DXL2_MAX_PATH];
	static DXL2_Settings_Window s_windowSettings = {};
	static DXL2_Settings_Graphics s_graphicsSettings = {};
	static char s_lineBuffer[LINEBUF_LEN];
	static std::vector<char> s_iniBuffer;

	enum SectionID
	{
		SECTION_WINDOW = 0,
		SECTION_GRAPHICS,
		SECTION_COUNT,
		SECTION_INVALID = SECTION_COUNT
	};

	static const char* c_sectionNames[SECTION_COUNT] =
	{
		"Window",
		"Graphics",
	};

	//////////////////////////////////////////////////////////////////////////////////
	// Forward Declarations
	//////////////////////////////////////////////////////////////////////////////////
	// Write
	void writeWindowSettings(FileStream& settings);
	void writeGraphicsSettings(FileStream& settings);

	// Read
	bool readFromDisk();
	void parseIniFile(const char* buffer, size_t len);
	void parseWindowSettings(const char* key, const char* value);
	void parseGraphicsSettings(const char* key, const char* value);

	//////////////////////////////////////////////////////////////////////////////////
	// Implementation
	//////////////////////////////////////////////////////////////////////////////////
	bool init()
	{
		DXL2_Paths::appendPath(PATH_USER_DOCUMENTS, "settings.ini", s_settingsPath);
		if (FileUtil::exists(s_settingsPath)) { return readFromDisk(); }

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
}
