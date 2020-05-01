#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Renderer/renderer.h>
#include <string>

#define CVAR_INT(var, name, flags) TFE_Console::registerCVarInt(name, flags, &##var)
#define CVAR_FLOAT(var, name, flags) TFE_Console::registerCVarFloat(name, flags, &##var)
#define CVAR_BOOL(var, name, flags) TFE_Console::registerCVarBool(name, flags, &##var)
#define CVAR_STRING(var, name, flags) TFE_Console::registerCVarString(name, flags, ##var, sizeof(var))

namespace TFE_Console
{
	enum CVarType
	{
		CVAR_INT = 0,
		CVAR_FLOAT,
		CVAR_BOOL,
		CVAR_STRING,
	};

	struct CVar
	{
		std::string name;
		CVarType type;
		u32 flags;
		u32 maxLen = 0;
		union
		{
			s32*  valueInt;
			f32*  valueFloat;
			bool* valueBool;
			char* stringValue;
		};
	};
		
	void registerCVarInt(const char* name, u32 flags, s32* var);
	void registerCVarFloat(const char* name, u32 flags, f32* var);
	void registerCVarBool(const char* name, u32 flags, bool* var);
	void registerCVarString(const char* name, u32 flags, char* var, u32 maxLen);

	bool init();
	void destroy();

	void update();
	bool isOpen();
	void startOpen();
	void startClose();

	void addToHistory(const char* str);
}
