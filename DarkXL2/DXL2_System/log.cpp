#pragma once
#include <DXL2_System/system.h>
#include <DXL2_FileSystem/filestream.h>
#include <DXL2_FileSystem/paths.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>

#ifdef _WIN32
	#include <Windows.h>
#endif

namespace DXL2_System
{
	static FileStream s_logFile;
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
		char logPath[DXL2_MAX_PATH];
		DXL2_Paths::appendPath(PATH_USER_DOCUMENTS, filename, logPath);

		return s_logFile.open(logPath, FileStream::MODE_WRITE);
	}

	void logClose()
	{
		s_logFile.close();
	}

	void logWrite(LogWriteType type, const char* tag, const char* str, ...)
	{
		assert(type < LOG_COUNT);
		assert( s_logFile.isOpen() && tag && str );
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
		s_logFile.writeBuffer(s_workStr, (u32)strlen(s_workStr));
		//Make sure to flush the file to disk if a crash is likely.
		if (type == LOG_ERROR || type == LOG_CRITICAL)
		{
			s_logFile.flush();
		}
		//Write to the debugger or terminal output.
		#ifdef _WIN32
			OutputDebugStringA(s_workStr);
		#else
			printf(s_workStr);
		#endif
		//Critical log messages also act as asserts in the debugger.
		if (type == LOG_CRITICAL)
		{
			assert(0);
		}
	}
}
