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
	void writeHeader(vpFile& file, const char* section)
	{
		int i = snprintf(s_lineBuffer, LINEBUF_LEN, "[%s]\r\n", section);
		file.write(s_lineBuffer, i);
	}

	void writeComment(vpFile& file, const char* comment)
	{
		int i = snprintf(s_lineBuffer, LINEBUF_LEN, ";%s\r\n", comment);
		file.write(s_lineBuffer, i);
	}

	void writeKeyValue_String(vpFile& file, const char* key, const char* value)
	{
		int i = snprintf(s_lineBuffer, LINEBUF_LEN, "%s=\"%s\"\r\n", key, value);
		file.write(s_lineBuffer, i);
	}

	void writeKeyValue_StringBlock(vpFile& file, const char* key, const char* value)
	{
		int i = snprintf(s_lineBuffer, LINEBUF_LEN, "%s {\r\n", key);
		file.write(s_lineBuffer, i);
		{
			file.write((char *)value, (u32)strlen(value));
		}
		i = snprintf(s_lineBuffer, LINEBUF_LEN, "\r\n}\r\n");
		file.write(s_lineBuffer, i);
	}

	void writeKeyValue_Int(vpFile& file, const char* key, s32 value)
	{
		int i = snprintf(s_lineBuffer, LINEBUF_LEN, "%s=%d\r\n", key, value);
		file.write(s_lineBuffer, i);
	}

	void writeKeyValue_Float(vpFile& file, const char* key, f32 value)
	{
		int i = snprintf(s_lineBuffer, LINEBUF_LEN, "%s=%0.3f\r\n", key, value);
		file.write(s_lineBuffer, i);
	}

	void writeKeyValue_Bool(vpFile& file, const char* key, bool value)
	{
		int i = snprintf(s_lineBuffer, LINEBUF_LEN, "%s=%s\r\n", key, value ? "true" : "false");
		file.write(s_lineBuffer, i);
	}

	void writeKeyValue_RGBA(vpFile& file, const char* key, RGBA value)
	{
		writeKeyValue_Int(file, key, value.color);
	}
}

