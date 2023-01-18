#include "crashHandler.h"
#include <TFE_System/system.h>
#include <TFE_FileSystem/paths.h>

#include <stdio.h>
#include <signal.h>

static void sigabrtHandler(int);
static void sigfpeHandler(int /*code*/, int subcode);
static void sigintHandler(int);
static void sigillHandler(int);
static void sigsegvHandler(int);
static void sigtermHandler(int);

namespace TFE_CrashHandler
{
	void setProcessExceptionHandlers()
	{
#if 0
		// Catch an abnormal program termination
		signal(SIGABRT, sigabrtHandler);

		// Catch illegal instruction handler
		signal(SIGINT, sigintHandler);

		// Catch a termination request
		signal(SIGTERM, sigtermHandler);
#endif
	}

	void setThreadExceptionHandlers()
	{
#if 0
		// Catch a floating point error
		typedef void(*sigh)(int);
		signal(SIGFPE, (sigh)sigfpeHandler);

		// Catch an illegal instruction
		signal(SIGILL, sigillHandler);

		// Catch illegal storage access errors
		signal(SIGSEGV, sigsegvHandler);
#endif
	}
}  // namespace TFE_CrashHandler

// CRT SIGABRT signal handler
void sigabrtHandler(int)
{
	fprintf(stderr, "Caught SIGBABRT");
	exit(-4);
}

// CRT SIGFPE signal handler
void sigfpeHandler(int /*code*/, int subcode)
{
	fprintf(stderr, "Caught SIGFPE");
	exit(-5);
}

// CRT sigill signal handler
void sigillHandler(int)
{
	fprintf(stderr, "Caught SIGILL");
	exit(-6);
}

// CRT sigint signal handler
void sigintHandler(int)
{
	fprintf(stderr, "Caught SIGINT");
	exit(-7);
}

// CRT SIGSEGV signal handler
void sigsegvHandler(int)
{
	fprintf(stderr, "Caught SIGSEGV");
	exit(-8);
}

// CRT SIGTERM signal handler
void sigtermHandler(int)
{
	fprintf(stderr, "Caught SIGTERM");
	exit(-9);
}
