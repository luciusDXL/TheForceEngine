#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine System Library
// System functionality, such as timers and logging.
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include <TFE_FileSystem/filestream.h>
#include <vector>
#include <string>

#define LINEBUF_LEN 1024

/// <summary>
	/// Represents an RGBA color as a 32-bit unsigned integer, with properties for reading the channel
	/// values as U8s or floats.
	/// </summary>
struct RGBA
{
	u32 color;
	u8 getAlpha() { return (color >> 24) & 0xff; }
	u8 getRed() { return (color >> 16) & 0xff; }
	u8 getGreen() { return (color >> 8) & 0xff; }
	u8 getBlue() { return color & 0xff; }

	f32 getAlphaF() { return getAlpha() / 255.0f; }
	f32 getRedF() { return getRed() / 255.0f; }
	f32 getGreenF() { return getGreen() / 255.0f; }
	f32 getBlueF() { return getBlue() / 255.0f; }

	RGBA()
	{

	}

	RGBA(u32 color)
	{
		this->color = color;
	}

	static RGBA fromFloats(f32 r, f32 g, f32 b)
	{
		RGBA color;
		color.color = (u8)(b * 255) + ((u8)(g * 255) << 8) + ((u8)(r * 255) << 16) + (255 << 24);
		return color;
	}
	static RGBA fromFloats(f32 r, f32 g, f32 b, f32 a)
	{
		RGBA color;
		color.color = (u8)(b * 255) + ((u8)(g * 255) << 8) + ((u8)(r * 255) << 16) + ((u8)(a * 255) << 24);
		return color;
	}
};

struct RGBAf
{
	f32 r, g, b, a;

	RGBA ToRGBA()
	{
		return RGBA::fromFloats(r, g, b, a);
	}
};

namespace TFE_IniParser
{
	s32  parseInt(const char* value);
	f32  parseFloat(const char* value);
	bool parseBool(const char* value);
	RGBA parseColor(const char* value);

	void writeHeader(FileStream& file, const char* section);
	void writeComment(FileStream& file, const char* comment);
	void writeKeyValue_String(FileStream& file, const char* key, const char* value);
	void writeKeyValue_StringBlock(FileStream& file, const char* key, const char* value);
	void writeKeyValue_Int(FileStream& file, const char* key, s32 value);
	void writeKeyValue_Float(FileStream& file, const char* key, f32 value);
	void writeKeyValue_Bool(FileStream& file, const char* key, bool value);
	void writeKeyValue_RGBA(FileStream& file, const char* key, RGBA value);
}
