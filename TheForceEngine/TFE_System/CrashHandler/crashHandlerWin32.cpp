#include "crashHandler.h"
#include <TFE_System/system.h>

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <new.h>
#include <signal.h>
#include <exception>
#include <sys/stat.h>
#include <psapi.h>
#include <rtcapi.h>
#include <Shellapi.h>
#include <dbghelp.h>

#ifndef _AddressOfReturnAddress

// Taken from: http://msdn.microsoft.com/en-us/library/s975zw7k(VS.71).aspx
#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

// _ReturnAddress and _AddressOfReturnAddress should be prototyped before use 
EXTERNC void * _AddressOfReturnAddress(void);
EXTERNC void * _ReturnAddress(void);

#endif 

// Collects current process state.
static void getExceptionPointers(u32 dwExceptionCode, EXCEPTION_POINTERS** pExceptionPointers);

// This method creates minidump of the process
static void createMiniDump(EXCEPTION_POINTERS* pExcPtrs);

// Exception handler functions
static LONG WINAPI sehHandler(PEXCEPTION_POINTERS pExceptionPtrs);
static void terminateHandler();
static void unexpectedHandler();
static void pureCallHandler();
static void invalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, u32 line, uintptr_t reserved);
static int  newHandler(size_t);

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
		// Install top-level SEH handler
		SetUnhandledExceptionFilter(sehHandler);

		// Catch pure virtual function calls.
		// Because there is one _purecall_handler for the whole process, 
		// calling this function immediately impacts all threads. The last 
		// caller on any thread sets the handler. 
		// http://msdn.microsoft.com/en-us/library/t296ys27.aspx
		_set_purecall_handler(pureCallHandler);

		// Catch new operator memory allocation exceptions
		_set_new_handler(newHandler);

		// Catch invalid parameter exceptions.
		_set_invalid_parameter_handler(invalidParameterHandler);

		// Set up C++ signal handlers
		_set_abort_behavior(_CALL_REPORTFAULT, _CALL_REPORTFAULT);

		// Catch an abnormal program termination
		signal(SIGABRT, sigabrtHandler);

		// Catch illegal instruction handler
		signal(SIGINT, sigintHandler);

		// Catch a termination request
		signal(SIGTERM, sigtermHandler);
	}

	void setThreadExceptionHandlers()
	{
		// Catch terminate() calls. 
		// In a multithreaded environment, terminate functions are maintained 
		// separately for each thread. Each new thread needs to install its own 
		// terminate function. Thus, each thread is in charge of its own termination handling.
		// http://msdn.microsoft.com/en-us/library/t6fk7h29.aspx
		set_terminate(terminateHandler);

		// Catch unexpected() calls.
		// In a multithreaded environment, unexpected functions are maintained 
		// separately for each thread. Each new thread needs to install its own 
		// unexpected function. Thus, each thread is in charge of its own unexpected handling.
		// http://msdn.microsoft.com/en-us/library/h46t5b69.aspx  
		set_unexpected(unexpectedHandler);

		// Catch a floating point error
		typedef void(*sigh)(int);
		signal(SIGFPE, (sigh)sigfpeHandler);

		// Catch an illegal instruction
		signal(SIGILL, sigillHandler);

		// Catch illegal storage access errors
		signal(SIGSEGV, sigsegvHandler);
	}
}  // namespace TFE_CrashHandler

