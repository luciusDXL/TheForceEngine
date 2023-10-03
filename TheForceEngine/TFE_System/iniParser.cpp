#include <cstring>
#include "iniParser.h"
#include <algorithm>

namespace TFE_IniParser
{
	static char s_lineBuffer[LINEBUF_LEN];

	// Read Functions
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

	RGBA parseColor(const char* value)
	{
		s32 v = parseInt(value);
		return RGBA(v);
	}
		
	// Write functions
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

	void writeKeyValue_StringBlock(FileStream& file, const char* key, const char* value)
	{
		snprintf(s_lineBuffer, LINEBUF_LEN, "%s {\r\n", key);
		file.writeBuffer(s_lineBuffer, (u32)strlen(s_lineBuffer));
		{
			file.writeBuffer(value, (u32)strlen(value));
		}
		snprintf(s_lineBuffer, LINEBUF_LEN, "\r\n}\r\n");
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

	void writeKeyValue_RGBA(FileStream& file, const char* key, RGBA value)
	{
		writeKeyValue_Int(file, key, value.color);
	}
}
