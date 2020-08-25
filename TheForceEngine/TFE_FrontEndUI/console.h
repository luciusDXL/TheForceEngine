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
#include <vector>

enum CVarFlag
{
	CVFLAG_NONE = 0,
	CVFLAG_READ_ONLY = (1 << 0),
};

#define CVAR_INT(var, name, flags, help) TFE_Console::registerCVarInt(name, flags, &##var, help)
#define CVAR_FLOAT(var, name, flags, help) TFE_Console::registerCVarFloat(name, flags, &##var, help)
#define CVAR_BOOL(var, name, flags, help) TFE_Console::registerCVarBool(name, flags, &##var, help)
#define CVAR_STRING(var, name, flags, help) TFE_Console::registerCVarString(name, flags, ##var, sizeof(var), help)

// Register a basic command
#define CCMD(name, func, argCount, help) TFE_Console::registerCommand(name, func, argCount, help)
// Register a command that is not repeated in the output (such as echo).
#define CCMD_NOREPEAT(name, func, argCount, help) TFE_Console::registerCommand(name, func, argCount, help, false)

typedef std::vector<std::string> ConsoleArgList;

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
		std::string helpString;
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

	typedef void(*ConsoleFunc)(const std::vector<std::string>& args);
			
	void registerCVarInt(const char* name, u32 flags, s32* var, const char* helpString);
	void registerCVarFloat(const char* name, u32 flags, f32* var, const char* helpString);
	void registerCVarBool(const char* name, u32 flags, bool* var, const char* helpString);
	void registerCVarString(const char* name, u32 flags, char* var, u32 maxLen, const char* helpString);
	void registerCommand(const char* name, ConsoleFunc func, u32 argCount, const char* helpString, bool repeat = true);

	bool init();
	void destroy();

	void update();
	bool isOpen();
	void startOpen();
	void startClose();

	void addToHistory(const char* str);

	inline f32 getFloatArg(const std::string& arg)
	{
		char* endPtr = nullptr;
		return (f32)strtod(arg.c_str(), &endPtr);
	}

	inline bool getBoolArg(const std::string& arg)
	{
		const char* cstr = arg.c_str();
		if (strcasecmp(cstr, "false") == 0 || strcasecmp(cstr, "0") == 0)
		{
			return false;
		}
		return true;
	}

	inline const char* getStringArg(const std::string& arg)
	{
		return arg.c_str();
	}
}