// The following code gets exception pointers using a workaround found in CRT code.
void getExceptionPointers(u32 dwExceptionCode, EXCEPTION_POINTERS** ppExceptionPointers)
{
	// The following code was taken from VC++ 8.0 CRT (invarg.c: line 104)
	EXCEPTION_RECORD ExceptionRecord;
	CONTEXT ContextRecord;
	memset(&ContextRecord, 0, sizeof(CONTEXT));

#ifdef _X86_
	__asm {
		mov dword ptr [ContextRecord.Eax], eax
			mov dword ptr [ContextRecord.Ecx], ecx
			mov dword ptr [ContextRecord.Edx], edx
			mov dword ptr [ContextRecord.Ebx], ebx
			mov dword ptr [ContextRecord.Esi], esi
			mov dword ptr [ContextRecord.Edi], edi
			mov word ptr [ContextRecord.SegSs], ss
			mov word ptr [ContextRecord.SegCs], cs
			mov word ptr [ContextRecord.SegDs], ds
			mov word ptr [ContextRecord.SegEs], es
			mov word ptr [ContextRecord.SegFs], fs
			mov word ptr [ContextRecord.SegGs], gs
			pushfd
			pop [ContextRecord.EFlags]
	}

	ContextRecord.ContextFlags = CONTEXT_CONTROL;
#pragma warning(push)
#pragma warning(disable:4311)
	ContextRecord.Eip = (ULONG)_ReturnAddress();
	ContextRecord.Esp = (ULONG)_AddressOfReturnAddress();
#pragma warning(pop)
	ContextRecord.Ebp = *((ULONG *)_AddressOfReturnAddress()-1);
#elif defined (_IA64_) || defined (_AMD64_)
	// Need to fill up the Context in IA64 and AMD64.
	RtlCaptureContext(&ContextRecord);
#else  // defined (_IA64_) || defined (_AMD64_)
	ZeroMemory(&ContextRecord, sizeof(ContextRecord));
#endif  // defined (_IA64_) || defined (_AMD64_)

	ZeroMemory(&ExceptionRecord, sizeof(EXCEPTION_RECORD));
	ExceptionRecord.ExceptionCode = dwExceptionCode;
	ExceptionRecord.ExceptionAddress = _ReturnAddress();

	EXCEPTION_RECORD* pExceptionRecord = new EXCEPTION_RECORD;
	memcpy(pExceptionRecord, &ExceptionRecord, sizeof(EXCEPTION_RECORD));
	CONTEXT* pContextRecord = new CONTEXT;
	memcpy(pContextRecord, &ContextRecord, sizeof(CONTEXT));

	*ppExceptionPointers = new EXCEPTION_POINTERS;
	(*ppExceptionPointers)->ExceptionRecord = pExceptionRecord;
	(*ppExceptionPointers)->ContextRecord = pContextRecord;  
}

