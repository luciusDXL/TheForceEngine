#include <cstdarg>
#include <cstring>

#include <TFE_System/system.h>
#include <TFE_FileSystem/physfswrapper.h>
#include <TFE_FrontEndUI/frontEndUi.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
	#include <Windows.h>
	#include <io.h>
#endif

namespace TFE_System
{
	static vpFile s_logFile;
	static char s_workStr[32768];
	static char s_msgStr[32768];
	static const char* c_typeNames[]=
	{
		"",			//LOG_MSG = 0,
		"Warning",	//LOG_WARNING,
		"Error",	//LOG_ERROR,
		"Critical", //LOG_CRITICAL,
	};

	bool logOpen(const char* filename)
	{
		return s_logFile.openwrite(filename);
	}

	void logClose()
	{
		s_logFile.close();
	}
	
	void debugWrite(const char* tag, const char* str, ...)
	{
		if (!tag || !str) { return; }

		//Handle the variable input, "printf" style messages
		va_list arg;
		va_start(arg, str);
		vsprintf(s_msgStr, str, arg);
		va_end(arg);

		sprintf(s_workStr, "[%s] %s\r\n", tag, s_msgStr);

		//Write to the debugger or terminal output.
		#ifdef _WIN32
			OutputDebugStringA(s_workStr);
		#else
			fprintf(stderr, "%s", s_workStr);
		#endif
	}

	void logWrite(LogWriteType type, const char* tag, const char* str, ...)
	{
		if (type >= LOG_COUNT || !tag || !str) { return; }

		//Handle the variable input, "printf" style messages
		va_list arg;
		va_start(arg, str);
		vsprintf(s_msgStr, str, arg);
		va_end(arg);
		//Format the message
		if (type != LOG_MSG)
		{
			sprintf(s_workStr, "[%s : %s] %s\r\n", c_typeNames[type], tag, s_msgStr);
		}
		else
		{
			sprintf(s_workStr, "[%s] %s\r\n", tag, s_msgStr);
		}
		//Write to disk
		s_logFile.write(s_workStr, (u32)strlen(s_workStr));

		//Write to the debugger or terminal output.
		#ifdef _WIN32
			OutputDebugStringA(s_workStr);
		#else
			fprintf(stderr, "%s", s_workStr);
		#endif
		//Critical log messages also act as asserts in the debugger.
		if (type == LOG_CRITICAL)
		{
			assert(0);
		}

		sprintf(s_workStr, "[%s] %s", tag, s_msgStr);
		size_t len = strlen(s_msgStr);
		char* msg = s_msgStr;
		char* msgStart = msg;
		for (size_t i = 0; i < len; i++)
		{
			if (msg[i] == '\n')
			{
				msg[i] = 0;
				TFE_FrontEndUI::logToConsole(msgStart);

				msgStart = msg + i + 1;
			}
		}
		if (msgStart < s_msgStr + len)
		{
			TFE_FrontEndUI::logToConsole(msgStart);
		}
	}
}