// This method creates minidump of the process
void createMiniDump(EXCEPTION_POINTERS* pExcPtrs)
{   
    HMODULE hDbgHelp = NULL;
    HANDLE hFile = NULL;
    MINIDUMP_EXCEPTION_INFORMATION mei;
    MINIDUMP_CALLBACK_INFORMATION mci;
    
    // Load dbghelp.dll
    hDbgHelp = LoadLibrary(_T("dbghelp.dll"));
    if (hDbgHelp == NULL)
    {
        // Error - couldn't load dbghelp.dll
		return;
    }

    // Create the minidump file
    hFile = CreateFile(_T("crashdump.dmp"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        // Couldn't create file
		return;
    }
   
    // Write minidump to the file
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = pExcPtrs;
    mei.ClientPointers = FALSE;
    mci.CallbackRoutine = NULL;
    mci.CallbackParam = NULL;

    typedef BOOL (WINAPI *LPMINIDUMPWRITEDUMP)(
        HANDLE hProcess, 
        DWORD ProcessId, 
        HANDLE hFile, 
        MINIDUMP_TYPE DumpType, 
        CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, 
        CONST PMINIDUMP_USER_STREAM_INFORMATION UserEncoderParam, 
        CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

    LPMINIDUMPWRITEDUMP pfnMiniDumpWriteDump = (LPMINIDUMPWRITEDUMP)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
    if(!pfnMiniDumpWriteDump)
    {    
        // Bad MiniDumpWriteDump function
        return;
    }

    HANDLE hProcess = GetCurrentProcess();
	DWORD dwProcessId = GetCurrentProcessId();
    if(!pfnMiniDumpWriteDump(hProcess, dwProcessId, hFile, MiniDumpNormal, &mei, NULL, &mci))
    {    
        // Error writing dump.
        return;
    }

    // Close file
    CloseHandle(hFile);

    // Unload dbghelp.dll
    FreeLibrary(hDbgHelp);
}

// Structured exception handler
LONG WINAPI sehHandler(PEXCEPTION_POINTERS pExceptionPtrs)
{ 
	// Write minidump file
	createMiniDump(pExceptionPtrs);

	// Terminate process
	TerminateProcess(GetCurrentProcess(), 1);

	// Attempt to write to the log...
	TFE_System::logWrite(LOG_ERROR, "Crash", "TFE Crashed - caught using SEH. A crash dump has been written to the TFE directory.");
	TFE_System::logClose();
	
	// Unreacheable code  
	return EXCEPTION_EXECUTE_HANDLER;
}

// CRT terminate() call handler
void terminateHandler()
{
	// Abnormal program termination (terminate() function was called)

	// Retrieve exception information
	EXCEPTION_POINTERS* pExceptionPtrs = NULL;
	getExceptionPointers(0, &pExceptionPtrs);

	// Write minidump file
	createMiniDump(pExceptionPtrs);

	// Attempt to write to the log...
	TFE_System::logWrite(LOG_ERROR, "Crash", "TFE Crashed - Abnormal Program Termination. A crash dump has been written to the TFE directory.");
	TFE_System::logClose();

	// Terminate process
	TerminateProcess(GetCurrentProcess(), 1);    
}

// CRT unexpected() call handler
void unexpectedHandler()
{
	// Unexpected error (unexpected() function was called)

	// Retrieve exception information
	EXCEPTION_POINTERS* pExceptionPtrs = NULL;
	getExceptionPointers(0, &pExceptionPtrs);

	// Write minidump file
	createMiniDump(pExceptionPtrs);

	// Attempt to write to the log...
	TFE_System::logWrite(LOG_ERROR, "Crash", "TFE Crashed - Unexpected Call Handler. A crash dump has been written to the TFE directory.");
	TFE_System::logClose();

	// Terminate process
	TerminateProcess(GetCurrentProcess(), 1);    	    
}

// CRT Pure virtual method call handler
void pureCallHandler()
{
	// Pure virtual function call

	// Retrieve exception information
	EXCEPTION_POINTERS* pExceptionPtrs = NULL;
	getExceptionPointers(0, &pExceptionPtrs);

	// Write minidump file
	createMiniDump(pExceptionPtrs);

	// Attempt to write to the log...
	TFE_System::logWrite(LOG_ERROR, "Crash", "TFE Crashed - Pure Call Handler. A crash dump has been written to the TFE directory.");
	TFE_System::logClose();

	// Terminate process
	TerminateProcess(GetCurrentProcess(), 1);    
	 
}

// CRT invalid parameter handler
void invalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, u32 line, uintptr_t reserved)
{
	reserved;
	// Invalid parameter exception

	// Retrieve exception information
	EXCEPTION_POINTERS* pExceptionPtrs = NULL;
	getExceptionPointers(0, &pExceptionPtrs);

	// Write minidump file
	createMiniDump(pExceptionPtrs);

	// Attempt to write to the log...
	TFE_System::logWrite(LOG_ERROR, "Crash", "TFE Crashed - Invalid System Call. A crash dump has been written to the TFE directory.");
	TFE_System::logClose();

	// Terminate process
	TerminateProcess(GetCurrentProcess(), 1);    
}

// CRT new operator fault handler
int newHandler(size_t)
{
	// 'new' operator memory allocation exception

	// Retrieve exception information
	EXCEPTION_POINTERS* pExceptionPtrs = NULL;
	getExceptionPointers(0, &pExceptionPtrs);

	// Write minidump file
	createMiniDump(pExceptionPtrs);

	// Attempt to write to the log...
	TFE_System::logWrite(LOG_ERROR, "Crash", "TFE Crashed - Memory Allocation Error. A crash dump has been written to the TFE directory.");
	TFE_System::logClose();

	// Terminate process
	TerminateProcess(GetCurrentProcess(), 1);
	
	// Unreacheable code
	return 0;
}

// CRT SIGABRT signal handler
void sigabrtHandler(int)
{
	// Caught SIGABRT C++ signal

	// Retrieve exception information
	EXCEPTION_POINTERS* pExceptionPtrs = NULL;
	getExceptionPointers(0, &pExceptionPtrs);

	// Write minidump file
	createMiniDump(pExceptionPtrs);

	// Attempt to write to the log...
	TFE_System::logWrite(LOG_ERROR, "Crash", "TFE Crashed - SIGABRT. A crash dump has been written to the TFE directory.");
	TFE_System::logClose();

	// Terminate process
	TerminateProcess(GetCurrentProcess(), 1);   
}

// CRT SIGFPE signal handler
void sigfpeHandler(int /*code*/, int subcode)
{
	// Floating point exception (SIGFPE)

	EXCEPTION_POINTERS* pExceptionPtrs = (PEXCEPTION_POINTERS)_pxcptinfoptrs;
	
	// Write minidump file
	createMiniDump(pExceptionPtrs);

	// Attempt to write to the log...
	TFE_System::logWrite(LOG_ERROR, "Crash", "TFE Crashed - SIGFPE. A crash dump has been written to the TFE directory.");
	TFE_System::logClose();

	// Terminate process
	TerminateProcess(GetCurrentProcess(), 1);    
}

// CRT sigill signal handler
void sigillHandler(int)
{
	// Illegal instruction (SIGILL)

	// Retrieve exception information
	EXCEPTION_POINTERS* pExceptionPtrs = NULL;
	getExceptionPointers(0, &pExceptionPtrs);

	// Write minidump file
	createMiniDump(pExceptionPtrs);

	// Attempt to write to the log...
	TFE_System::logWrite(LOG_ERROR, "Crash", "TFE Crashed - SIGILL. A crash dump has been written to the TFE directory.");
	TFE_System::logClose();

	// Terminate process
	TerminateProcess(GetCurrentProcess(), 1);    
}

// CRT sigint signal handler
void sigintHandler(int)
{
	// Interruption (SIGINT)

	// Retrieve exception information
	EXCEPTION_POINTERS* pExceptionPtrs = NULL;
	getExceptionPointers(0, &pExceptionPtrs);

	// Write minidump file
	createMiniDump(pExceptionPtrs);

	// Attempt to write to the log...
	TFE_System::logWrite(LOG_ERROR, "Crash", "TFE Crashed - SIGINT. A crash dump has been written to the TFE directory.");
	TFE_System::logClose();

	// Terminate process
	TerminateProcess(GetCurrentProcess(), 1);    
}

// CRT SIGSEGV signal handler
void sigsegvHandler(int)
{
	// Invalid storage access (SIGSEGV)

	PEXCEPTION_POINTERS pExceptionPtrs = (PEXCEPTION_POINTERS)_pxcptinfoptrs;

	// Write minidump file
	createMiniDump(pExceptionPtrs);

	// Attempt to write to the log...
	TFE_System::logWrite(LOG_ERROR, "Crash", "TFE Crashed - SIGSEGV. A crash dump has been written to the TFE directory.");
	TFE_System::logClose();

	// Terminate process
	TerminateProcess(GetCurrentProcess(), 1);    
}

// CRT SIGTERM signal handler
void sigtermHandler(int)
{
	// Termination request (SIGTERM)

	// Retrieve exception information
	EXCEPTION_POINTERS* pExceptionPtrs = NULL;
	getExceptionPointers(0, &pExceptionPtrs);

	// Write minidump file
	createMiniDump(pExceptionPtrs);

	// Attempt to write to the log...
	TFE_System::logWrite(LOG_ERROR, "Crash", "TFE Crashed - SIGTERM. A crash dump has been written to the TFE directory.");
	TFE_System::logClose();

	// Terminate process
	TerminateProcess(GetCurrentProcess(), 1);    
}


